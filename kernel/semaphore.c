#include <debug.h>
#include <err.h>
#include <kernel/semaphore.h>
#include <kernel/thread.h>

void sem_init(semaphore_t *sem, unsigned int value)
{
	sem->magic = SEMAPHORE_MAGIC;
	sem->count = value;
	wait_queue_init(&sem->wait);
}

void sem_destroy(semaphore_t *sem)
{
	enter_critical_section();
	sem->count = 0;
	wait_queue_destroy(&sem->wait, true);
	exit_critical_section();
}

status_t sem_post(semaphore_t *sem)
{
	status_t ret = NO_ERROR;
	enter_critical_section();

	/*
	 * If the count is or was negative then a thread is waiting for a resource, otherwise
	 * it's safe to just increase the count available with no downsides
	 */
	if (++sem->count <= 0)
		wait_queue_wake_one(&sem->wait, true, NO_ERROR);

	exit_critical_section();
	return ret;
}

status_t sem_wait(semaphore_t *sem)
{
	status_t ret = NO_ERROR;
	enter_critical_section();

	/* 
	 * If there are no resources available then we need to 
	 * sit in the wait queue until sem_post adds some. 
	 */
	if (--sem->count < 0)
		ret = wait_queue_block(&sem->wait, INFINITE_TIME);

	exit_critical_section();
	return ret;
}

status_t sem_trywait(semaphore_t *sem)
{
	status_t ret = NO_ERROR;
	enter_critical_section();

	if (sem->count <= 0)
		ret = ERR_NOT_READY;
	else
		sem->count--;
	
	exit_critical_section();
	return ret;
}

status_t sem_timedwait(semaphore_t *sem, lk_time_t timeout)
{
	status_t ret = NO_ERROR;
	enter_critical_section();

	if (--sem->count < 0) {
		ret = wait_queue_block(&sem->wait, timeout);
		if (ret < NO_ERROR) {
			if (ret == ERR_TIMED_OUT) {
				sem->count++;
			}
		}
	}

	exit_critical_section();
	return ret;
}
