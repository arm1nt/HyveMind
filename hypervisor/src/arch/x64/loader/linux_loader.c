#include "vm.h"
#include "pf_alloc.h"
#include "loader/loader.h"
#include "loader/linux.h"
#include "asm/gdt_idt.h"
#include "asm/paging.h"
#include "asm/segmentation.h"

/* The default load addresses that we use */
#define LINUX_GUEST_DEFAULT_GDT_GPADDR                      0x10000
#define LINUX_GUEST_DEFAULT_COMMAND_LINE_GPADDR             0x20000
#define LINUX_GUEST_DEFAULT_ZERO_PAGE_GPADDR                0x30000

static int
load_32bit_boot_gdt(struct vm *vm, const gpaddr load_addr)
{
    virt_addr_t gdt_vaddr;
    struct gdt_struct *gdt;

    if (get_page_zeroed(&gdt_vaddr) != 0) {
        pr_error("Failed to allocate guest boot GDT");
        return -1;
    }

    gdt = (struct gdt_struct *) gdt_vaddr;

    segment_descriptor_t cs_desc = {
        .limit0 = 0xFFFF,
        .limit1 = 0xF,

        .low_base = 0,
        .mid_base = 0,
        .high_base = 0,

        .p = SEGMENT_PRESENT,
        .type = CODE_EXECUTE_READ_ACCESSED,
        .s = CODE_DATA_SEGMENT_DESC,

        .dpl = 0,
        .avl = 0,
        .l = 0,
        /* since linux requires a 4gb flat segment */
        .g = 1,
        .db = 1
    };

    segment_descriptor_t ds_desc = {
        .limit0 = 0xFFFF,
        .limit1 = 0xF,

        .low_base = 0,
        .mid_base = 0,
        .high_base = 0,

        .p = SEGMENT_PRESENT,
        .type = DATA_RW_ACCESSED,
        .s = CODE_DATA_SEGMENT_DESC,

        .dpl = 0,
        .avl = 0,
        .l = 0,
        /* since linux here also requires a 4gb flat segment */
        .g = 1,
        .db = 1
    };

    write_gdt_entry(gdt, &cs_desc, 2, CODE_DATA_SEGMENT_DESC);
    write_gdt_entry(gdt, &ds_desc, 3, CODE_DATA_SEGMENT_DESC);

    if (copy_to_vm_gpaddr(vm, load_addr, gdt, PAGE_SIZE) != 0) {
        pr_error("Failed to copy guest GDT into VM");
        free_page(gdt_vaddr);
        return -1;
    }

    free_page(gdt_vaddr);
    return 0;
}

static inline bool
valid_linux_setup_header(const struct setup_header *hdr)
{
    uint64_t declared_setup_hdr_end, expected_setup_hdr_end;

    if (hdr->boot_flag != SETUP_HEADER_MAGIC_NUMBER) {
        pr_error("Cannot detect setup header boot flag with value '0xAA55'. "
                "Instead found: %lx",
                U64(hdr->boot_flag)
        );
        return false;
    }

    if (hdr->header != SETUP_HEADER_MAGIC_SIG_RAW) {
        pr_error("Cannot detect 'HdrS' signature in the setup header. "
                "Expected (%lx) but found (%lx)",
                U64(SETUP_HEADER_MAGIC_SIG_RAW),
                U64(hdr->header)
        );
        return false;
    }

    /**
     * Since this opcode is explicitly used in the header.S, we can use it
     * as additional sanity check
     */
    if ((hdr->jump & 0xFF) != SETUP_HEADER_JMP_OPCODE) {
        pr_error("Cannot detect the explicit '0xEB' jump opcode in the setup "
                "header.\nExpected (0xEB) but found (%lx)",
                U64((hdr->jump & 0xFF))
        );
        return false;
    }

    declared_setup_hdr_end = SETUP_HEADER_MAGIC_SIG_OFFSET + U64(((hdr->jump >> 8) & 0xFF));
    expected_setup_hdr_end = SETUP_HEADER_OFFSET + sizeof(*hdr);

    /* todo: check if this is too strict. is a '<' check sufficient? */
    if (declared_setup_hdr_end != expected_setup_hdr_end) {
        pr_error("Setup header ends at offset 0x%lx, but we expected the "
                "offset to be 0x%lx!",
                U64(declared_setup_hdr_end),
                U64(expected_setup_hdr_end)
        );
        return -1;
    }

    return true;
}

