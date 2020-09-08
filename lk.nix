{ stdenv, project }:

stdenv.mkDerivation {
  name = "littlekernel-${project}";
  src = stdenv.lib.cleanSource ./.;
  makeFlags = [ "PROJECT=${project}" ];
  hardeningDisable = [ "format" ];
  installPhase = ''
    mkdir -p $out/nix-support
    cp -r build-${project}/{config.h,lk.*} $out
    cat <<EOF > $out/nix-support/hydra-metrics
    lk.bin $(stat --printf=%s $out/lk.bin) bytes
    lk.elf $(stat --printf=%s $out/lk.elf) bytes
    EOF
  '';
}
