#include "config_private.h"
#include "fatal.h"
#include "types.h"
#include "guest_config.h"
#include "printf.h"
#include "string.h"
#include "vm_config_defs.h"
#include "lib/radix-tree.h"

#define T struct vm_config
#define PREFIX vm_config
#define VECTOR_IMPLEMENTATION
#include "lib/vector_template.h"

struct vm_request {
    /* To associate error messages with config entries */
    int id;
    char *name;
    enum guest_type type;
    unsigned int vcpus;
    unsigned int mem_size;
    enum mem_granularity granularity;

    struct {
        struct {
            char *bzImage_name;
            char *initramfs_name;
            char *cmdline_str;
        } linux;
    } boot;
};

#define T struct vm_request
#define PREFIX vm_req
#define VECTOR_DEFINITIONS
#define VECTOR_IMPLEMENTATION
#include "lib/vector_template.h"

static struct radix_tree *supported_keys = NULL;

static inline struct limine_file *
__get_limine_file(const struct limine_module_response *mods, const char *file_name)
{
    for (uint64_t i = 0; i < mods->module_count; i++) {
        if (strcmp(file_name, mods->modules[i]->string) == 0) {
            return mods->modules[i];
        }
    }
    return NULL;
}

static bool
get_vm_config_file(const struct limine_module_response *mods, config_file_t *config)
{
    const struct limine_file *raw_file = __get_limine_file(mods, HYVEMIND_CONFIG_FILE);
    if (!raw_file) {
        return false;
    }

    config->f = (char *) raw_file->address;
    config->size = raw_file->size;
    config->pos = 0;
    config->ops.is_whitespace = is_config_whitespace;
    config->ops.is_delim = is_config_delim;
    config->ops.is_valid_token_char = is_valid_config_token_char;

    return true;
}

static inline struct radix_tree *
get_config_key_radix(void)
{
    struct radix_tree *tree = create_radix_tree();
    if (!tree) {
        pr_error("Failed to allocate radix tree containing the allowed config keys");
        return NULL;
    }

    for (int i = 0; i < __CONFIG_OPTIONS_NR; i++) {
        if (!radix_tree_add(tree, config_key_strings[i], i)) {
            pr_error("Failed to add key '%s' into the radix tree", config_key_strings[i]);
            goto error_out;
        }
    }

    return tree;

error_out:
    destroy_radix_tree(tree);
    return NULL;
}

#define log_req_error(fmt, ...) \
    pr_error("(Config #%lu) " fmt, request->id __VA_OPT__(,) __VA_ARGS__)

static bool
valid_config_request(const struct vm_request *request)
{
    if (request->name == NULL) {
        log_req_error("The VM name is missing!");
        return false;
    }

    if (request->type == GUEST_UNSPECIFIED) {
        log_req_error("No VM type is specified!");
        return false;
    }

    if (request->vcpus == 0) {
        log_req_error("The desired number of vCPUs is not set!");
        return false;
    }

    if (request->mem_size == 0) {
        log_req_error("The desired memory capacity is not configured!");
        return false;
    }

    switch (request->type) {
        case LINUX_DIRECT_BOOT_32BIT:
            if (request->boot.linux.bzImage_name == NULL) {
                log_req_error("No bzImage name specified");
                return false;
            }

            if (request->boot.linux.initramfs_name == NULL) {
                log_req_error("No initramfs name specified");
                return false;
            }
            break;
        case MIRROR_VMM:
            break;
        case GUEST_UNSPECIFIED:
            die_reason("Unreachable");
    }

    return true;
}

static inline int
get_guest_type_from_string(const char *str)
{
    int index = GUEST_UNSPECIFIED + 1;
    const char *type_str;

    while ((type_str = vm_type_strings[index]) != NULL) {
        if (strcmp(type_str, str) == 0) {
            return index;
        }
        index++;
    }
    return -1;
}

static inline int
get_mem_granularity_from_string(const char *str)
{
    int index = 0;
    const char *gran_string;

    while ((gran_string = mem_granularity_strings[index]) != NULL) {
        if (strcmp(gran_string, str) == 0) {
            return index;
        }
        index++;
    }
    return -1;
}

