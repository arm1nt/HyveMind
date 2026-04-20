# Disable implicit suffix rules
.SUFFIXES:

override NAME 	:= HyveMind
override OUTPUT := hyvemind_img

BUILD_TARGET_DIR_NAME 	:= build
BUILD_DEPS_DIR_NAME 	:= deps
BUILD_TARGET_DIR_PATH 	:= $(PWD)
override BUILD_TARGET_LOCATION 	:= $(BUILD_TARGET_DIR_PATH)/$(BUILD_TARGET_DIR_NAME)
override BUILD_DEPS_LOCATION	:= $(BUILD_TARGET_LOCATION)/$(BUILD_DEPS_DIR_NAME)
override ISO_ROOT_DIR := $(BUILD_TARGET_LOCATION)/iso_root

ifdef QUIET
    override Q := @
else
    override Q :=
endif

PHONY = __all
__all:

include scripts/Makefile.limine-bootloader

include scripts/Makefile.qemu

PHONY += build_hypervisor
build_hypervisor:
	$(Q)$(MAKE) -C hypervisor \
		BUILD_TARGET_DIR_PATH="$(PWD)/hypervisor" QUIET=$(QUIET) all

PHONY += clean_hypervisor
clean_hypervisor:
	$(Q)$(MAKE) -C hypervisor \
		BUILD_TARGET_DIR_PATH="$(PWD)/hypervisor" QUIET=$(QUIET) clean

PHONY += gen_iso_root_dir
gen_iso_root_dir: install_limine_bootloader build_hypervisor
	$(Q)echo "Generating iso root directory..."
	$(Q)rm -rf $(ISO_ROOT_DIR)
	$(Q)mkdir -p $(ISO_ROOT_DIR)/{boot/limine,EFI/BOOT,guest-info}
	$(Q)cp -v hypervisor/build/bin-x64/hyvemind $(ISO_ROOT_DIR)/boot
	$(Q)cp -v limine.conf $(BUILD_DEPS_LOCATION)/limine/limine-bios.sys \
		$(BUILD_DEPS_LOCATION)/limine/limine-bios-cd.bin \
		$(BUILD_DEPS_LOCATION)/limine/limine-uefi-cd.bin \
		$(ISO_ROOT_DIR)/boot/limine/
	$(Q)cp -v $(BUILD_DEPS_LOCATION)/limine/BOOTX64.EFI \
		$(BUILD_DEPS_LOCATION)/limine/BOOTIA32.EFI \
		$(ISO_ROOT_DIR)/EFI/BOOT/
	$(Q)cp hypervisor/resources/* $(ISO_ROOT_DIR)/guest-info/

PHONY += gen_iso_image
gen_iso_image: $(BUILD_TARGET_LOCATION)/isos/$(OUTPUT).iso

$(BUILD_TARGET_LOCATION)/isos/$(OUTPUT).iso: gen_iso_root_dir
	$(Q)mkdir -p $(BUILD_TARGET_LOCATION)/isos
	$(Q)xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		-hfsplus \
		-apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label $(ISO_ROOT_DIR) \
		-o $(BUILD_TARGET_LOCATION)/isos/$(OUTPUT).iso
	$(Q)$(BUILD_DEPS_LOCATION)/limine/limine bios-install $(BUILD_TARGET_LOCATION)/isos/$(OUTPUT).iso
	$(Q)rm -rf $(ISO_ROOT_DIR)

PHONY += clean
clean:
	$(Q)echo "Cleaning up build artifacts..."
	$(Q)$(MAKE) clean_hypervisor
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)/isos/

.PHONY := $(PHONY)

