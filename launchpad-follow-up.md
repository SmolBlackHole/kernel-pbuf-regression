# Draft follow-up for LP #2162843

I investigated this while developing a C++23 entry for the Twelve Dollar
Server Challenge. Other entrants already retry registration with nonzero
reserved fields: [kensac's Rust submission](https://github.com/arjaythedev/twelve-dollar-server-challenge/blob/main/submissions/rust-iouring-kensac/src/main.rs#L62-L87)
and [pavlyhalim's C submission](https://github.com/arjaythedev/twelve-dollar-server-challenge/blob/main/submissions/c-iouring-pavlyhalim/src/sys.h).
Both explicitly reference this bug.

All three reserved-field checks in `io_uring/kbuf.c` are inverted:
`io_register_pbuf_ring()`, `io_unregister_pbuf_ring()` and
`io_register_pbuf_status()`. Each uses `!memchr_inv(..., 0, ...)` where it
needs `memchr_inv(..., 0, ...)`. This rejects valid zero-filled reserved
fields and accepts nonzero ones. The upstream `172484907285` commit is correct;
the inversion is in Ubuntu's adaptation.

Official Noble source packages `136`, `139`, `147` and `149` contain identical
affected `kbuf.c` files; `134` has correct validation. Public Noble master-next
`6dcf89bf9d1650d117680189bf4bbb596af7ab84` still has the three inversions.

The attached standalone reproducer uses raw syscalls and 6.8 UAPI headers:

| Kernel | Result |
| --- | --- |
| `6.8.0-117-generic` | 17/17 pass |
| Official `6.8.0-139-generic`, QEMU | 17/17 fail with inverted results |
| Custom build from Ubuntu `139`, unpatched | 17/17 fail |
| Same source/config/initramfs, three negations removed | 17/17 pass |

The custom builds use a minimal x86-64 QEMU configuration. Full Ubuntu generic
builds and application benchmarks have not been tested. The unmodified build
has an unrelated unused-variable warning in `init/do_mounts.c:464`.

Could you provide the commit for the verified linux-bluefield fix already
recorded here, and confirm whether it should be carried to Noble generic and
Jammy HWE? I have not identified that commit or independently checked Jammy's
source package.

Suggested attachments: `pbuf_reserved.c`, the proposed patch, and the
`results/unpatched.log`, `results/corrected.log` and `results/kernel.config`.
