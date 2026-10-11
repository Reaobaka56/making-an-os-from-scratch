# making-an-os-from-scratch

A small x86_64 operating system in C++ and NASM, booted by [Limine](https://github.com/limine-bootloader/limine).
It has interrupts, a preemptive scheduler, virtual memory, user-mode processes loaded from ELF files,
a disk driver, a filesystem, a small C library and a user-mode shell.

## What works

| Area | Details |
|---|---|
| Boot / output | Limine protocol, framebuffer text console, COM1 serial logging |
| CPU setup | Own GDT + TSS, IDT with all 256 vectors, exception dumps (user faults kill only the process) |
| Memory | Bitmap frame allocator, own 4-level page tables (W^X kernel, NX), kernel heap with `new`/`delete` |
| Interrupts | LAPIC timer (PIT-calibrated, 100 Hz), IOAPIC, PS/2 keyboard, legacy PIC disabled |
| Scheduling | Kernel threads, preemptive round-robin, `sleep_ms`, interrupt-safe heap/allocators |
| Userspace | Per-process address spaces, `syscall`/`sysret`, ELF64 loader, `argv`, user-pointer validation |
| Processes | `fork` (full address-space copy), `exec`, `wait`/`waitpid`, `spawn`, `brk`, anonymous `mmap`/`munmap` |
| User libc | `printf` family, `malloc`/`free`/`realloc`/`calloc` (brk + mmap), string/ctype, `FILE` I/O (`fopen`/`fgets`/`fprintf`...) |
| Storage | ATA PIO driver, RaptorFS (flat named files, contiguous extents), `tools/mkfs.py` |
| Shell | `/bin/sh` runs in user mode (fork+exec+wait, `&` for background jobs); programs: `ls cat echo write rm ps mem uptime clear hello fault`. A built-in kernel shell is the fallback if no disk is present |

Syscalls (see `karnel/syscall.hpp`): `write read open close exit getpid yield sleep uptime spawn fork exec wait brk mmap munmap unlink readdir ps meminfo`.

## Layout

```
karnel/entry.asm    kernel entry (stack setup, calls kmain)
karnel/main.cpp     kmain: init order, boot demos, idle loop
karnel/console.*    framebuffer console + kprintf (mirrors to serial)
karnel/font.hpp     generated 8x16 bitmap font
karnel/serial.*     COM1 logging and input
karnel/gdt.*        GDT + TSS
karnel/idt.*        IDT, exception handler, IRQ dispatch
karnel/isr.asm      ISR stubs for all 256 vectors, gdt_load
karnel/mm.*         memory-manager init + self-test
karnel/pmm.*        physical frame allocator
karnel/vmm.*        page tables, address spaces, MMIO mapping
karnel/heap.*       kernel heap (kmalloc/kfree, operator new/delete)
karnel/apic.*       LAPIC timer, IOAPIC routing
karnel/kbd.*        PS/2 keyboard
karnel/sched.*      threads and scheduler (sched.asm: context switch, usermode entry)
karnel/syscall.*    syscall setup and dispatch (syscall.asm: entry stub)
karnel/elf.*        ELF64 loader
karnel/proc.*       build a process image (ELF + argv) and spawn/wait
karnel/ata.*        ATA PIO driver
karnel/fs.*         RaptorFS
karnel/shell.*      interactive shell
user/               user programs (sh, ls, cat, echo, write, rm, ps, mem, uptime, clear, hello, fault)
user/libc/          the C library (crt0, stdio, stdlib, string, syscall wrappers)
user/include/       libc headers
tests/              end-to-end test runner (tests/run_tests.py) and user-space test suite (tests/user/)
rootfs/             files copied into the disk image
tools/mkfs.py       builds the disk image
linker.ld           kernel link script (higher half, 0xffffffff80000000)
boot/limine.conf    bootloader entry
```

## Requirements

`g++`, `gcc`, `nasm`, `ld`, `make`, `git`, `python3`, `xorriso`, `qemu-system-x86_64`

Debian/Ubuntu: `sudo apt install build-essential nasm xorriso qemu-system-x86 python3`

## Build and run

```
make          # build/kernel.elf
make disk     # build/disk.img (user programs + rootfs/)
make iso      # build/os.iso (clones Limine v8.x-binary into limine-bin/ on first run)
make run      # boot in QEMU with the disk attached; serial is your terminal
make test     # boot the OS in QEMU and run all tests (see tests/README.md)
make clean
```

In `make run` you can type in the QEMU window (PS/2 keyboard) or in the terminal (serial).
Try:

```
os$ ls
os$ cat /README.txt
os$ hello a b c
os$ fault
os$ write /notes.txt hello world
os$ sleepy-job &      (any program followed by & runs in the background)
os$ ps
os$ selftest
```

The disk image persists across boots; delete `build/disk.img` to reset it.

## Limitations / next ideas

- Single CPU only (no SMP); the IOAPIC is assumed at the standard address instead of parsing ACPI.
- `fork` copies every page (no copy-on-write); there are no signals, pipes, redirection or environment variables.
- RaptorFS is flat (no directories), reuses space only when a file is rewritten in place, holds 64 files, and a written file is committed when closed.
- ATA PIO is slow; AHCI or virtio-blk would be the next driver.
- Possible next steps: copy-on-write fork, pipes and `dup2`, a directory-aware filesystem, ACPI + PCI enumeration, networking.
