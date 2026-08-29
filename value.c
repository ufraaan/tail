/*
 * value.c — dynamic Value type for Tail.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tail.h"

#include <stdlib.h>
#include <string.h>

Value value_number(double n)
{
    Value v;
    v.kind = VAL_NUMBER;
    v.as.number = n;
    return v;
}

Value value_string(const char *s)
{
    return value_string_len(s, strlen(s));
}

Value value_string_len(const char *s, size_t len)
{
    Value v;
    v.kind = VAL_STRING;
    v.as.str.len = len;
    v.as.str.chars = malloc(len + 1);
    if (v.as.str.chars == NULL) {
        /* Out of memory: represent as empty string to keep things simple. */
        v.as.str.chars = malloc(1);
        if (v.as.str.chars != NULL) {
            v.as.str.chars[0] = '\0';
        }
        v.as.str.len = 0;
        return v;
    }
    memcpy(v.as.str.chars, s, len);
    v.as.str.chars[len] = '\0';
    return v;
}

Value value_clone(const Value *v)
{
    if (v->kind == VAL_NUMBER) {
        return value_number(v->as.number);
    }
    return value_string_len(v->as.str.chars, v->as.str.len);
}

void value_free(Value *v)
{
    if (v->kind == VAL_STRING && v->as.str.chars != NULL) {
        free(v->as.str.chars);
        v->as.str.chars = NULL;
        v->as.str.len = 0;
    }
}
