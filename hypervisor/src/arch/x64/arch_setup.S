.section .data

# Scratch space for storing the memory map, executable info, etc.
.global boot_mem_info_scratch
boot_mem_info_scratch:
    .fill 4096, 1, 0