static bool
set_vm_request_val(struct vm_request *request, const struct config_line *entry)
{
    bool overflow;
    char *endptr;
    int guest_type, mem_gran;

    switch (entry->key) {
        case CONFIG_VM_NAME_KEY:
            request->name = strdup_nt(entry->value);
            break;
        case CONFIG_VM_TYPE_KEY:
            guest_type = get_guest_type_from_string(entry->value);
            if (guest_type == -1) {
                log_req_error("VM type '%s' is not supported", entry->value);
                return false;
            }

            request->type = (enum guest_type) guest_type;
            break;
        case CONFIG_VM_VCPUS_KEY:
            request->vcpus = (unsigned int) strtoul(entry->value, &endptr, &overflow);

            if ((entry->value == endptr || *endptr != '\0') && *endptr != '-' && !overflow) {
                log_req_error("Expected the vcpu number value to only consist of "
                        "one or more digits. Instead, found: %s",
                        entry->value
                );
                return false;
            } else if (*endptr == '-' || overflow || request->vcpus == 0) {
                log_req_error("Expected the vcpu number value to be an integer "
                        "greater than 1. Instead, found: %s",
                        entry->value
                );
                return false;
            }

            break;
        case CONFIG_VM_MEM_SIZE_KEY:
            request->mem_size = (unsigned int) strtoul(entry->value, &endptr, &overflow);

            if ((entry->value == endptr || *endptr != '\0') && *endptr != '-' && !overflow) {
                log_req_error("Expected the mem size value to only consist of "
                        "one or more digits. Instead, found: %s",
                        entry->value
                );
                return false;
            } else if (*endptr == '-' || overflow || request->vcpus == 0) {
                log_req_error("Expected the mem size value to be an integer "
                        "greater than 1. Instead, found: %s",
                        entry->value
                );
                return false;
            }

            break;
        case CONFIG_VM_MEM_GRANULARITY_KEY:
            mem_gran = get_mem_granularity_from_string(entry->value);
            if (mem_gran == -1) {
                log_req_error("Mem granularity value '%s' is not allowed", entry->value);
                return false;
            }

            request->granularity = (enum mem_granularity) mem_gran;
            break;
        case CONFIG_VM_BOOT_LINUX_BZIMAGE_KEY:
            request->boot.linux.bzImage_name = strdup_nt(entry->value);
            break;
        case CONFIG_VM_BOOT_LINUX_INITRAMFS_KEY:
            request->boot.linux.initramfs_name = strdup_nt(entry->value);
            break;
        case CONFIG_VM_BOOT_LINUX_CMDLINE_KEY:
            request->boot.linux.cmdline_str = strdup_nt(entry->value);
            break;
        case __CONFIG_OPTIONS_NR:
        case __CONFIG_INVALID_KEY:
            die_reason("Unreachable");
    }

    return true;
}

static inline void
get_token(struct parser_state *state, char *token)
{
    const int token_len = token_len(state);
    memcpy(token, token_ptr(state), token_len);
    token[token_len] = '\0';
}

static inline bool
__is_token_equal_to(const char *target, const struct parser_state *state)
{
    const int target_len = strlen(target);
    const int token_len = token_len(state);

    if (token_len != target_len) {
        return false;
    }

    return strncmp(target, token_ptr(state), token_len) == 0;
}

static enum parsing_state
__handle_search_start_marker(struct parser_state *state)
{
    if (state->token_seek_status == SEEK_OOM) {
        return PS_DONE_SUCCESS;
    }

    if (__is_token_equal_to(CONFIG_SECTION_START, state)) {
        state->token_state = TOKEN_SEARCH_CONFIG_ENTRY_START;
        return PS_CONTINUE;
    }

    char token[token_len(state) + 1];
    get_token(state, token);

    pr_error("Expected a config section start marker (%s) but found '%s' instead",
            CONFIG_SECTION_START,
            token
    );

    return PS_STOP_ERROR;
}

static enum parsing_state
__handle_search_config_entry_start(struct parser_state *state)
{
    if (state->token_seek_status == SEEK_OOM) {
        pr_error("Expected a configuration line or a '%s' but reached EOF instead",
                CONFIG_SECTION_END
        );
        return PS_STOP_ERROR;
    }

    if (__is_token_equal_to(CONFIG_SECTION_END, state)) {
        state->token_state = TOKEN_SEARCH_START_MARKER;
        return PS_CONFIG_ENTRY_COMPLETED;
    }

    char token[token_len(state) + 1];
    get_token(state, token);
    uintptr_t key_val;

    if (!radix_tree_get(supported_keys, token, &key_val)) {
        pr_error("Encountered unsupported key '%s'", token);
        return PS_STOP_ERROR;
    }

    state->curr_line->key = (enum config_key) key_val;
    state->token_state = TOKEN_SEARCH_ENTRY_DELIM;
    return PS_CONTINUE;
}

