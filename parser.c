/*
 * parser.c — recursive-descent parser for Tail.
 *
 * Grammar:
 *
 *   program  := chain+
 *   chain    := primary { "(" string ")" }
 *   primary  := atom { "," atom }
 *   atom     := NUMBER | QUOTED_STRING | BARE_STRING
 *
 * A chain is a comma-separated argument list followed by one or more
 * function applications. Each application's result becomes the single
 * argument of the next application in the chain.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tail.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Token *tokens;
    size_t count;
    size_t pos;
    char *err_msg;
    size_t err_msg_cap;
} Parser;

static void perr(Parser *p, const char *msg)
{
    if (p->err_msg != NULL && p->err_msg_cap > 0) {
        snprintf(p->err_msg, p->err_msg_cap, "%s", msg);
    }
}

static const Token *peek(Parser *p)
{
    return &p->tokens[p->pos];
}

static const Token *advance(Parser *p)
{
    const Token *t = &p->tokens[p->pos];
    if (p->pos < p->count - 1) {
        p->pos++;
    }
    return t;
}

static bool is_atom(const Token *t)
{
    return t->kind == TOK_NUMBER || t->kind == TOK_QUOTED_STRING ||
           t->kind == TOK_BARE_STRING;
}

/* Parse a single atom (number or string). */
static bool parse_atom(Parser *p, Value *out)
{
    const Token *t = peek(p);
    if (!is_atom(t)) {
        perr(p, "expected a value");
        return false;
    }
    *out = value_clone(&t->value);
    advance(p);
    return true;
}

/* Parse a primary: atom { "," atom }. */
static bool parse_primary(Parser *p, Value **args, size_t *arg_count)
{
    size_t cap = 4;
    size_t count = 0;
    Value *arr = malloc(cap * sizeof(Value));
    if (arr == NULL) {
        perr(p, "out of memory");
        return false;
    }

    if (!parse_atom(p, &arr[count++])) {
        free(arr);
        return false;
    }

    while (peek(p)->kind == TOK_COMMA) {
        advance(p);
        if (count == cap) {
            cap *= 2;
            Value *na = realloc(arr, cap * sizeof(Value));
            if (na == NULL) {
                for (size_t i = 0; i < count; i++) value_free(&arr[i]);
                free(arr);
                perr(p, "out of memory");
                return false;
            }
            arr = na;
        }
        if (!parse_atom(p, &arr[count++])) {
            for (size_t i = 0; i < count; i++) value_free(&arr[i]);
            free(arr);
            return false;
        }
    }

    *args = arr;
    *arg_count = count;
    return true;
}

/* Parse a chain: primary { "(" string ")" }. */
static bool parse_chain(Parser *p, Chain *out)
{
    Value *args = NULL;
    size_t arg_count = 0;

    if (!parse_primary(p, &args, &arg_count)) {
        return false;
    }

    CallNode *head = NULL;
    CallNode *tail = NULL;

    while (peek(p)->kind == TOK_LPAREN) {
        advance(p);

        /* Function name: a string token (quoted or bare). */
        const Token *name = peek(p);
        if (name->kind != TOK_QUOTED_STRING && name->kind != TOK_BARE_STRING) {
            perr(p, "expected a function name inside parentheses");
            goto fail;
        }
        advance(p);

        if (peek(p)->kind != TOK_RPAREN) {
            perr(p, "expected ')' after function name");
            goto fail;
        }
        advance(p);

        CallNode *node = calloc(1, sizeof(CallNode));
        if (node == NULL) {
            perr(p, "out of memory");
            goto fail;
        }
        node->fn_name = strdup(name->value.as.str.chars);
        node->args = args;
        node->arg_count = arg_count;
        node->next = NULL;

        if (head == NULL) {
            head = node;
        } else {
            tail->next = node;
        }
        tail = node;

        /* The result of this call becomes the single argument of the next. */
        args = NULL;
        arg_count = 0;
        Value *single = malloc(sizeof(Value));
        if (single == NULL) {
            perr(p, "out of memory");
            goto fail;
        }
        single[0].kind = VAL_NUMBER;
        single[0].as.number = 0; /* placeholder; filled by interpreter */
        args = single;
        arg_count = 1;
    }

    /* Free the trailing placeholder args (they are not owned by any node). */
    if (args != NULL) {
        free(args);
    }

    out->head = head;
    return true;

fail:
    if (args != NULL) {
        for (size_t i = 0; i < arg_count; i++) value_free(&args[i]);
        free(args);
    }
    while (head != NULL) {
        CallNode *n = head;
        head = n->next;
        free(n->fn_name);
        if (n->args != NULL) {
            for (size_t i = 0; i < n->arg_count; i++) value_free(&n->args[i]);
            free(n->args);
        }
        free(n);
    }
    return false;
}

int parse(const TokenList *tokens, Chain **out_chains, size_t *out_count,
          char *err_msg, size_t err_msg_cap)
{
    Parser p;
    p.tokens = tokens->items;
    p.count = tokens->count;
    p.pos = 0;
    p.err_msg = err_msg;
    p.err_msg_cap = err_msg_cap;

    size_t cap = 8;
    size_t count = 0;
    Chain *chains = malloc(cap * sizeof(Chain));
    if (chains == NULL) {
        perr(&p, "out of memory");
        return -1;
    }

    while (peek(&p)->kind != TOK_EOF) {
        if (count == cap) {
            cap *= 2;
            Chain *nc = realloc(chains, cap * sizeof(Chain));
            if (nc == NULL) {
                perr(&p, "out of memory");
                chains_free(chains, count);
                return -1;
            }
            chains = nc;
        }
        if (!parse_chain(&p, &chains[count])) {
            chains_free(chains, count);
            free(chains);
            return -1;
        }
        count++;
    }

    *out_chains = chains;
    *out_count = count;
    return 0;
}

void chains_free(Chain *chains, size_t count)
{
    if (chains == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        CallNode *n = chains[i].head;
        while (n != NULL) {
            CallNode *next = n->next;
            free(n->fn_name);
            if (n->args != NULL) {
                for (size_t j = 0; j < n->arg_count; j++) {
                    value_free(&n->args[j]);
                }
                free(n->args);
            }
            free(n);
            n = next;
        }
    }
    free(chains);
}
