# Disable implicit suffix rules
.SUFFIXES :=

NAME 	= HyveMind
OUTPUT  = hyvemind

BUILD_TARGET_DIR_NAME 	:= build
BUILD_DEPS_DIR_NAME		:= deps
BUILD_TARGET_DIR_PATH	:= $(CURDIR)
override BUILD_TARGET_LOCATION 	:= $(BUILD_TARGET_DIR_PATH)/$(BUILD_TARGET_DIR_NAME)
override BUILD_DEPS_LOCATION	:= $(BUILD_TARGET_LOCATION)/$(BUILD_DEPS_DIR_NAME)
override HYVEMIND_TARGET = $(BUILD_TARGET_LOCATION)/bin-$(ARCH)/$(OUTPUT)

PHONY := __all
__all:

ifdef QUIET
    override Q := @
else
    override Q :=
endif

ARCH := x64
override SUPPORTED_ARCHS := x64
ifeq ($(filter $(ARCH),$(SUPPORTED_ARCHS)),)
    $(error '$(NAME)' does not support '$(ARCH)'!)
endif

CC := gcc
ifeq ($(filter $(CC),gcc),)
    $(error '$(NAME)' requires gcc as compiler. '$(CC)' is not supported!)
endif

LD := ld
ifeq ($(filter $(LD),ld),)
    $(error '$(NAME)' requires the GNU linker. '$(LD)' is not supported!)
endif

ABS_SRC_TREE		:= $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
SRC_DIR 			:= src
SRC_INCLUDE_DIR 	:= $(SRC_DIR)/include
ARCH_SRC_DIR 		:= $(SRC_DIR)/arch/$(ARCH)
ARCH_INCLUDE_DIR 	:= $(ARCH_SRC_DIR)/include/asm

ARCH_LINKER_SCRIPT := $(ARCH_SRC_DIR)/boot/$(ARCH).lds

CFLAGS 		:= -pipe
CPPFLAGS 	:=
LDFLAGS 	:=

ifeq ($(DEBUG_BUILD), 1)
    CFLAGS += -g -O0 -DHYVEMIND_DEBUG_BUILD -DHYVEMIND_USE_ANSI_COLORING
else
    CFLAGS += -O2
endif

override CFLAGS += \
	-Wall \
    -Wextra \
    -std=gnu23 \
    -nostdinc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-PIC \
    -ffunction-sections \
    -fdata-sections \
	-m64 \
	-march=x86-64 \
	-mabi=sysv \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-mno-red-zone \
	-mcmodel=kernel

override CPPFLAGS += \
	-I src/include \
	-I src/arch/$(ARCH)/include \
	-isystem $(BUILD_DEPS_LOCATION)/freestnd-c-hdrs/include \
	-MMD \
	-MP

override LDFLAGS += \
	-m elf_x86_64 \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	--gc-sections \
	-T $(ARCH_LINKER_SCRIPT)

DEPS_SOURCES_RAW = $(shell find -L $(BUILD_DEPS_LOCATION)/cc-runtime/src -type f 2>/dev/null | LC_ALL=C sort)
DEPS_SOURCES = $(subst $(BUILD_DEPS_LOCATION),ext-deps,$(DEPS_SOURCES_RAW))

# Files that we only use to auto generate header files -> they shouldnt be included
# in the final executable.
C_AUTOGEN_SOURCES := $(ARCH_SRC_DIR)/asm-offsets.c
C_AUTOGEN_TARGETS := $(patsubst %.c, $(ARCH_INCLUDE_DIR)/generated/%.h, $(notdir $(C_AUTOGEN_SOURCES)))

HYVEMIND_COMMON_SOURCES = $(shell find -L $(SRC_DIR) -type f -not -path 'src/arch/*' 2>/dev/null | LC_ALL=C sort)
HYVEMIND_ARCH_SOURCES = $(shell find -L $(ARCH_SRC_DIR) -type f 2>/dev/null | LC_ALL=C sort)
HYVEMIND_SOURCES = $(filter-out %$(C_AUTOGEN_SOURCES), $(HYVEMIND_COMMON_SOURCES) $(HYVEMIND_ARCH_SOURCES))