static enum parsing_state
__handle_search_delim(struct parser_state *state)
{
    if (state->token_seek_status == SEEK_OOM) {
        pr_error("Expected config entry delimiter (%s) but reached EOF instead",
                CONFIG_SECTION_DELIM
        );
        return PS_STOP_ERROR;
    }

    if (__is_token_equal_to(CONFIG_SECTION_DELIM, state)) {
        state->token_state = TOKEN_SEARCH_ENTRY_VAL;
        return PS_CONTINUE;
    }

    char token[token_len(state) +1];
    get_token(state, token);

    pr_error("Expected the config entry delimiter '%s' but found '%s' instead",
            CONFIG_SECTION_DELIM,
            token
    );
    return PS_STOP_ERROR;
}

static enum parsing_state
__handle_search_value(struct parser_state *state)
{
    if (state->token_seek_status == SEEK_OOM) {
        pr_error("Expected a configuration value for '%s' but reached EOF instead",
                config_key_strings[state->curr_line->key]
        );
        return PS_STOP_ERROR;
    }

    for (int i = 0; reserved_tokens[i] != NULL; i++) {
        if (__is_token_equal_to(reserved_tokens[i], state)) {
            pr_error("Expected a configuration value for key %s, but found '%s' instead",
                    config_key_strings[state->curr_line->key],
                    reserved_tokens[i]
            );
            return PS_STOP_ERROR;
        }
    }

    char token[token_len(state) + 1];
    get_token(state, token);
    state->curr_line->value = token;

    if (!set_vm_request_val(state->curr_request, state->curr_line)) {
        pr_error("Encountered invalid value for configuration key %s: %s",
                config_key_strings[state->curr_line->key],
                token
        );
        return PS_STOP_ERROR;
    }

    state->token_state = TOKEN_SEARCH_CONFIG_ENTRY_START;
    return PS_CONTINUE;
}

static enum config_parsing_status
parse_vm_config_file(config_file_t *file, struct vm_req_vector *req_vec)
{
    enum parsing_state parsing_state = PS_CONTINUE;

    struct config_line entry = {0};
    struct vm_request request = {0};
    struct parser_state state = {0};

    state.file = file;
    state.curr_line = &entry;
    state.curr_request = &request;
    state.token_state = TOKEN_SEARCH_START_MARKER;

    while (parsing_state == PS_CONTINUE) {
        state.token_seek_status =
            seek_next_token(file, &state.token_start, &state.token_end);

        if (state.token_seek_status == SEEK_INVALID_SYMBOL) {
            pr_error("Encountered invalid symbol while parsing the VM config file");
            parsing_state = PS_STOP_ERROR;
            break;
        }

        switch (state.token_state) {
            case TOKEN_SEARCH_START_MARKER:
                parsing_state = __handle_search_start_marker(&state);
                break;
            case TOKEN_SEARCH_CONFIG_ENTRY_START:
                parsing_state = __handle_search_config_entry_start(&state);
                break;
            case TOKEN_SEARCH_ENTRY_DELIM:
                parsing_state = __handle_search_delim(&state);
                break;
            case TOKEN_SEARCH_ENTRY_VAL:
                parsing_state = __handle_search_value(&state);
                break;
            default:
                die_reason("Unsupported parser state");
        }

        if (parsing_state == PS_CONFIG_ENTRY_COMPLETED) {
            if (!valid_config_request(state.curr_request)) {
                return PARSING_ERR_INVALID_ENTRY;
            }

            const int prev_req_id = request.id;
            push_back_vm_req_vector(req_vec, *state.curr_request);
            memset(state.curr_request, 0, sizeof(struct vm_request));
            request.id = prev_req_id + 1;
            parsing_state = PS_CONTINUE;
        }
    }

    if (parsing_state == PS_DONE_SUCCESS) {
        return PARSING_SUCCESS;
    }

    return PARSING_ERR_MALFORMED_FILE;
}