static inline bool
verify_bzImage(const struct setup_header *hdr)
{
    if (hdr->version < BZIMAGE_MIN_BOOT_PROTOCOL_VERSION) {
        pr_error("A bzImage kernel must have a boot protocol version of atleast "
                "2.0. Instead found version (%lx)",
                U64(hdr->version)
        );
        return false;
    }

    if (!(hdr->loadflags & SETUP_LOADFLAGS_LOADED_HIGH)) {
        pr_error("A bzImage kernel must have the 'LOADED_HIGH' flag set");
        return false;
    }

    return true;
}

int
load_linux_32bit_direct_boot_for_vm(
        struct vm *vm,
        const struct guest_config *config,
        struct linux_load_info *load_info
) {
    int ret;
    uint8_t setup_sects;
    uint32_t command_line_len;
    uint32_t pm_kernel_offset;
    uint64_t declared_pm_kernel_size, effective_pm_kernel_load_size;
    uint64_t available_pm_kernel_size;
    virt_addr_t bzImage_start, setup_header_addr, boot_params_raw_addr;
    virt_addr_t pm_kernel_start;
    gpaddr gpaddr_pm_kernel_load_addr;
    gpaddr initramfs_gpaddr;
    struct setup_header *setup_header;
    struct boot_params *boot_params;
    const char *command_line = "console=ttyS0 earlyprintk=serial nokaslr";

    if (config->bzImage_size < SETUP_HEADER_OFFSET + sizeof(struct setup_header)) {
        pr_error("Malformed bzImage provided. The image is too small to contain "
                "even a complete setup header!"
        );
        return -1;
    }

    bzImage_start = __vaddr(config->bzImage_addr);
    setup_header_addr = bzImage_start + SETUP_HEADER_OFFSET;
    setup_header = (struct setup_header *) setup_header_addr;

    if (!valid_linux_setup_header(setup_header)) {
        pr_error("The provided kernel image contains an invalid setup header");
        return -1;
    }

    if (!verify_bzImage(setup_header)) {
        pr_error("Provided image is not a bzImage");
        return -1;
    }

    if (setup_header->version < LINUX_BOOT_MIN_PROTOCOL_VERSION) {
        pr_error("Hypervisor requires the boot protocol version to be "
                "atleast 2.15. Instead, found version: '%lx'",
                U64(setup_header->version)
        );
        return -1;
    }

    if (setup_header->setup_sects == 0) {
        setup_sects = 4;
    } else {
        setup_sects = setup_header->setup_sects;
    }

    pm_kernel_offset = (setup_sects + 1) * LINUX_BOOT_SECTOR_SIZE;
    if (pm_kernel_offset >= config->bzImage_size) {
        pr_error("Malformed bzImage provided. The indicated start of the "
                "protected mode kernel lies outside the kernel bzImage!"
        );
        return -1;
    }

    pm_kernel_start = bzImage_start + pm_kernel_offset;
    available_pm_kernel_size = config->bzImage_size - pm_kernel_offset;
    declared_pm_kernel_size = U64(setup_header->syssize) << 4;

    if (!declared_pm_kernel_size) {
        pr_error("The setup header does not provide information about the "
                "size of the contained protected mode kernel"
        );
        return -1;
    }

    if (declared_pm_kernel_size > available_pm_kernel_size) {
        pr_error("Malformed bzImage provided. The setup header declares a "
                "protected mode kernel size that is larger than can be "
                "satisfied by the kernel image. Declared size (%lx) vs "
                "available size (%lx).",
                U64(declared_pm_kernel_size),
                U64(available_pm_kernel_size)
        );
        return -1;
    }

    gpaddr_pm_kernel_load_addr = setup_header->code32_start;
    if (!gpaddr_pm_kernel_load_addr) {
        pr_error("The setup header does not provide a default pm kernel "
                "load address"
        );
        return -1;
    }

    effective_pm_kernel_load_size = MAX(
            declared_pm_kernel_size,
            U64(setup_header->init_size)
    );

    /**
     * Since we currently load all components (zero-page, gdt, cli) 'in front'
     * of the protected mode kernel, and since we know that they do not overlap,
     * it suffices to check that the pm kernel loaded at the given offset fits
     * into the VM memory.
     *
     * TODO: Try to be more flexible and allow loading the kernel at different
     * addresses (e.g. if we don't have enough mem to load at the default addr).
     * Then we also need to ensure / find a positioning that we can load the
     * other components without conflicts.
     */
    ret = vm_memory_contig_range_fits(
            vm,
            gpaddr_pm_kernel_load_addr,
            effective_pm_kernel_load_size
    );
    if (!ret) {
        pr_error("VM does not have sufficient memory to load protected mode "
                "kernel at address 0x%ld",
                U64(gpaddr_pm_kernel_load_addr)
        );
        return -1;
    }

    ret = copy_to_vm_gpaddr(
            vm,
            gpaddr_pm_kernel_load_addr,
            (void *) pm_kernel_start,
            declared_pm_kernel_size
    );
    if (ret != 0) {
        pr_error("Failed to copy protected mode kernel into the VM memory");
        return -1;
    }

    /**
     * Here we only support non-relocated kernels where the runtime start address
     * is 'pref_address'.
     */
    initramfs_gpaddr =  setup_header->pref_address + effective_pm_kernel_load_size + 1;
    initramfs_gpaddr = align_forward(initramfs_gpaddr, PAGE_SIZE);

    ret = vm_memory_contig_range_fits(vm, initramfs_gpaddr, config->initramfs_size);
    if (!ret) {
        pr_error("VM does not have sufficient memory to store the initramfs image");
        return -1;
    }

    ret = copy_to_vm_gpaddr(
            vm,
            initramfs_gpaddr,
            (void *) config->initramfs_addr,
            config->initramfs_size
    );
    if (ret != 0) {
        pr_error("Failed to copy the initramfs image into the VM memory");
        return -1;
    }

    if (load_32bit_boot_gdt(vm, LINUX_GUEST_DEFAULT_GDT_GPADDR) != 0) {
        pr_error("Failed to load default linux boot GDT into guest memory");
        return -1;
    }

    command_line_len = strlen(command_line);
    if (command_line_len > setup_header->cmdline_size) {
        pr_error("Configured kernel command line string is larger than allowed. "
                "Actual size (%lx) vs max. allowed size (%lx)",
                U64(command_line_len),
                U64(setup_header->cmdline_size)
        );
        return 1;
    }

    ret = copy_to_vm_gpaddr(
            vm,
            LINUX_GUEST_DEFAULT_COMMAND_LINE_GPADDR,
            command_line,
            command_line_len + 1
    );
    if (ret != 0) {
        pr_error("Failed to load the command line string into the VM memory");
        return -1;
    }

    if (get_page_zeroed(&boot_params_raw_addr) != 0) {
        pr_error("Failed to allocate a boot_params struct");
        return -1;
    }

    boot_params = (struct boot_params *) boot_params_raw_addr;

    /**
     * Ensure that we don't modify the header in the loaded bzImage.
     */
    memcpy(&boot_params->hdr, setup_header, sizeof(struct setup_header));
    boot_params->hdr.type_of_loader = 0xFF;
    boot_params->hdr.loadflags &= ~(SETUP_LOADFLAGS_CAN_USE_HEAP);
    boot_params->hdr.cmd_line_ptr = LINUX_GUEST_DEFAULT_COMMAND_LINE_GPADDR;
    boot_params->hdr.ramdisk_image = initramfs_gpaddr;
    boot_params->hdr.ramdisk_size = config->initramfs_size;

    /**
     * Atm we don't simulate any memory holes, but represent the kernel with
     * one big chunk of memory [0, mem_size-1].
     */
    boot_params->e820_entries = 1;
    boot_params->e820_table[0].addr = 0;
    boot_params->e820_table[0].size = get_vm_memory_size(vm);
    boot_params->e820_table[0].type = LINUX_E820_TYPE_RAM;

    ret = copy_to_vm_gpaddr(
            vm,
            LINUX_GUEST_DEFAULT_ZERO_PAGE_GPADDR,
            boot_params,
            PAGE_SIZE
    );
    if (ret != 0) {
        pr_error("Failed to load zero-page into VM memory");
        free_page(boot_params_raw_addr);
        return -1;
    }

    free_page(boot_params_raw_addr);

    load_info->gdt_page = LINUX_GUEST_DEFAULT_GDT_GPADDR;
    load_info->command_line_string = LINUX_GUEST_DEFAULT_COMMAND_LINE_GPADDR;
    load_info->pm_kernel = gpaddr_pm_kernel_load_addr;
    load_info->pm_kernel_size = declared_pm_kernel_size;
    load_info->pm_kernel_entry_point = gpaddr_pm_kernel_load_addr;
    load_info->zero_page = LINUX_GUEST_DEFAULT_ZERO_PAGE_GPADDR;

    return 0;
}
