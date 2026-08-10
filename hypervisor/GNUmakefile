# Disable implicit suffix rules
.SUFFIXES :=

NAME 	= HyveMind
OUTPUT  = hyvemind

BUILD_TARGET_DIR_NAME 	:= build
BUILD_DEPS_DIR_NAME		:= deps
BUILD_TARGET_DIR_PATH	:= $(CURDIR)
override BUILD_TARGET_LOCATION 	:= $(BUILD_TARGET_DIR_PATH)/$(BUILD_TARGET_DIR_NAME)
override BUILD_DEPS_LOCATION	:= $(BUILD_TARGET_LOCATION)/$(BUILD_DEPS_DIR_NAME)

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

CFLAGS 		:= -pipe
CPPFLAGS 	:=
LDFLAGS 	:=
NASMFLAGS 	:=

ifeq ($(DEBUG_BUILD), 1)
    CFLAGS += -g -O0 -DHYVEMIND_DEBUG_BUILD -DHYVEMIND_USE_ANSI_COLORING
    NASMFLAGS += -g
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

override NASMFLAGS += \
	-f elf64 \
	$(patsubst -g,-g -F dwarf,$(NASMFLAGS)) \
	-Wall

ARCH_LINKER_SCRIPT := $(ARCH_SRC_DIR)/boot/$(ARCH).lds

override LDFLAGS += \
	-m elf_x86_64 \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	--gc-sections \
	-T $(ARCH_LINKER_SCRIPT)

HYPERVISOR_SRC_FILES := $(shell find -L src -type f -not -path 'src/arch/*' 2>/dev/null | LC_ALL=C sort)
ARCH_HYPERVISOR_SRC_FILES:= $(shell find -L src/arch/$(ARCH) -type f 2>/dev/null | LC_ALL=C sort)
DEP_SRC_FILES_RAW := $(shell find -L $(BUILD_DEPS_LOCATION)/cc-runtime/src -type f 2>/dev/null | LC_ALL=C sort)
DEP_SRC_FILES := $(subst $(BUILD_DEPS_LOCATION),ext-deps,$(DEP_SRC_FILES_RAW))

ALL_SRC_FILES := $(HYPERVISOR_SRC_FILES) 		\
				 $(ARCH_HYPERVISOR_SRC_FILES) 	\
				 $(DEP_SRC_FILES)

# Files that we only use to auto generate header files -> they shouldnt be included
# in the final executable.
C_AUTOGEN_SOURCES := $(ARCH_SRC_DIR)/asm-offsets.c
C_AUTOGEN_TARGETS := $(patsubst %.c, $(ARCH_INCLUDE_DIR)/generated/%.h, $(notdir $(C_AUTOGEN_SOURCES)))

C_FILES := $(filter-out %$(C_AUTOGEN_SOURCES), $(filter %.c,$(ALL_SRC_FILES)))
GNU_AS_FILES := $(filter %.S,$(ALL_SRC_FILES))
NASM_FILES := $(filter %.asm,$(ALL_SRC_FILES))

# Hypervisor objs go into 			...build/obj-$(ARCH)/...
# External dependency objs go into 	...build/obj-$(ARCH)/ext-deps/...
override OBJ_PREFIX := $(BUILD_TARGET_LOCATION)/obj-$(ARCH)/
OBJS = 	$(addprefix $(OBJ_PREFIX), $(C_FILES:.c=.c.o)) 			\
		$(addprefix $(OBJ_PREFIX), $(GNU_AS_FILES:.S=.S.o)) 	\
		$(addprefix $(OBJ_PREFIX), $(NASM_FILES:.asm=.asm.o))

HEADER_DEPS = $(addprefix $(OBJ_PREFIX),$(C_FILES:.c=.c.d) $(GNU_AS_FILES:.S=.S.d))

# Search for (targets &) prerequisites in the deps folder too
VPATH += $(BUILD_DEPS_LOCATION)/

__all: all

PHONY += all
all: install_build_dependencies build_hypervisor

PHONY += build_hypervisor
build_hypervisor: prepare $(BUILD_TARGET_LOCATION)/bin-$(ARCH)/$(OUTPUT)

-include $(HEADER_DEPS)

PHONY += prepare
prepare: prepare_gen_headers

prepare_gen_headers: $(C_AUTOGEN_TARGETS)

$(OBJ_PREFIX)%.S: $(ARCH_SRC_DIR)/%.c
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -S -o $@

$(C_AUTOGEN_TARGETS): $(ARCH_INCLUDE_DIR)/generated/%.h: $(OBJ_PREFIX)%.S
	$(Q)mkdir -p "$(dir $@)"
	$(Q)../scripts/gen_header_from_c_source.sh $(notdir $@) $(ABS_SRC_TREE) $< $(patsubst %/, %, $(dir $@))

$(BUILD_TARGET_LOCATION)/bin-$(ARCH)/$(OUTPUT): GNUmakefile $(ARCH_LINKER_SCRIPT) $(OBJS)
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(LD) $(LDFLAGS) $(OBJS) -o $@
	$(Q)echo "Successfully built '$(NAME)' into '$(BUILD_TARGET_LOCATION)'!"

$(OBJ_PREFIX)%.c.o: %.c GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_PREFIX)%.S.o: %.S GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_PREFIX)%.asm.o: %.asm GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)nasm $(NASMFLAGS) $< -o $@

$(OBJ_PREFIX)ext-deps/%.c.o: %.c GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_PREFIX)ext-deps/%.S.o: %.S GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(OBJ_PREFIX)ext-deps/%.asm.o: %.asm GNUmakefile
	$(Q)mkdir -p "$(dir $@)"
	$(Q)nasm $(NASMFLAGS) $< -o $@

PHONY += install_build_dependencies
install_build_dependencies: create_build_target
	$(Q)echo "Attempting to install hypervisor build dependencies..."
	$(Q) ../scripts/deps/get-hypervisor-build-deps.sh $(BUILD_DEPS_LOCATION)
	$(Q)echo "Successfully installed hypervisor build dependencies!"

PHONY += create_build_target
create_build_target:
	$(Q)mkdir -p $(BUILD_TARGET_LOCATION)
	$(Q)mkdir -p $(BUILD_DEPS_LOCATION)

PHONY += delete_build_dependencies
delete_build_dependencies:
	$(Q)rm -rf $(BUILD_DEPS_LOCATION)

PHONY += clean
clean:
	$(Q)echo "Removing build artifacts..."
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)/bin-$(ARCH) $(BUILD_TARGET_LOCATION)/obj-$(ARCH)
	$(Q)rm -rf $(ARCH_INCLUDE_DIR)/generated/

PHONY += distclean
distclean: clean
	$(Q)rm -rf $(BUILD_TARGET_LOCATION)
	$(Q)rm -rf .cache/
	$(Q)rm -rf compile_commands.json

.PHONY: $(PHONY)

