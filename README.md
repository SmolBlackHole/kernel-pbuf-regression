# Ubuntu 6.8 io_uring buffer-ring regression

I ran into this while working on a C++23 entry for the
[Twelve Dollar Server Challenge](https://github.com/arjaythedev/twelve-dollar-server-challenge).
It's already reported as [LP #2162843](https://bugs.launchpad.net/bugs/2162843).
[kensac's Rust entry](https://github.com/arjaythedev/twelve-dollar-server-challenge/blob/main/submissions/rust-iouring-kensac/src/main.rs#L62-L87)
and [pavlyhalim's C entry](https://github.com/arjaythedev/twelve-dollar-server-challenge/blob/main/submissions/c-iouring-pavlyhalim/src/sys.h)
already work around it by retrying registration with nonzero reserved fields.

The Ubuntu backport kept the `!` when replacing `mem_is_zero()` with
`memchr_inv(..., 0, ...)`. That flips the check: zero-filled reserved fields
are rejected, and nonzero ones are accepted. The patch removes three `!`
operators in `io_uring/kbuf.c`, covering registration, unregistration and status.

I checked the sources for `6.8.0-134.134`, `136`, `139`, `147` and `149`.
`134` is correct; the other four have the bug. The checked Noble
[`master-next`](https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/noble/commit/?id=6dcf89bf9d1650d117680189bf4bbb596af7ab84)
still has it.

[Bluefield `6.8.0-1035.40`](https://git.launchpad.net/~canonical-kernel-bluefield/ubuntu/+source/linux-bluefield/+git/noble/plain/io_uring/kbuf.c?h=Ubuntu-bluefield-6.8.0-1035.40)
already has the same correction. Its `kbuf.c` matches the file this patch
produces. The [Bluefield log](https://git.launchpad.net/~canonical-kernel-bluefield/ubuntu/+source/linux-bluefield/+git/noble/log/?h=master-next)
lists the existing fix as `1473fb4e7a3198b834878dce273d2311738a353a`,
by Uri Dar.

## Run the reproducer

Requires Linux 6.8+ UAPI headers and a C compiler. No liburing needed.

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic pbuf_reserved.c -o pbuf_reserved
./pbuf_reserved
```

With 6.8 headers, it runs 17 checks: valid requests and each reserved word set
separately. To reach unregistration and status on broken kernels, it first
registers a ring with nonzero reserved bits. That's only test setup; the valid
registration failure is still reported.

Exit codes: 0 = pass, 1 = regression detected, 2 = prerequisite failure,
4 = `io_uring_setup()` failed. Newer headers may change the check count.

## What I tested

| Kernel | Result |
| --- | --- |
| Ubuntu `139` source, minimal QEMU config, unpatched | [17/17 failed](results/unpatched.log) |
| Same source and config, with the patch | [17/17 passed](results/corrected.log) |

Both builds used the same [config](results/kernel.config) and initramfs.
The logs contain the test section of the QEMU serial output.
The raw diff passed `checkpatch --no-tree --no-signoff`: 0 errors, 0 warnings.
Checking the full Ubuntu mail also flags `BugLink:`, a tag Ubuntu requires.
The unmodified minimal build warns about an unused variable in
`init/do_mounts.c:464`. I haven't tested the full Ubuntu `generic` build
or run application benchmarks with the patch.

To rebuild both kernels in QEMU (Docker, Linux shell):

```sh
mkdir -p .work/build
docker run --rm --cpus=8 --memory=10g \
  -v "$PWD:/work:ro" -v "$PWD/.work/build:/out" \
  ubuntu:24.04 sh /work/build-and-test.sh
```

The script downloads the Ubuntu sources into `.work/`, checks their SHA256
hashes and builds both versions. Downloads and binaries stay out of Git.

I submitted the reproducer, test logs and config in
[Launchpad comment #21](https://bugs.launchpad.net/ubuntu/+source/linux-bluefield/+bug/2162843/comments/21),
asking for the existing Bluefield fix to be carried to Noble `generic`.
The [follow-up notes](launchpad-follow-up.md),
[Ubuntu SRU cover letter](0000-cover-letter.txt) and
[comparison patch](0001-pbuf-reserved-checks.patch) document the investigation.
The cover letter and patch were not submitted separately.
