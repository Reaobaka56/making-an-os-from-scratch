# making-an-os-from-scratch

A minimal x86_64 kernel in C++ and NASM, booted by [Limine](https://github.com/limine-bootloader/limine).
Currently boots and paints a gradient on the framebuffer.

## Layout

```
karnel/entry.asm   kernel entry point (stack setup, calls kmain)
karnel/main.cpp    kmain: requests a framebuffer from Limine and draws to it
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

Expected result: a blue-green gradient filling the QEMU window.

## Roadmap

- [ ] Text console (bitmap font) and serial logging
- [ ] GDT, IDT, exception handlers
- [ ] Memory map, physical allocator, paging, heap
- [ ] APIC, timer, keyboard
- [ ] Threads and scheduler
- [ ] Syscalls and userspace (ELF loader)
- [ ] Disk driver and filesystem
