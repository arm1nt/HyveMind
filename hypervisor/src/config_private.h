#ifndef _HYVEMIND_CONFIG_PRIVATE_H
#define _HYVEMIND_CONFIG_PRIVATE_H

#include "vm_config_defs.h"
#include "lib/mem_file.h"

struct vm_request;
typedef struct mem_file config_file_t;

enum config_parsing_status {
    PARSING_SUCCESS,
    PARSING_ERR_INVALID_ENTRY,
    PARSING_ERR_MALFORMED_FILE,
};

enum token_state {
    TOKEN_SEARCH_START_MARKER,
    TOKEN_SEARCH_CONFIG_ENTRY_START,
    TOKEN_SEARCH_ENTRY_DELIM,
    TOKEN_SEARCH_ENTRY_VAL
};

enum parsing_state {
    PS_CONTINUE,
    PS_DONE_SUCCESS,
    PS_STOP_ERROR,
    PS_CONFIG_ENTRY_COMPLETED,
};

struct config_line {
    enum config_key key;
    char *value;
};

#define token_len(state) (((state)->token_end - (state)->token_start) + 1)
#define token_ptr(state) &file_at((state)->file, (state)->token_start)

struct parser_state {
    config_file_t *file;

    struct vm_request *curr_request;
    struct config_line *curr_line;

    enum token_state token_state;

    enum seek_status token_seek_status;
    uint64_t token_start;
    uint64_t token_end;
};

#endif /* _HYVEMIND_CONFIG_PRIVATE_H */