static void
destroy_vm_request(const struct vm_request *request)
{
    if (request->name) {
        hfree(request->name);
    }
    if (request->boot.linux.bzImage_name) {
        hfree(request->boot.linux.bzImage_name);
    }
    if (request->boot.linux.initramfs_name) {
        hfree(request->boot.linux.initramfs_name);
    }
    if (request->boot.linux.cmdline_str) {
        hfree(request->boot.linux.cmdline_str);
    }
}

static void
destroy_vm_reqs(const struct vm_req_vector *requests)
{
    const int nr_reqs = size_vm_req_vector(requests);
    struct vm_request request;

    for (int i = 0; i < nr_reqs; i++) {
        at_vm_req_vector(requests, &request, i);
        destroy_vm_request(&request);
    }

    destroy_vm_req_vector(requests);
}

static struct vm_req_vector *
get_vm_requests(const struct limine_module_response *mods)
{
    config_file_t config;
    if (!get_vm_config_file(mods, &config)) {
        pr_error("VM config file is missing!");
        return NULL;
    }

    supported_keys = get_config_key_radix();
    if (!supported_keys) {
        return NULL;
    }

    struct vm_req_vector *requests = create_vm_req_vector(DEFAULT_VEC_INIT_CAPACITY);
    if (!requests) {
        goto req_alloc_err;
    }

    const enum config_parsing_status status = parse_vm_config_file(&config, requests);
    switch (status) {
        case PARSING_SUCCESS:
            pr_info("Successfully parsed the VM config file");
            break;
        case PARSING_ERR_INVALID_ENTRY:
            pr_error("VM config file contains an invalid config entry");
            goto req_parsing_err;
        case PARSING_ERR_MALFORMED_FILE:
            pr_error("VM config file is malformed");
            goto req_parsing_err;
    }

    destroy_radix_tree(supported_keys);
    return requests;

req_parsing_err:
    destroy_vm_reqs(requests);
req_alloc_err:
    destroy_radix_tree(supported_keys);
    return NULL;
}

static bool
get_linux_boot_info(
        const struct limine_module_response *mods,
        const struct vm_request *request,
        struct vm_config *config
) {
    const struct limine_file *raw_bzimage =
        __get_limine_file(mods, request->boot.linux.bzImage_name);

    if (raw_bzimage == NULL) {
        log_req_error("Cannot find specified bzImage file by the name '%s'",
                request->boot.linux.bzImage_name
        );
        return false;
    }

    config->boot_info.linux.bzImage.addr = (uintptr_t) raw_bzimage->address;
    config->boot_info.linux.bzImage.size = raw_bzimage->size;

    const struct limine_file *raw_initramfs =
        __get_limine_file(mods, request->boot.linux.initramfs_name);

    if (raw_initramfs == NULL) {
        log_req_error("Cannot find specified initramfs file by the name '%s'",
                request->boot.linux.initramfs_name
        );
        return false;
    }

    config->boot_info.linux.initramfs.addr = (uintptr_t) raw_initramfs->address;
    config->boot_info.linux.initramfs.size = raw_initramfs->size;

    config->boot_info.linux.cmdline_str = strdup_nt(request->boot.linux.cmdline_str);
    return true;
}

static bool
vm_req_to_vm_config(
        const struct limine_module_response *mods,
        const struct vm_request *req,
        struct vm_config *config
) {
    memset(config, 0, sizeof(struct vm_config));

    config->name = strdup_nt(req->name);
    config->type = req->type;
    config->nr_vcpus = req->vcpus;
    config->mem_size = req->mem_size;
    config->granularity = req->granularity;

    switch (config->type) {
        case LINUX_DIRECT_BOOT_32BIT:
            if (!get_linux_boot_info(mods, req, config)) {
                return false;
            }
            break;
        case MIRROR_VMM:
            break;
        case GUEST_UNSPECIFIED:
            die_reason("Unreachable");
    }

    return true;
}

#undef log_req_error

