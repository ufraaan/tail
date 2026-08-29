/*
 * lexer.c — tokenizer for Tail.
 *
 * Converts raw source text into a flat list of tokens:
 *
 *   NUMBER, QUOTED_STRING, BARE_STRING, COMMA, LPAREN, RPAREN, EOF
 *
 * Bare text (unquoted) is treated as a string literal, per the language
 * design. A bare atom ends at a comma, an opening parenthesis, or a newline.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tail.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Token list helpers                                                  */
/* ------------------------------------------------------------------ */

static int token_list_push(TokenList *list, Token tok)
{
    if (list->count == list->cap) {
        size_t new_cap = list->cap == 0 ? 16 : list->cap * 2;
        Token *new_items = realloc(list->items, new_cap * sizeof(Token));
        if (new_items == NULL) {
            return -1;
        }
        list->items = new_items;
        list->cap = new_cap;
    }
    list->items[list->count++] = tok;
    return 0;
}

void token_list_free(TokenList *list)
{
    if (list->items == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        Token *t = &list->items[i];
        if (t->kind == TOK_NUMBER || t->kind == TOK_QUOTED_STRING ||
            t->kind == TOK_BARE_STRING) {
            value_free(&t->value);
        }
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

/* ------------------------------------------------------------------ */
/* Lexer                                                               */
/* ------------------------------------------------------------------ */

static void set_err(char *err_msg, size_t cap, const char *msg)
{
    if (err_msg != NULL && cap > 0) {
        snprintf(err_msg, cap, "%s", msg);
    }
}

int lex(const char *src, TokenList *out,
        char *err_msg, size_t err_msg_cap, int *err_line, int *err_col)
{
    memset(out, 0, sizeof(*out));

    const char *p = src;
    int line = 1;
    int col = 1;

    for (;;) {
        /* Skip whitespace (spaces, tabs, CR). Newlines separate expressions
         * but are otherwise insignificant. */
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
            col++;
        }

        if (*p == '\n') {
            p++;
            line++;
            col = 1;
            continue;
        }

        /* Comment: '#' to end of line. */
        if (*p == '#') {
            while (*p != '\0' && *p != '\n') {
                p++;
                col++;
            }
            continue;
        }

        if (*p == '\0') {
            Token eof;
            eof.kind = TOK_EOF;
            eof.line = line;
            eof.col = col;
            eof.value.kind = VAL_NUMBER;
            eof.value.as.number = 0;
            if (token_list_push(out, eof) != 0) {
                set_err(err_msg, err_msg_cap, "out of memory");
                return -1;
            }
            return 0;
        }

        /* Comma */
        if (*p == ',') {
            Token t;
            t.kind = TOK_COMMA;
            t.line = line;
            t.col = col;
            t.value.kind = VAL_NUMBER;
            t.value.as.number = 0;
            if (token_list_push(out, t) != 0) {
                set_err(err_msg, err_msg_cap, "out of memory");
                return -1;
            }
            p++;
            col++;
            continue;
        }

        /* Parentheses */
        if (*p == '(') {
            Token t;
            t.kind = TOK_LPAREN;
            t.line = line;
            t.col = col;
            t.value.kind = VAL_NUMBER;
            t.value.as.number = 0;
            if (token_list_push(out, t) != 0) {
                set_err(err_msg, err_msg_cap, "out of memory");
                return -1;
            }
            p++;
            col++;
            continue;
        }

        if (*p == ')') {
            Token t;
            t.kind = TOK_RPAREN;
            t.line = line;
            t.col = col;
            t.value.kind = VAL_NUMBER;
            t.value.as.number = 0;
            if (token_list_push(out, t) != 0) {
                set_err(err_msg, err_msg_cap, "out of memory");
                return -1;
            }
            p++;
            col++;
            continue;
        }

        /* Quoted string */
        if (*p == '"') {
            int start_line = line;
            int start_col = col;
            p++;
            col++;

            /* First pass: measure the decoded length. */
            size_t len = 0;
            const char *q = p;
            while (*q != '\0' && *q != '"') {
                if (*q == '\\') {
                    q++;
                    if (*q == '\0') {
                        break;
                    }
                    q++;
                } else {
                    q++;
                }
                len++;
            }
            if (*q != '"') {
                if (err_line) *err_line = start_line;
                if (err_col) *err_col = start_col;
                set_err(err_msg, err_msg_cap, "unterminated string literal");
                return -1;
            }

            /* Second pass: decode. */
            char *buf = malloc(len + 1);
            if (buf == NULL) {
                set_err(err_msg, err_msg_cap, "out of memory");
                return -1;
            }
            size_t i = 0;
            while (*p != '"') {
                if (*p == '\\') {
                    p++;
                    col++;
                    switch (*p) {
                    case 'n': buf[i++] = '\n'; break;
                    case 't': buf[i++] = '\t'; break;
                    case 'r': buf[i++] = '\r'; break;
                    case '\\': buf[i++] = '\\'; break;
                    case '"': buf[i++] = '"'; break;
                    case '0': buf[i++] = '\0'; break;
                    default:  buf[i++] = *p; break;
                    }
                    p++;
                    col++;
                } else {
                    buf[i++] = *p;
                    p++;
                    col++;
                }
            }
            buf[i] = '\0';
            /* Skip closing quote. */
            p++;
            col++;

            Token t;
            t.kind = TOK_QUOTED_STRING;
            t.line = start_line;
            t.col = start_col;
            t.value = value_string_len(buf, i);
            free(buf);
            if (token_list_push(out, t) != 0) {
                set_err(err_msg, err_msg_cap, "out of memory");
                return -1;
            }
            continue;
        }

        /* Bare string or number. A bare atom runs until a comma, an opening
         * parenthesis, or end of line. Leading/trailing whitespace is
         * trimmed. */
        {
            int start_line = line;
            int start_col = col;
            const char *start = p;

            while (*p != '\0' && *p != ',' && *p != '(' && *p != '\n' &&
                   *p != '\r') {
                p++;
                col++;
            }

            /* Trim trailing whitespace. */
            const char *end = p;
            while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
                end--;
            }

            size_t len = (size_t)(end - start);

            /* Determine if it's a number. */
            bool is_number = len > 0;
            bool seen_dot = false;
            for (size_t i = 0; i < len; i++) {
                char c = start[i];
                if (c == '.') {
                    if (seen_dot) { is_number = false; break; }
                    seen_dot = true;
                } else if (!isdigit((unsigned char)c)) {
                    is_number = false;
                    break;
                }
            }

            if (is_number) {
                char *tmp = malloc(len + 1);
                if (tmp == NULL) {
                    set_err(err_msg, err_msg_cap, "out of memory");
                    return -1;
                }
                memcpy(tmp, start, len);
                tmp[len] = '\0';
                Token t;
                t.kind = TOK_NUMBER;
                t.line = start_line;
                t.col = start_col;
                t.value = value_number(strtod(tmp, NULL));
                free(tmp);
                if (token_list_push(out, t) != 0) {
                    set_err(err_msg, err_msg_cap, "out of memory");
                    return -1;
                }
            } else {
                Token t;
                t.kind = TOK_BARE_STRING;
                t.line = start_line;
                t.col = start_col;
                t.value = value_string_len(start, len);
                if (token_list_push(out, t) != 0) {
                    set_err(err_msg, err_msg_cap, "out of memory");
                    return -1;
                }
            }
        }
    }
}
