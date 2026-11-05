# Ubuntu 6.8 io_uring buffer-ring regression

Reproducer and proposed fix for [LP #2162843](https://bugs.launchpad.net/bugs/2162843),
investigated while developing a C++23 entry for the
[Twelve Dollar Server Challenge](https://github.com/arjaythedev/twelve-dollar-server-challenge).
Other entrants already work around this bug: [kensac's Rust submission](https://github.com/arjaythedev/twelve-dollar-server-challenge/blob/main/submissions/rust-iouring-kensac/src/main.rs#L62-L87)
and [pavlyhalim's C submission](https://github.com/arjaythedev/twelve-dollar-server-challenge/blob/main/submissions/c-iouring-pavlyhalim/src/sys.h)
retry failed registration with nonzero reserved fields.

The Ubuntu backport uses `!memchr_inv(..., 0, ...)` where it needs
`memchr_inv(..., 0, ...)`: valid zero-filled reserved fields are rejected,
and nonzero reserved fields are accepted. The proposed patch removes the
negation in registration, unregistration and status queries in `io_uring/kbuf.c`.

Source checks found correct validation in `6.8.0-134.134` and inverted checks
in `136`, `139`, `147` and `149`. The checked Noble `master-next` commit
[`6dcf89bf9d16`](https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/noble/commit/?id=6dcf89bf9d1650d117680189bf4bbb596af7ab84)
still contains the inversions.

## Reproduce

Requires Linux 6.8+ UAPI headers and a C compiler; no liburing dependency.

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic pbuf_reserved.c -o pbuf_reserved
./pbuf_reserved
```

The 6.8-header build runs 17 checks: valid requests and each reserved word set
independently. On affected kernels, diagnostic registration with nonzero reserved
bits lets it reach the unregistration and status checks. Exit codes: 0 = pass,
1 = regression detected, 2 = prerequisite failure, 4 = `io_uring_setup()` failed.
Newer headers may produce a different check count.

## Validation

| Kernel | Result |
| --- | --- |
| Ubuntu `139` source, minimal QEMU config, unpatched | [17/17 failed](results/unpatched.log) |
| Same source and config, with the patch | [17/17 passed](results/corrected.log) |

The custom builds used identical [configuration](results/kernel.config)
and initramfs. Logs contain the test section of the QEMU serial output.
The technical diff passed `checkpatch --no-tree --no-signoff`, with 0 errors
and 0 warnings. The unmodified minimal build warns about an unused variable in
`init/do_mounts.c:464`. Full Ubuntu `generic` builds and application benchmarks
have not been tested.

To rebuild both kernels in disposable QEMU guests (Docker, Linux shell):

```sh
mkdir -p .work/build
docker run --rm --cpus=8 --memory=10g \
  -v "$PWD:/work:ro" -v "$PWD/.work/build:/out" \
  ubuntu:24.04 sh /work/build-and-test.sh
```

The script downloads the recorded Ubuntu sources into `.work/`, verifies their
SHA256 hashes and builds both variants. Kernel sources and binaries are not
needed in the repository.

## Before submission

- LP #2162843 records a verified `linux-bluefield` fix. Its commit has not been
  identified; ask for it before submitting a duplicate correction.
- [Launchpad follow-up](launchpad-follow-up.md) and
  [Ubuntu SRU cover letter](0000-cover-letter.txt) remain unsent drafts.