struct vm_config_vector *
get_vm_configs(const struct limine_module_response *mods)
{
    struct vm_request request;
    struct vm_config config;
    struct vm_config_vector *vm_configs;

    const struct vm_req_vector *requests = get_vm_requests(mods);
    if (!requests) {
        pr_error("Error reading the VM configuration file");
        return NULL;
    }

    const int nr_reqs = size_vm_req_vector(requests);
    if (nr_reqs < 1) {
        pr_error("No VM configuration(s) specified");
        goto nr_reqs_err;
    }

    vm_configs = create_vm_config_vector(nr_reqs);
    if (!vm_configs) {
        goto nr_reqs_err;
    }

    for (int i = 0; i < nr_reqs; i++) {
        at_vm_req_vector(requests, &request, i);

        if (!vm_req_to_vm_config(mods, &request, &config)) {
            /* Deallocate everything we might have already allocated for this entry */
            push_back_vm_config_vector(vm_configs, config);
            goto req_to_conf_err;
        }

        push_back_vm_config_vector(vm_configs, config);
    }

    destroy_vm_reqs(requests);
    return vm_configs;

req_to_conf_err:
    destroy_vm_configs(vm_configs);
nr_reqs_err:
    destroy_vm_reqs(requests);
    return NULL;
}

static inline void
destroy_vm_config(const struct vm_config *config)
{
    if (config->name) {
        hfree(config->name);
    }

    switch (config->type) {
        case LINUX_DIRECT_BOOT_32BIT:
            if (config->boot_info.linux.cmdline_str) {
                hfree(config->boot_info.linux.cmdline_str);
            }
            break;
        case MIRROR_VMM:
            break;
        case GUEST_UNSPECIFIED:
            die_reason("Unreachable");
    }
}

void
destroy_vm_configs(const struct vm_config_vector *configs)
{
    const int nr_configs = size_vm_config_vector(configs);
    struct vm_config config;

    for (int i = 0; i < nr_configs; i++) {
        at_vm_config_vector(configs, &config, i);
        destroy_vm_config(&config);
    }

    destroy_vm_config_vector(configs);
}

#define MEM_GRANULARITY_KB_SHIFT 10
#define MEM_GRANULARITY_MB_SHIFT 20
#define MEM_GRANULARITY_GB_SHIFT 30

uint64_t
get_vm_config_req_bytes(const struct vm_config *config)
{
    switch (config->granularity) {
        case BYTES:
            return config->mem_size;
        case KB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_KB_SHIFT);
        case MB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_MB_SHIFT);
        case GB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_GB_SHIFT);
    }
}

//-----------------------
//-----------------------
//-----------------------
//-----------------------













#define MEM_GRANULARITY_KB_SHIFT 10
#define MEM_GRANULARITY_MB_SHIFT 20
#define MEM_GRANULARITY_GB_SHIFT 30

/* Hardcode for now, but change later */
#define GUEST_CFG_NUMBER_OF_GUESTS 1

static struct guest_config guest_configs[GUEST_CFG_NUMBER_OF_GUESTS] = {
    {
        .name = "vm1",
        .nr_vcpus = 1,
        .mem_size = 1,
        .mem_granularity = GB,
        .guest_type = MIRROR_VMM,
        .bzImage_name = "vm1-bzImage",
        .bzImage_addr = NULL,
        .initramfs_name = "vm1-initramfs",
        .initramfs_addr = NULL,
    },
};

struct guest_config_info
get_guest_configs(const struct limine_module_response *mods)
{
    struct guest_config_info info;

    info.nr_guests = GUEST_CFG_NUMBER_OF_GUESTS;
    info.guest_configs = guest_configs;

    /* todo: !!!do properly, this is just for testing!!! */

    for (unsigned int i = 0; i < info.nr_guests; i++) {
        guest_cfg_t *guest = &guest_configs[i];

        if (guest->guest_type != LINUX_DIRECT_BOOT_32BIT) {
            continue;
        } else if (!mods) {
            die_reason("linux guest but no kernel image provided");
        }

        for (uint64_t j = 0; j < mods->module_count; j++) {
            struct limine_file *mod = mods->modules[j];

            if (strcmp(guest->bzImage_name, mod->string) == 0) {
                guest->bzImage_addr = mod->address;
                guest->bzImage_size = mod->size;
                continue;
            } else if (strcmp(guest->initramfs_name, mod->string) == 0) {
                guest->initramfs_addr = mod->address;
                guest->initramfs_size = mod->size;
                continue;
            }
        }

        if (!guest->bzImage_addr) {
            die_reason("No kernel image found");
        }
    }

    return info;
}

uint64_t
get_req_mem_size_bytes(const struct guest_config *config)
{
    switch (config->mem_granularity) {
        case BYTES:
            return config->mem_size;
        case KB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_KB_SHIFT);
        case MB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_MB_SHIFT);
        case GB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_GB_SHIFT);
        default:
            pr_error("VM configuration specifies unsupported memory granularity");
            return 0;
    }
}

