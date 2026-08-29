/*
 * main.c — Tail command-line interface.
 *
 * Usage:
 *   ./tail file.tail      run a .tail source file
 *   ./tail < file.tail    read source from stdin
 *
 * SPDX-License-Identifier: MIT
 */

#include "tail.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "tail: cannot open '%s'\n", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        fprintf(stderr, "tail: cannot read '%s'\n", path);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        fprintf(stderr, "tail: cannot read '%s'\n", path);
        return NULL;
    }
    rewind(f);

    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) {
        fclose(f);
        fprintf(stderr, "tail: out of memory\n");
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';
    *out_len = n;
    return buf;
}

static char *read_stdin(size_t *out_len)
{
    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);
    if (buf == NULL) {
        fprintf(stderr, "tail: out of memory\n");
        return NULL;
    }

    int ch;
    while ((ch = fgetc(stdin)) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (nb == NULL) {
                free(buf);
                fprintf(stderr, "tail: out of memory\n");
                return NULL;
            }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

static int run_source(const char *src)
{
    TokenList tokens;
    char err[512];
    int err_line = 0, err_col = 0;

    if (lex(src, &tokens, err, sizeof(err), &err_line, &err_col) != 0) {
        fprintf(stderr, "tail: lex error");
        if (err_line > 0) fprintf(stderr, " at line %d, col %d", err_line, err_col);
        fprintf(stderr, ": %s\n", err);
        return 1;
    }

    Chain *chains = NULL;
    size_t chain_count = 0;
    if (parse(&tokens, &chains, &chain_count, err, sizeof(err)) != 0) {
        fprintf(stderr, "tail: parse error: %s\n", err);
        token_list_free(&tokens);
        return 1;
    }
    token_list_free(&tokens);

    if (interpret(chains, chain_count, err, sizeof(err)) != 0) {
        fprintf(stderr, "tail: runtime error: %s\n", err);
        chains_free(chains, chain_count);
        return 1;
    }

    chains_free(chains, chain_count);
    return 0;
}

int main(int argc, char **argv)
{
    char *src = NULL;
    size_t len = 0;

    if (argc > 2) {
        fprintf(stderr, "usage: %s [file.tail]\n", argv[0]);
        return 2;
    }

    if (argc == 2) {
        src = read_file(argv[1], &len);
        if (src == NULL) {
            return 1;
        }
    } else {
        src = read_stdin(&len);
        if (src == NULL) {
            return 1;
        }
    }

    int rc = run_source(src);
    free(src);
    return rc;
}
