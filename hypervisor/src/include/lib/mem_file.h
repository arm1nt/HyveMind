#ifndef _HYVEMIND_LIB_MEM_FILE_H
#define _HYVEMIND_LIB_MEM_FILE_H

#include "hyvstdlib.h"

/* For now, separating the tokens like this is sufficient */
struct mem_file_ops {
    bool (*is_whitespace)(const char c);
    bool (*is_delim)(const char c);
    bool (*is_valid_token_char)(const char c);
};

struct mem_file {
    char *f;
    uint64_t pos;
    uint64_t size;
    struct mem_file_ops ops;
};

#define file_at(_f, pos) ((_f)->f[(pos)])
#define file_curr(_f) ((_f)->f[(_f)->pos])
#define file_oom(_f) ((_f)->pos >= (_f)->size)

enum seek_status {
    SEEK_OOM,
    SEEK_EXCLUSIVE_VIOLATION,
    SEEK_INVALID_SYMBOL,
    SEEK_NO_TOKEN,
    SEEK_TOKEN,
    SEEK_SUCCESS,
};

/**
 * Seek the next token in the current line.
 *
 * The file cursor position after this function depends on the return status:
 * - SEEK_OOM: f->pos is out of bounds (pointing to the first char after the file)
 * - SEEK_TOKEN: f->pos points to the char right after the token (possibly OOB)
 * - SEEK_NO_TOKEN: f->pos points to the line-terminating new-line char
 * - SEEK_INVALID_SYMBOL: f->pos points to the invalid symbol
 */
enum seek_status seek_token_in_line(struct mem_file *f, uint64_t *start, uint64_t *end);

/**
 * Positions the file cursor at the beginning of the next line.
 *
 * If 'exclusive==true' we return an EXCLUSIVE_VIOLATION if we find any
 * non-whitespace characters while moving to the next line.
 *
 * The file cursor position after the function depends on the return status:
 * - SEEK_OOM: f->ops is out of bounds
 * - SEEK_EXCLUSIVE_VIOLATION: f->ops points to the violating char
 * - SEEK_SUCCESS: f->ops points to the beginning of the new line
 */
enum seek_status __seek_newline(struct mem_file *f, const bool exclusive);
#define seek_newline(f) __seek_newline((f), true)

/**
 * Seeks the next occurence of 'token' in the file, possibly searching multiple
 * lines.
 *
 * @token ... the token we are searching for
 * @exclusive ... if true, we return an EXCLUSIVE_VIOLATION if we find any token
 *                  before the targeted @token.
 *
 * The file cursor position after the function depends on the return status:
 * - SEEK_OOM: f->ops is out of bounds
 * - SEEK_INVALID_SYMBOL: f->ops points to the invalid symbol
 * - SEEK_EXCLUSIVE_VIOLATION: f->ops points behind the violating token and
 *          start & end hold the positions of the violating token
 * - SEEK_TOKEN: f->ops points right behind the found token.
 */
enum seek_status seek_token(
        struct mem_file *f,
        const char *token,
        const bool exclusive,
        uint64_t *start,
        uint64_t *end
);

/**
 * Seeks the next token in the file, possibly searching multiple lines.
 *
 * Thee file cursor position after the function depends on the return status:
 *
 * - SEEK_OOM: f->ops is out of bounds
 * - SEEK_INVALID_SYMBOL: f->ops points to the invalid symbol
 * - SEEK_TOKEN: f->ops points right behind the found token (possibly OOB)
 */
enum seek_status seek_next_token(struct mem_file *f, uint64_t *start, uint64_t *end);

#endif /* _HYVEMIND_LIB_MEM_FILE_H */