# Hypervisor objs go into 			...build/obj-$(ARCH)/...
# External dependency objs go into 	...build/obj-$(ARCH)/ext-deps/...
__get_ext_files = $(filter %.$(1), $(2))
__get_ext_objs = $(patsubst %.$(1), %.$(1).o, $(call __get_ext_files,$(1),$(2)))
get_ext_files = $(foreach ext,$(1),$(call __get_ext_files,$(ext),$(2)))
get_ext_objs = $(foreach ext,$(1),$(call __get_ext_objs,$(ext),$(2)))

override OBJ_PREFIX := $(BUILD_TARGET_LOCATION)/obj-$(ARCH)/
add_obj_prefix = $(addprefix $(OBJ_PREFIX), $(1))

DEPS_OBJS = $(call add_obj_prefix,$(call get_ext_objs, c S, $(DEPS_SOURCES)))
HYVEMIND_OBJS = $(call add_obj_prefix,$(call get_ext_objs, c S, $(HYVEMIND_SOURCES)))
OBJS = $(DEPS_OBJS) $(HYVEMIND_OBJS)

HEADER_DEPS_RAW = $(call get_ext_files,c S,$(DEPS_SOURCES)) 	\
				  $(call get_ext_files,c S,$(HYVEMIND_SOURCES)) \
				  $(call get_ext_files,c S,$(C_AUTOGEN_SOURCES))
HEADER_DEPS = $(call add_obj_prefix,$(addsuffix .d, $(HEADER_DEPS_RAW)))
-include $(HEADER_DEPS)

# Search for (targets &) prerequisites in the deps folder too
VPATH += $(BUILD_DEPS_LOCATION)/

__all: all

PHONY += all
all: setup
	$(Q)$(MAKE) build_hypervisor

PHONY += build_hypervisor
build_hypervisor: prepare $(HYVEMIND_TARGET)

PHONY += prepare
prepare: prepare_gen_headers

prepare_gen_headers: $(C_AUTOGEN_TARGETS)

$(OBJ_PREFIX)%.S: $(ARCH_SRC_DIR)/%.c
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -S -o $@

$(C_AUTOGEN_TARGETS): $(ARCH_INCLUDE_DIR)/generated/%.h: $(OBJ_PREFIX)%.S
	$(Q)mkdir -p "$(dir $@)"
	$(Q)../scripts/gen_header_from_c_source.sh \
		$(notdir $@) $(ABS_SRC_TREE) $< $(patsubst %/, %, $(dir $@))

$(HYVEMIND_TARGET): GNUmakefile $(ARCH_LINKER_SCRIPT) $(OBJS)
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(LD) $(LDFLAGS) $(OBJS) -o $@
	$(Q)echo "Successfully built '$(NAME)' into '$(BUILD_TARGET_LOCATION)'!"

$(OBJ_PREFIX)%.c.o: %.c GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_PREFIX)%.S.o: %.S GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_PREFIX)ext-deps/%.c.o: %.c GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_PREFIX)ext-deps/%.S.o: %.S GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

PHONY += setup
setup: install_deps

PHONY += install_deps
install_deps: $(BUILD_DEPS_LOCATION)/.deps-installed

$(BUILD_DEPS_LOCATION)/.deps-installed:
	$(Q)echo "Attempting to install hypervisor dependencies..."
	$(Q)mkdir -p $(BUILD_DEPS_LOCATION)
	$(Q)../scripts/deps/get-hypervisor-build-deps.sh $(dir $@)
	$(Q)touch $@
	$(Q)echo "Successfully installed hypervisor dependencies!"

PHONY += clean
clean:
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)/bin-$(ARCH) $(BUILD_TARGET_LOCATION)/obj-$(ARCH)
	$(Q)rm -rf $(ARCH_INCLUDE_DIR)/generated/
	$(Q)rm -rf .cache/
	$(Q)rm -rf compile_commands.json

PHONY += distclean
distclean: clean
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)

.PHONY: $(PHONY)

