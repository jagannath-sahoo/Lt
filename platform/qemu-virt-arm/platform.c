/*
 * Copyright (c) 2012-2015 Travis Geiselbrecht
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <arch.h>
#include <lk/err.h>
#include <lk/debug.h>
#include <lk/trace.h>
#include <dev/interrupt/arm_gic.h>
#include <dev/timer/arm_generic.h>
#include <dev/uart.h>
#include <dev/virtio.h>
#include <dev/virtio/net.h>
#include <lib/fdtwalk.h>
#include <lk/init.h>
#include <kernel/vm.h>
#include <kernel/spinlock.h>
#include <platform.h>
#include <platform/gic.h>
#include <platform/interrupts.h>
#include <platform/qemu-virt.h>
#include "platform_p.h"

#if WITH_LIB_MINIP
#include <lib/minip.h>
#endif

#define LOCAL_TRACE 0

#define DEFAULT_MEMORY_SIZE (MEMSIZE) /* try to fetch from the emulator via the fdt */

/* initial memory mappings. parsed by start.S */
struct mmu_initial_mapping mmu_initial_mappings[] = {
    /* All of the memory */
    {
        .phys = MEMORY_BASE_PHYS,
        .virt = KERNEL_BASE,
        .size = MEMORY_APERTURE_SIZE,
        .flags = 0,
        .name = "memory"
    },
    /* 1GB of peripherals */
    {
        .phys = PERIPHERAL_BASE_PHYS,
        .virt = PERIPHERAL_BASE_VIRT,
        .size = PERIPHERAL_BASE_SIZE,
        .flags = MMU_INITIAL_MAPPING_FLAG_DEVICE,
        .name = "peripherals"
    },

    /* null entry to terminate the list */
    { 0 }
};

static pmm_arena_t mapped_arena = {
    .name = "ram",
    .base = MEMORY_BASE_PHYS,
    .size = DEFAULT_MEMORY_SIZE,
    .flags = PMM_ARENA_FLAG_KMAP,
};

static pmm_arena_t unmapped_arena = {
    .name = "unmapped_ram",
    .base = MEMORY_BASE_PHYS + DEFAULT_MEMORY_SIZE,
    .size = UNMAPPED_MEMSIZE,
    .flags = PMM_ARENA_FLAG_UNAMPPED,
};

extern void psci_call(ulong arg0, ulong arg1, ulong arg2, ulong arg3);

// callbacks to the fdt_walk routine
static void memcallback(uint64_t base, uint64_t len, void *cookie) {
    bool *found_mem = (bool *)cookie;

    LTRACEF("base %#llx len %#llx cookie %p\n", base, len, cookie);

    /* add another novm arena */
    if (!*found_mem) {
        printf("FDT: found memory arena, base %#llx size %#llx\n", base, len);

        /* trim size on certain platforms */
#if ARCH_ARM
        if (len > 1024*1024*1024U) {
            len = 1024*1024*1024; /* only use the first 1GB on ARM32 */
            printf("trimming memory to 1GB\n");
        }
#endif

        /* set the size in the pmm arena */
        /* TODO: Check whether unmapped memsize can fit,
         * probably new fdt entry can be added */
        mapped_arena.size = (len - UNMAPPED_MEMSIZE);
        unmapped_arena.base = (MEMORY_BASE_PHYS + mapped_arena.size);

        *found_mem = true; // stop searching after the first one
    }
}

static void cpucallback(uint64_t id, void *cookie) {
    int *cpu_count = (int *)cookie;

    LTRACEF("id %#llx cookie %p\n", id, cookie);

    (*cpu_count)++;
}

void platform_early_init(void) {
    /* initialize the interrupt controller */
    arm_gic_init();

    arm_generic_timer_init(ARM_GENERIC_TIMER_PHYSICAL_INT, 0);

    uart_init_early();

    int cpu_count = 0;
    bool found_mem = false;
    struct fdt_walk_callbacks cb = {
        .mem = memcallback,
        .memcookie = &found_mem,
        .cpu = cpucallback,
        .cpucookie = &cpu_count,
    };

    const void *fdt = (void *)KERNEL_BASE;
    status_t err = fdt_walk(fdt, &cb);
    LTRACEF("fdt_walk returns %d\n", err);

    if (err != 0) {
        printf("FDT: error finding FDT at %p, using default memory & cpu count\n", fdt);
    }

    /* add the main memory arena */
    pmm_add_arena(&mapped_arena);
    pmm_add_arena(&unmapped_arena);

    /* reserve the first 64k of ram, which should be holding the fdt */
    struct list_node list = LIST_INITIAL_VALUE(list);
    pmm_alloc_range(MEMBASE, 0x10000 / PAGE_SIZE, &list);

    /* count the number of secondary cpus */
    if (cpu_count == 0) {
        /* if we didn't find any in the FDT, assume max number */
        cpu_count = SMP_MAX_CPUS;
    } else if (cpu_count > 0) {
        printf("FDT: found %d cpus\n", cpu_count);
        cpu_count = MIN(cpu_count, SMP_MAX_CPUS);
    }

    LTRACEF("booting %d cpus\n", cpu_count);

    /* boot the secondary cpus using the Power State Coordintion Interface */
    ulong psci_call_num = 0x84000000 + 3; /* SMC32 CPU_ON */
#if ARCH_ARM64
    psci_call_num += 0x40000000; /* SMC64 */
#endif
    for (int i = 1; i < cpu_count; i++) {
        psci_call(psci_call_num, i, MEMBASE + KERNEL_LOAD_OFFSET, 0);
    }
}

void platform_init(void) {
    /* TODO: Take care of any vmm_alloc* invocations before this point */
    /* Unmap the memory mapped initially for unmapped ram arena */
    arch_clean_invalidate_cache_range(paddr_to_kvaddr(unmapped_arena.base),
        unmapped_arena.size);
    arch_mmu_unmap(&vmm_get_kernel_aspace()->arch_aspace,
        paddr_to_kvaddr(unmapped_arena.base), unmapped_arena.size / PAGE_SIZE);

    uart_init();

    /* detect any virtio devices */
    uint virtio_irqs[NUM_VIRTIO_TRANSPORTS];
    for (int i = 0; i < NUM_VIRTIO_TRANSPORTS; i++) {
        virtio_irqs[i] = VIRTIO0_INT + i;
    }

    virtio_mmio_detect((void *)VIRTIO_BASE, NUM_VIRTIO_TRANSPORTS, virtio_irqs, 0x200);

#if WITH_LIB_MINIP
    if (virtio_net_found() > 0) {
        uint8_t mac_addr[6];

        virtio_net_get_mac_addr(mac_addr);

        TRACEF("found virtio networking interface\n");

        /* start minip */
        minip_set_macaddr(mac_addr);

        __UNUSED uint32_t ip_addr = IPV4(192, 168, 0, 99);
        __UNUSED uint32_t ip_mask = IPV4(255, 255, 255, 0);
        __UNUSED uint32_t ip_gateway = IPV4_NONE;

        //minip_init(virtio_net_send_minip_pkt, NULL, ip_addr, ip_mask, ip_gateway);
        minip_init_dhcp(virtio_net_send_minip_pkt, NULL);

        virtio_net_start();
    }
#endif
}
