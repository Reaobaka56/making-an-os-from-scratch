CXX  := g++
NASM := nasm
BUILD := build

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel \
            -mno-80387 -mno-mmx -mno-sse -mno-sse2 -m64
LDFLAGS  := -nostdlib -static -z max-page-size=0x1000 -T linker.ld

OBJS := $(BUILD)/entry.o $(BUILD)/main.o

all: $(BUILD)/kernel.elf

$(BUILD)/entry.o: karnel/entry.asm
	@mkdir -p $(BUILD)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/main.o: karnel/main.cpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJS) linker.ld
	ld $(LDFLAGS) $(OBJS) -o $@

# Bootable ISO (needs xorriso; fetches Limine binaries on first run)
limine-bin/limine:
	git clone --depth 1 --branch v8.x-binary https://github.com/limine-bootloader/limine.git limine-bin
	$(MAKE) -C limine-bin

iso: all limine-bin/limine
	rm -rf $(BUILD)/iso && mkdir -p $(BUILD)/iso/boot/limine $(BUILD)/iso/EFI/BOOT
	cp $(BUILD)/kernel.elf $(BUILD)/iso/boot/
	cp boot/limine.conf $(BUILD)/iso/boot/limine/
	cp limine-bin/limine-bios.sys limine-bin/limine-bios-cd.bin limine-bin/limine-uefi-cd.bin $(BUILD)/iso/boot/limine/
	cp limine-bin/BOOTX64.EFI $(BUILD)/iso/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin -no-emul-boot \
	  -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 \
	  --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image \
	  --protective-msdos-label $(BUILD)/iso -o $(BUILD)/os.iso
	limine-bin/limine bios-install $(BUILD)/os.iso

run: iso
	qemu-system-x86_64 -m 256M -cdrom $(BUILD)/os.iso

clean:
	rm -rf $(BUILD)

.PHONY: all iso run clean
