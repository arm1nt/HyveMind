#include "string.h"
#include "lib/mem_file.h"

#define LINE_BREAK '\n'

enum seek_status
seek_token_in_line(struct mem_file *f, uint64_t *start, uint64_t *end)
{
    uint64_t pos = f->pos;
    uint64_t start_pos = 0;
    bool inside_token = false;
    bool in_quoted_token = false;

    while (!file_oom(f)) {
        const char c = file_at(f, pos);

        if (in_quoted_token) {
            if (c == '\"') {
                f->pos = pos + 1;
                *start = start_pos + 1;
                *end = pos - 1;
                return (*start < *end) ? SEEK_TOKEN : SEEK_INVALID_SYMBOL;
            }

            pos++;
            continue;
        }

        if (!inside_token) {
            if (c == LINE_BREAK) {
                f->pos = pos;
                *start = *end;
                return SEEK_NO_TOKEN;
            } else if (f->ops.is_whitespace(c)) {
                pos++;
                continue;
            } else if (f->ops.is_delim(c)) {
                *start = pos;
                *end = pos;
                f->pos = pos+1;
                return SEEK_TOKEN;
            } else if (f->ops.is_valid_token_char(c)) {
                inside_token = true;
                start_pos = pos;
                pos++;
                continue;
            } else if (c == '\"') {
                in_quoted_token = true;
                start_pos = pos;
                pos++;
                continue;
            } else {
                f->pos = pos;
                return SEEK_INVALID_SYMBOL;
            }
        } else {
            if (c == LINE_BREAK || f->ops.is_whitespace(c) || f->ops.is_delim(c)) {
                *start = start_pos;
                *end = pos-1;
                f->pos = pos;
                return SEEK_TOKEN;
            } else if (f->ops.is_valid_token_char(c)) {
                pos++;
                continue;
            } else {
                f->pos = pos;
                return SEEK_INVALID_SYMBOL;
            }
        }
    }

    f->pos = pos;
    return SEEK_OOM;
}

enum seek_status
__seek_newline(struct mem_file *f, const bool exclusive)
{
    uint64_t pos = f->pos;

    while (!file_oom(f)) {
        if (file_at(f, pos) == LINE_BREAK) {
            pos++;
            break;
        }

        if (exclusive && !f->ops.is_whitespace(file_at(f, pos))) {
            f->pos = pos;
            return SEEK_EXCLUSIVE_VIOLATION;
        }

        pos++;
    }

    f->pos = pos;
    return (!file_oom(f)) ? SEEK_SUCCESS : SEEK_OOM;
}

static enum seek_status
__seek_token(
        struct mem_file *f,
        const char *token,
        const bool exclusive,
        uint64_t *start,
        uint64_t *end
) {
    enum seek_status status;
    uint64_t start_pos = 0, end_pos = 0;

    while ((status = seek_token_in_line(f, &start_pos, &end_pos)) != SEEK_OOM) {

        if (status == SEEK_NO_TOKEN) {
            __seek_newline(f, true);
            continue;
        }

        if (status == SEEK_INVALID_SYMBOL) {
            return status;
        }

        if (token != NULL) {
            const int token_len = strlen(token);
            const int found_len = (end_pos - start_pos) + 1;

            if ((found_len != token_len)
                    || (strncmp(token, &file_at(f, start_pos), token_len) != 0)) {
                if (exclusive) {
                    *start = start_pos;
                    *end = end_pos;
                    return SEEK_EXCLUSIVE_VIOLATION;
                }

                continue;
            }
        }

        *start = start_pos;
        *end = end_pos;
        return SEEK_TOKEN;
    }

    return status;
}

enum seek_status
seek_token(
        struct mem_file *f,
        const char *token,
        const bool exclusive,
        uint64_t *start,
        uint64_t *end
) {
    return __seek_token(f, token, exclusive, start, end);
}

enum seek_status
seek_next_token(struct mem_file *f, uint64_t *start, uint64_t *end)
{
    return __seek_token(f, NULL, false, start, end);
}

