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

include scripts/Makefile.limine
include scripts/Makefile.qemu

PHONY += build_hypervisor
build_hypervisor:
	$(Q)$(MAKE) -C hypervisor BUILD_TARGET_DIR_PATH="$(PWD)/hypervisor" all

PHONY += clean_hypervisor
clean_hypervisor:
	$(Q)$(MAKE) -C hypervisor BUILD_TARGET_DIR_PATH="$(PWD)/hypervisor" clean

TARGET_ISO := $(BUILD_TARGET_LOCATION)/isos/$(OUTPUT).iso
TARGET_HDD := $(BUILD_TARGET_LOCATION)/hdds/$(OUTPUT).hdd

PHONY += gen_iso_image
gen_iso_image: $(TARGET_ISO)

$(TARGET_ISO): gen_iso_root_dir
	$(Q)mkdir -p $(BUILD_TARGET_LOCATION)/isos

	$(Q)xorriso -as mkisofs -R -r -J \
		-hfsplus \
		-apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		$(ISO_ROOT_DIR) \
		-o $(TARGET_ISO)

	$(Q)rm -rf $(ISO_ROOT_DIR)

PHONY += gen_iso_root_dir
gen_iso_root_dir: install_limine build_hypervisor
	$(Q)echo "Generating iso root directory..."
	$(Q)rm -rf $(ISO_ROOT_DIR)
	$(Q)mkdir -p $(ISO_ROOT_DIR)/{boot/limine,EFI/BOOT,guest-info}

	$(Q)cp -v hypervisor/build/bin-x64/hyvemind $(ISO_ROOT_DIR)/boot
	$(Q)cp -v limine.conf $(BUILD_DEPS_LOCATION)/limine/bin/limine-uefi-cd.bin \
		$(ISO_ROOT_DIR)/boot/limine/
	$(Q)cp -v $(BUILD_DEPS_LOCATION)/limine/bin/BOOTX64.EFI $(ISO_ROOT_DIR)/EFI/BOOT/

	$(Q)cp -r hypervisor/resources/. $(ISO_ROOT_DIR)/guest-info/

PHONY += gen_hdd_image
gen_hdd_image: $(TARGET_HDD)

$(TARGET_HDD): install_limine build_hypervisor
	$(Q)echo "Generating HDD image..."
	$(Q)mkdir -p $(BUILD_TARGET_LOCATION)/hdds

	$(Q)rm -f $@
	$(Q)dd if=/dev/zero bs=1M count=0 seek=64 of=$@

	$(Q)PATH=$$PATH:/usr/sbin:/sbin sgdisk $@ -n 1:2048 -t 1:ef00 -m 1
	$(Q)mformat -i $@@@1M
	$(Q)mmd -i $@@@1M \
		::/EFI \
		::/EFI/BOOT \
		::/boot \
		::/boot/limine \
		::/guest-info

	$(Q)mcopy -i $@@@1M limine.conf ::/boot/limine/
	$(Q)mcopy -i $@@@1M $(BUILD_DEPS_LOCATION)/limine/bin/BOOTX64.EFI ::/EFI/BOOT/
	$(Q)mcopy -i $@@@1M hypervisor/build/bin-x64/hyvemind ::/boot/
	$(Q)mcopy -s -i $@@@1M hypervisor/resources/* ::/guest-info/

PHONY += clean
clean: clean_hypervisor
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)/isos/
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)/hdds/

PHONY += distclean
distclean: clean_hypervisor delete_limine_installation
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)/isos/
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)/hdds/

.PHONY : $(PHONY)

