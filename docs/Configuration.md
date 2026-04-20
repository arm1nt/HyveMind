# Hyvemind Configuration

## Overview of supported configuration flags

- `HYVEMIND_GUEST_CONFIG_FROM_BOOTLOADER`

*Description*:

Receive information about guest VMs (e.g. kernel images, file system images, etc.)
or any other miscellaneous files directly from the bootloader.

To instruct a Limine compliant bootloader to load the desired files as modules
and pass them as `struct limine_file`'s to the hypervisor, you have to update
the `limine.conf` file accordingly.

For example:
```
timeout: 5

/HyveMind Hypervisor
    protocol: limine
    path: boot():/boot/hyvemind

    # File to be passed by the bootloader
    module_path: boot():/guest-info/test.txt
    module_string: my-test-file
```

This minimal example configures the bootloader to pass the file 'test.txt' to the hypervisor.

>*Note*: By placing the requested file(s) into the 'hypervisor/resources' directory,
they will automatically be copied into the '/guest-info/' directory in the built
ISO image.

