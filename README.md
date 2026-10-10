# making-an-os-from-scratch

A minimal x86_64 kernel in C++ and NASM, booted by [Limine](https://github.com/limine-bootloader/limine).
Currently boots, prints to the framebuffer and serial port, and has its own GDT/IDT with exception handlers.

## Layout

```
karnel/entry.asm   kernel entry point (stack setup, calls kmain)
karnel/main.cpp    kmain: Limine requests, init order
karnel/console.*   framebuffer text console + kprintf (mirrors to serial)
karnel/font.hpp    generated 8x16 bitmap font
karnel/serial.*    COM1 serial logging
karnel/gdt.*       GDT + TSS (IST1 stack for double faults)
karnel/idt.*       IDT + exception handler (register dump)
karnel/isr.asm     ISR stubs for all 256 vectors, gdt_load
karnel/mem.cpp    memcpy/memset/memmove/memcmp
linker.ld          higher-half link script (0xffffffff80000000)
boot/limine.conf   bootloader menu entry
Makefile           build, ISO, and QEMU targets
```

## Requirements

`g++`, `nasm`, `ld`, `make`, `git`, `xorriso`, `qemu-system-x86_64`

Debian/Ubuntu: `sudo apt install build-essential nasm xorriso qemu-system-x86`

## Build and run

```
make          # build/kernel.elf
make iso      # build/os.iso (clones Limine v8.x-binary into limine-bin/ on first run)
make run      # boot the ISO in QEMU
make clean
```

Expected result: boot messages in the QEMU window and on stdout (`-serial stdio`).
To test the exception handler, uncomment the null-write line in `kmain` (page fault dump).

## Roadmap

- [x] Text console (bitmap font) and serial logging
- [x] GDT, IDT, exception handlers
- [ ] Memory map, physical allocator, paging, heap
- [ ] APIC, timer, keyboard
- [ ] Threads and scheduler
- [ ] Syscalls and userspace (ELF loader)
- [ ] Disk driver and filesystem
