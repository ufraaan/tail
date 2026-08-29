/*
 * tail.h — Tail programming language
 *
 * A minimal programming language with reversed function-call syntax.
 *
 *   hello world("print")   ->   print("hello world")
 *   10, 20("add")          ->   add(10, 20)
 *   "hello world"("upper")("print")  ->  print(upper("hello world"))
 *
 * Written entirely from scratch in C11, depending only on the C standard
 * library. No external libraries, no runtime, no interpreter framework.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TAIL_H
#define TAIL_H

#include <stddef.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Values                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
} ValueKind;

typedef struct {
    ValueKind kind;
    union {
        double number;
        struct {
            char *chars;   /* NUL-terminated, owned by this Value */
            size_t len;
        } str;
    } as;
} Value;

/* Create a number value. */
Value value_number(double n);

/* Create a string value from a NUL-terminated buffer (copied). */
Value value_string(const char *s);

/* Create a string value from a buffer with explicit length (copied). */
Value value_string_len(const char *s, size_t len);

/* Deep-copy a value. */
Value value_clone(const Value *v);

/* Release any memory owned by a value. */
void value_free(Value *v);

/* ------------------------------------------------------------------ */
/* Lexer                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    TOK_NUMBER,        /* 10, 3.14 */
    TOK_QUOTED_STRING, /* "hello world" */
    TOK_BARE_STRING,   /* hello world (unquoted text) */
    TOK_COMMA,         /* , */
    TOK_LPAREN,        /* ( */
    TOK_RPAREN,        /* ) */
    TOK_EOF,
} TokenKind;

typedef struct {
    TokenKind kind;
    Value value;       /* payload for NUMBER / STRING tokens */
    int line;
    int col;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t cap;
} TokenList;

/* Tokenize `src` (NUL-terminated). Returns 0 on success, -1 on error.
 * On error, `err_msg` (if non-NULL) is filled with a message and `err_line`
 * / `err_col` receive the location. */
int lex(const char *src, TokenList *out,
        char *err_msg, size_t err_msg_cap, int *err_line, int *err_col);

/* Free all tokens in a list. */
void token_list_free(TokenList *list);

/* ------------------------------------------------------------------ */
/* AST                                                                 */
/* ------------------------------------------------------------------ */

typedef struct CallNode CallNode;

/* A single function application:  args ( "fn" ) */
struct CallNode {
    char *fn_name;        /* owned */
    Value *args;          /* owned array of arg values */
    size_t arg_count;
    CallNode *next;       /* next call in the chain (result feeds into it) */
};

typedef struct {
    CallNode *head;       /* first call of the chain */
} Chain;

/* Parse a token list into a list of chains (one per top-level expression).
 * Returns 0 on success, -1 on error (err_msg filled). */
int parse(const TokenList *tokens, Chain **out_chains, size_t *out_count,
          char *err_msg, size_t err_msg_cap);

/* Free all chains and their contents. */
void chains_free(Chain *chains, size_t count);

/* ------------------------------------------------------------------ */
/* Interpreter                                                         */
/* ------------------------------------------------------------------ */

/* Evaluate all chains, printing results to stdout.
 * Returns 0 on success, -1 on runtime error (err_msg filled). */
int interpret(const Chain *chains, size_t count,
              char *err_msg, size_t err_msg_cap);

#endif /* TAIL_H */
