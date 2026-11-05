#!/bin/sh
# Disposable container build. /work is read-only; /out contains only this run's artifacts.
set -eu
export LC_ALL=C

apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates curl gcc make bc bison flex libssl-dev libelf-dev \
    cpio qemu-system-x86 patch xz-utils

source_url=https://launchpad.net/ubuntu/+archive/primary/+sourcefiles/linux/6.8.0-139.139
for name in linux_6.8.0.orig.tar.gz linux_6.8.0-139.139.diff.gz; do
    if [ ! -f "/out/$name" ]; then
        curl --fail --location --retry 3 "$source_url/$name" -o "/out/$name"
    fi
done
(cd /out && printf '%s  %s\n' \
    26512115972bdf017a4ac826cc7d3e9b0ba397d4f85cd330e4e4ff54c78061c8 linux_6.8.0.orig.tar.gz \
    c18cf1db6c7ed0ad5cf7d651de117b415f17e05baaeefb13a4a2306e94e9ae57 linux_6.8.0-139.139.diff.gz \
    | sha256sum -c -)

mkdir -p /tmp/pbuf-build /tmp/pbuf-root/dev
tar -xzf /out/linux_6.8.0.orig.tar.gz -C /tmp/pbuf-build --strip-components=1
cd /tmp/pbuf-build
gzip -dc /out/linux_6.8.0-139.139.diff.gz | patch -p1 --batch > /out/ubuntu-source-apply.log
printf '%s  %s\n' \
    3be9ff5bd164bec9340c1a162995fc91f41103e7130d641a1e561ad2956c8c07 io_uring/kbuf.c \
    | sha256sum -c -

make tinyconfig > /out/configure.log
scripts/config --enable 64BIT --enable PRINTK --enable MULTIUSER \
    --enable TTY --enable SERIAL_8250 --enable SERIAL_8250_CONSOLE \
    --enable BINFMT_ELF --enable BLK_DEV_INITRD --enable RD_GZIP \
    --enable IO_URING --enable EPOLL --enable EVENTFD --enable FUTEX \
    --enable SHMEM --enable POSIX_TIMERS --enable SYSCTL \
    --set-str LOCALVERSION '-pbuf-check' --disable LOCALVERSION_AUTO
make olddefconfig >> /out/configure.log
grep -q '^CONFIG_64BIT=y$' .config
grep -q '^CONFIG_IO_URING=y$' .config
grep -q '^CONFIG_SERIAL_8250_CONSOLE=y$' .config
cp .config /out/kernel.config
cc --version > /out/compiler.txt

cc -std=c11 -O2 -static -Wall -Wextra -Werror -Wpedantic /work/pbuf_reserved.c -o /tmp/pbuf-root/pbuf_reserved
cc -std=c11 -O2 -static -Wall -Wextra -Werror -Wpedantic /work/vm_init.c -o /tmp/pbuf-root/init
mknod /tmp/pbuf-root/dev/console c 5 1
(cd /tmp/pbuf-root && find . -print0 | cpio --null -o --format=newc | gzip -9) > /out/initramfs.cpio.gz

echo 'Building unpatched Ubuntu source with the minimal QEMU configuration'
make -j8 bzImage > /out/build-unpatched.log
cp arch/x86/boot/bzImage /out/bzImage-unpatched
sha256sum io_uring/kbuf.c arch/x86/boot/bzImage .config > /out/hashes-unpatched.txt
qemu-system-x86_64 -accel tcg -cpu max -smp 1 -m 512 \
    -nodefaults -display none -serial stdio -no-reboot \
    -kernel /out/bzImage-unpatched -initrd /out/initramfs.cpio.gz \
    -append 'console=ttyS0 rdinit=/init panic=-1' > /out/unpatched-console.txt
grep -E '^# kernel=|^# tests=|^VM_TEST_EXIT=' /out/unpatched-console.txt
grep -q 'tests=17 failures=17 diagnostic_bootstraps=13' /out/unpatched-console.txt
grep -q 'VM_TEST_EXIT=1' /out/unpatched-console.txt

patch -p1 --batch < /work/0001-pbuf-reserved-checks.patch > /out/fix-apply.log
echo 'Building corrected kernel with the identical configuration'
make -j8 bzImage > /out/build-corrected.log
cp arch/x86/boot/bzImage /out/bzImage-corrected
sha256sum io_uring/kbuf.c arch/x86/boot/bzImage .config > /out/hashes-corrected.txt
cmp .config /out/kernel.config
qemu-system-x86_64 -accel tcg -cpu max -smp 1 -m 512 \
    -nodefaults -display none -serial stdio -no-reboot \
    -kernel /out/bzImage-corrected -initrd /out/initramfs.cpio.gz \
    -append 'console=ttyS0 rdinit=/init panic=-1' > /out/corrected-console.txt
grep -E '^# kernel=|^# tests=|^VM_TEST_EXIT=' /out/corrected-console.txt
grep -q 'tests=17 failures=0 diagnostic_bootstraps=0' /out/corrected-console.txt
grep -q 'VM_TEST_EXIT=0' /out/corrected-console.txt
echo 'Unpatched/corrected kernel runtime verification passed'
