# Follow-up notes for LP #2162843

Results submitted in [comment #21](https://bugs.launchpad.net/ubuntu/+source/linux-bluefield/+bug/2162843/comments/21).

I ran into this while working on a C++23 entry for the Twelve Dollar Server
Challenge. Some entries already retry registration with nonzero reserved
fields: [kensac's Rust entry](https://github.com/arjaythedev/twelve-dollar-server-challenge/blob/main/submissions/rust-iouring-kensac/src/main.rs#L62-L87)
and [pavlyhalim's C entry](https://github.com/arjaythedev/twelve-dollar-server-challenge/blob/main/submissions/c-iouring-pavlyhalim/src/sys.h).
Both reference this bug.

I checked all three reserved-field checks in `io_uring/kbuf.c`:
`io_register_pbuf_ring()`, `io_unregister_pbuf_ring()` and
`io_register_pbuf_status()`. They use `!memchr_inv(..., 0, ...)` where they
need `memchr_inv(..., 0, ...)`. Valid zero-filled fields are rejected;
nonzero ones are accepted. The upstream `172484907285` change is correct.
The inversion is in Ubuntu's adaptation.

Noble packages `136`, `139`, `147` and `149` have identical affected
`kbuf.c` files; `134` has the correct checks. Noble master-next
`6dcf89bf9d1650d117680189bf4bbb596af7ab84` still has the bug.

I tested a standalone reproducer using raw syscalls and 6.8 UAPI headers:

| Kernel | Result |
| --- | --- |
| `6.8.0-117-generic` | 17/17 pass |
| Official `6.8.0-139-generic`, QEMU | 17/17 fail with inverted results |
| Custom build from Ubuntu `139`, unpatched | 17/17 fail |
| Same source/config/initramfs, three negations removed | 17/17 pass |

The custom builds use a minimal x86-64 QEMU config. I haven't tested the full
Ubuntu generic build or application benchmarks. The unmodified build warns
about an unused variable in `init/do_mounts.c:464`.

I also checked [Bluefield tag `Ubuntu-bluefield-6.8.0-1035.40`](https://git.launchpad.net/~canonical-kernel-bluefield/ubuntu/+source/linux-bluefield/+git/noble/plain/io_uring/kbuf.c?h=Ubuntu-bluefield-6.8.0-1035.40),
at release commit `f5285baaf0cbec83f2841e900a4f7cfaf5adf485`. Its corrected
`kbuf.c` matches the file produced by these three changes.

The [Bluefield log](https://git.launchpad.net/~canonical-kernel-bluefield/ubuntu/+source/linux-bluefield/+git/noble/log/?h=master-next)
lists Uri Dar's fix as `1473fb4e7a3198b834878dce273d2311738a353a`.
The follow-up asks for this existing fix to be carried to Noble generic.
Jammy HWE may need the same follow-up; I haven't checked Jammy's source.

Attached as `launchpad-pbuf-tests.tar.gz`: `pbuf_reserved.c`,
`results/unpatched.log`, `results/corrected.log` and `results/kernel.config`.
