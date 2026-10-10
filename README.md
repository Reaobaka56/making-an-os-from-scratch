# making-an-os-from-scratch

A small x86_64 operating system in C++ and NASM, booted by [Limine](https://github.com/limine-bootloader/limine).
It has interrupts, a preemptive scheduler, virtual memory, user-mode processes loaded from ELF files,
a disk driver, a filesystem and an interactive shell.

## What works

| Area | Details |
|---|---|
| Boot / output | Limine protocol, framebuffer text console, COM1 serial logging |
| CPU setup | Own GDT + TSS, IDT with all 256 vectors, exception dumps (user faults kill only the process) |
| Memory | Bitmap frame allocator, own 4-level page tables (W^X kernel, NX), kernel heap with `new`/`delete` |
| Interrupts | LAPIC timer (PIT-calibrated, 100 Hz), IOAPIC, PS/2 keyboard, legacy PIC disabled |
| Scheduling | Kernel threads, preemptive round-robin, `sleep_ms`, interrupt-safe heap/allocators |
| Userspace | Per-process address spaces, `syscall`/`sysret`, ELF64 loader, user-pointer validation |
| Storage | ATA PIO driver, RaptorFS (flat named files, contiguous extents), `tools/mkfs.py` |
| Shell | `ls cat write rm run ps mem df uptime echo clear help`; input from keyboard or serial |

Syscalls: `write read open close exit getpid yield sleep uptime spawn` (see `karnel/syscall.hpp`).

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
karnel/proc.*       spawn a user process from a file
karnel/ata.*        ATA PIO driver
karnel/fs.*         RaptorFS
karnel/shell.*      interactive shell
user/               user programs (hello, fault) + tiny syscall header + linker script
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
make clean
```

In `make run` you can type in the QEMU window (PS/2 keyboard) or in the terminal (serial).
Try:

```
os> ls
os> cat /README.txt
os> run /bin/hello
os> run /bin/fault
os> write /notes.txt hello
os> ps
```

The disk image persists across boots; delete `build/disk.img` to reset it.

## Limitations / next ideas

- Single CPU only (no SMP); the IOAPIC is assumed at the standard address instead of parsing ACPI.
- RaptorFS is flat (no directories), reuses space only when a file is rewritten in place, and holds 64 files.
- ATA PIO is slow; AHCI or virtio-blk would be the next driver.
- User processes have no argv/environment, no fork/exec, no signals, and no dynamic memory syscall yet.
- Possible next steps: `brk`/`mmap`, argv, a user-space libc, pipes, ACPI, PCI enumeration, networking.
