/*
 * interpreter.c — tree-walking interpreter for Tail.
 *
 * Evaluates each chain by applying its function calls left to right. The
 * result of each call becomes the single argument of the next call in the
 * chain. Built-in functions are resolved from a fixed table.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tail.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Built-in function table                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    /* Returns 0 on success, -1 on error (err_msg filled). */
    int (*fn)(const Value *args, size_t n, Value *out,
              char *err_msg, size_t err_msg_cap);
} Builtin;

static void berr(char *err_msg, size_t cap, const char *msg)
{
    if (err_msg != NULL && cap > 0) {
        snprintf(err_msg, cap, "%s", msg);
    }
}

/* print: identity function. Returns its last argument unchanged. The actual
 * output of a program is produced by the interpreter, which prints the
 * result of every top-level expression. `print` exists to make intent
 * explicit and to be usable inside chains. */
static int fn_print(const Value *args, size_t n, Value *out,
                    char *err_msg, size_t err_msg_cap)
{
    (void)err_msg;
    (void)err_msg_cap;
    if (n == 0) {
        *out = value_string("");
    } else {
        *out = value_clone(&args[n - 1]);
    }
    return 0;
}

/* square: square(n) -> n * n. */
static int fn_square(const Value *args, size_t n, Value *out,
                     char *err_msg, size_t err_msg_cap)
{
    if (n != 1 || args[0].kind != VAL_NUMBER) {
        berr(err_msg, err_msg_cap, "square expects a single number");
        return -1;
    }
    double x = args[0].as.number;
    *out = value_number(x * x);
    return 0;
}

/* add: add(a, b) -> a + b (numbers). */
static int fn_add(const Value *args, size_t n, Value *out,
                  char *err_msg, size_t err_msg_cap)
{
    if (n != 2 || args[0].kind != VAL_NUMBER || args[1].kind != VAL_NUMBER) {
        berr(err_msg, err_msg_cap, "add expects two numbers");
        return -1;
    }
    *out = value_number(args[0].as.number + args[1].as.number);
    return 0;
}

/* concat: concat(a, b) -> a + b (strings). */
static int fn_concat(const Value *args, size_t n, Value *out,
                     char *err_msg, size_t err_msg_cap)
{
    if (n != 2 || args[0].kind != VAL_STRING || args[1].kind != VAL_STRING) {
        berr(err_msg, err_msg_cap, "concat expects two strings");
        return -1;
    }
    size_t total = args[0].as.str.len + args[1].as.str.len;
    char *buf = malloc(total + 1);
    if (buf == NULL) {
        berr(err_msg, err_msg_cap, "out of memory");
        return -1;
    }
    memcpy(buf, args[0].as.str.chars, args[0].as.str.len);
    memcpy(buf + args[0].as.str.len, args[1].as.str.chars, args[1].as.str.len);
    buf[total] = '\0';
    *out = value_string_len(buf, total);
    free(buf);
    return 0;
}

/* upper: upper(s) -> uppercase copy of s. */
static int fn_upper(const Value *args, size_t n, Value *out,
                    char *err_msg, size_t err_msg_cap)
{
    if (n != 1 || args[0].kind != VAL_STRING) {
        berr(err_msg, err_msg_cap, "upper expects a single string");
        return -1;
    }
    const char *s = args[0].as.str.chars;
    size_t len = args[0].as.str.len;
    char *buf = malloc(len + 1);
    if (buf == NULL) {
        berr(err_msg, err_msg_cap, "out of memory");
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = (char)toupper((unsigned char)s[i]);
    }
    buf[len] = '\0';
    *out = value_string_len(buf, len);
    free(buf);
    return 0;
}

static const Builtin builtins[] = {
    { "print",  fn_print  },
    { "square", fn_square },
    { "add",    fn_add    },
    { "concat", fn_concat },
    { "upper",  fn_upper  },
};

static const size_t builtin_count = sizeof(builtins) / sizeof(builtins[0]);

static const Builtin *find_builtin(const char *name)
{
    for (size_t i = 0; i < builtin_count; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return &builtins[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Chain evaluation                                                    */
/* ------------------------------------------------------------------ */

/* Print a value to stdout followed by a newline. */
static void print_value(const Value *v)
{
    if (v->kind == VAL_NUMBER) {
        double d = v->as.number;
        if (d == (double)(long long)d) {
            printf("%lld\n", (long long)d);
        } else {
            printf("%g\n", d);
        }
    } else {
        fwrite(v->as.str.chars, 1, v->as.str.len, stdout);
        putchar('\n');
    }
}

int interpret(const Chain *chains, size_t count,
              char *err_msg, size_t err_msg_cap)
{
    for (size_t c = 0; c < count; c++) {
        const CallNode *node = chains[c].head;
        if (node == NULL) {
            continue;
        }

        /* Evaluate the first call with its parsed arguments. */
        const Builtin *fn = find_builtin(node->fn_name);
        if (fn == NULL) {
            if (err_msg != NULL && err_msg_cap > 0) {
                snprintf(err_msg, err_msg_cap, "unknown function: %s",
                         node->fn_name);
            }
            return -1;
        }

        Value result;
        if (fn->fn(node->args, node->arg_count, &result, err_msg, err_msg_cap) != 0) {
            return -1;
        }

        /* Chain: each subsequent call receives the previous result. */
        node = node->next;
        while (node != NULL) {
            const Builtin *next_fn = find_builtin(node->fn_name);
            if (next_fn == NULL) {
                value_free(&result);
                if (err_msg != NULL && err_msg_cap > 0) {
                    snprintf(err_msg, err_msg_cap, "unknown function: %s",
                             node->fn_name);
                }
                return -1;
            }

            Value next_result;
            if (next_fn->fn(&result, 1, &next_result, err_msg, err_msg_cap) != 0) {
                value_free(&result);
                return -1;
            }
            value_free(&result);
            result = next_result;
            node = node->next;
        }

        /* Print the result of this top-level expression. */
        print_value(&result);
        value_free(&result);
    }

    return 0;
}
