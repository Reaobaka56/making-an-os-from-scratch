# Tests

Everything here runs against the real OS in QEMU - there is no mocking.

```
make test                    # builds the ISO + disk, then runs every test
python3 tests/run_tests.py -k fork     # run only tests whose name contains "fork"
python3 tests/run_tests.py -v          # print the serial log of failures
```

Needs `qemu-system-x86_64`. Each test boots a fresh VM from a *copy* of `build/disk.img`, so your own disk is never modified.

## What is covered

| Layer | Where | What it checks |
|---|---|---|
| Kernel boot self-test | `karnel/mm.cpp` (`mm_selftest`) | frame allocator, page tables, heap grow/coalesce - the runner fails if any `[FAIL]` appears in the boot log |
| User-space suite | `tests/user/selftest.c` (installed as `/bin/selftest`) | libc (printf, strtol, string functions), `malloc`/`realloc`/`calloc`, `brk`/`mmap`/`munmap`, `fork`, `wait`, `exec`, `spawn`, pipes (incl. blocking on a full pipe, EOF, no-reader errors), `dup`/`dup2`, stdout redirection into pipes and files, file create/read/delete, `readdir`, `ps`, `meminfo` |
| End-to-end runner | `tests/run_tests.py` | boot log, shell, argv, fault isolation, frame leaks, background jobs, shell pipelines and redirection, timer, PS/2 keyboard IRQ, disk persistence across a reboot |

## Adding tests

- **A new user-space check:** add a `check("what it proves", condition)` call in `tests/user/selftest.c`. It prints `[PASS]`/`[FAIL]` and the runner fails if any check fails.
- **A new end-to-end test:** add a function to `tests/run_tests.py` decorated with `@test`. Use `vm.boot()`, then `vm.run("command")` to type at the shell and get its output back; `assert` what you expect.
- **A whole new user program to test:** drop `name.c` in `tests/user/`, add `name` to `TEST_PROGS` in the `Makefile`; it is built with the in-tree libc and installed in `/bin`.
