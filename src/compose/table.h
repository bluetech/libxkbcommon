/*
 * Copyright © 2013 Ran Benita <ran234@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef COMPOSE_COMPOSE_H
#define COMPOSE_COMPOSE_H

#include "xkbcommon/xkbcommon-compose.h"
#include "utils.h"
#include "context.h"

struct compose_node {
    /* Offset in xbk_compose_table::sequences. */
    uint32_t sequence:24;
    uint32_t sequence_len:8;
    /* Offset into xkb_compose-table::utf8. */
    uint32_t utf8;
    xkb_keysym_t keysym;
};

struct xkb_compose_table {
    int refcnt;
    enum xkb_compose_format format;
    enum xkb_compose_compile_flags flags;
    struct xkb_context *ctx;

    char *locale;

    darray(struct compose_node) nodes;
    darray(xkb_keysym_t) sequences;
    darray_char utf8;
};

#endif
