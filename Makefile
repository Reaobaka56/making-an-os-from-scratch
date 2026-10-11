CXX  := g++
NASM := nasm
BUILD := build

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -fno-pic -mno-red-zone -mcmodel=kernel \
            -mno-80387 -mno-mmx -mno-sse -mno-sse2 -m64
LDFLAGS  := -nostdlib -static -z max-page-size=0x1000 -T linker.ld

CPP_SRCS := $(wildcard karnel/*.cpp)
ASM_SRCS := $(wildcard karnel/*.asm)
OBJS := $(patsubst karnel/%.cpp,$(BUILD)/%.o,$(CPP_SRCS)) $(patsubst karnel/%.asm,$(BUILD)/%_asm.o,$(ASM_SRCS))

USER_CFLAGS := -std=gnu11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie \
               -fno-tree-loop-distribute-patterns -mgeneral-regs-only -Wall -Wextra \
               -nostdinc -isystem $(shell gcc -print-file-name=include) -Iuser/include
USER_PROGS  := sh ls cat echo write rm ps mem uptime clear hello fault
TEST_PROGS  := selftest
USER_ELFS   := $(patsubst %,$(BUILD)/user/%.elf,$(USER_PROGS) $(TEST_PROGS))
LIBC_OBJS   := $(patsubst user/libc/%.c,$(BUILD)/user/lib/%.o,$(wildcard user/libc/*.c))

all: $(BUILD)/kernel.elf

$(BUILD)/%_asm.o: karnel/%.asm
	@mkdir -p $(BUILD)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/%.o: karnel/%.cpp $(wildcard karnel/*.hpp)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJS) linker.ld
	ld $(LDFLAGS) $(OBJS) -o $@

# User-space libc and programs (static ELF, linked at 0x400000)
$(BUILD)/user/lib/%.o: user/libc/%.c $(wildcard user/include/*.h)
	@mkdir -p $(BUILD)/user/lib
	gcc $(USER_CFLAGS) -c $< -o $@

$(BUILD)/user/lib/crt0.o: user/libc/crt0.S
	@mkdir -p $(BUILD)/user/lib
	gcc -c $< -o $@

$(BUILD)/user/libc.a: $(LIBC_OBJS)
	ar rcs $@ $^

USER_LINK = gcc $(USER_CFLAGS) -nostdlib -static -no-pie -Wl,-T,user/linker.ld -Wl,--build-id=none \
            $(BUILD)/user/lib/crt0.o $< $(BUILD)/user/libc.a

$(BUILD)/user/%.elf: user/%.c $(BUILD)/user/libc.a $(BUILD)/user/lib/crt0.o user/linker.ld $(wildcard user/include/*.h)
	@mkdir -p $(BUILD)/user
	$(USER_LINK) -o $@

$(BUILD)/user/%.elf: tests/user/%.c $(BUILD)/user/libc.a $(BUILD)/user/lib/crt0.o user/linker.ld $(wildcard user/include/*.h)
	@mkdir -p $(BUILD)/user
	$(USER_LINK) -o $@

# Disk image: everything under rootfs/ plus the programs in /bin
$(BUILD)/disk.img: tools/mkfs.py $(USER_ELFS) $(shell find rootfs -type f)
	@mkdir -p $(BUILD)
	python3 tools/mkfs.py $@ 8 rootfs $(foreach p,$(USER_PROGS) $(TEST_PROGS),/bin/$(p)=$(BUILD)/user/$(p).elf)

disk: $(BUILD)/disk.img

# Automated tests: boots the OS headless in QEMU and checks its behaviour (see tests/README.md)
test: iso disk
	python3 tests/run_tests.py

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

run: iso $(BUILD)/disk.img
	qemu-system-x86_64 -m 256M -serial stdio -drive file=$(BUILD)/disk.img,format=raw,if=ide -cdrom $(BUILD)/os.iso -boot d

clean:
	rm -rf $(BUILD)

.PHONY: all iso run clean disk test
