/*
 * Copyright © 2012 Ran Benita <ran234@gmail.com>
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

#include "config.h"

#include "xkbcomp-priv.h"
#include "parser-priv.h"
#include "scanner-utils.h"

int
_xkbcommon_lex(YYSTYPE *yylval, struct scanner *s)
{
    s->token_pos = s->pos;
    start: {
        char ch = scanner_next(s);
        /* Clean EOF. */
        if (ch == '\0') {
            return END_OF_FILE;
        }
        /* Whitespace. */
        else if (is_space(ch)) {
            s->token_pos = s->pos;
            goto start;
        }
        /* Comment. */
        else if (ch == '#' || (ch == '/' && scanner_peek(s) == '/')) {
            goto comment;
        }
        /* String literal. */
        else if (ch == '\"') {
            s->buf_pos = 0;
            goto string;
        }
        /* Key name literal. */
        else if (ch == '<') {
            goto keyname;
        }
        /* Operator or punctuation. */
        else if (ch == ';') { return SEMI; }
        else if (ch == '{') { return OBRACE; }
        else if (ch == '}') { return CBRACE; }
        else if (ch == '=') { return EQUALS; }
        else if (ch == '[') { return OBRACKET; }
        else if (ch == ']') { return CBRACKET; }
        else if (ch == '(') { return OPAREN; }
        else if (ch == ')') { return CPAREN; }
        else if (ch == '.') { return DOT; }
        else if (ch == ',') { return COMMA; }
        else if (ch == '+') { return PLUS; }
        else if (ch == '-') { return MINUS; }
        else if (ch == '*') { return TIMES; }
        else if (ch == '/') { return DIVIDE; }
        else if (ch == '!') { return EXCLAM; }
        else if (ch == '~') { return INVERT; }
        /* Identifier or keyword. */
        else if (is_alpha(ch) || ch == '_') {
            goto identifier;
        }
        /* Number literal. */
        else if (is_digit(ch)) {
            s->buf_pos = 0;
            scanner_buf_append(s, ch);
            goto number;
        }
        /* Unrecognized character. */
        else {
            scanner_err(s, XKB_LOG_MESSAGE_NO_ID, "unrecognized token");
            return ERROR_TOK;
        }
    }

    comment: {
        scanner_skip_to_eol(s);
        goto start;
    }

    string: {
        char ch = scanner_next(s);
        /* Closing quote. */
        if (ch == '\"') {
            if (!scanner_buf_append(s, '\0')) {
                scanner_err(s, XKB_LOG_MESSAGE_NO_ID,
                            "string literal too long");
            }
            yylval->str = strdup(s->buf);
            if (!yylval->str) {
                return ERROR_TOK;
            }
            return STRING;
        }
        /* Escape sequence. */
        else if (ch == '\\') {
            goto string_esc;
        }
        /* Newline or EOF. */
        else if (ch == '\n' || ch == '\0') {
            scanner_err(s, XKB_LOG_MESSAGE_NO_ID,
                        "unterminated string literal");
            return ERROR_TOK;
        }
        /* Normal character. */
        else {
            scanner_buf_append(s, ch);
            goto string;
        }
    }

    string_esc: {
        char ch = scanner_next(s);
        if (ch == '\\') scanner_buf_append(s, '\\');
        else if (ch == 'n') scanner_buf_append(s, '\n');
        else if (ch == 't') scanner_buf_append(s, '\t');
        else if (ch == 'r') scanner_buf_append(s, '\r');
        else if (ch == 'b') scanner_buf_append(s, '\b');
        else if (ch == 'f') scanner_buf_append(s, '\f');
        else if (ch == 'v') scanner_buf_append(s, '\v');
        else if (ch == 'e') scanner_buf_append(s, '\033');
        else if (is_digit(ch)) {
            s->pos--;
            size_t start_pos = s->pos;
            uint8_t o;
            if (!scanner_oct(s, &o) || !is_valid_char((char) o)) {
                scanner_warn(s, XKB_WARNING_INVALID_ESCAPE_SEQUENCE,
                             "invalid octal escape sequence (%.*s) in string literal",
                             (int) (s->pos - start_pos + 1),
                             s->s + start_pos - 1);
                /* Ignore. */
            } else {
                scanner_buf_append(s, (char) o);
            }
        }
        else {
            scanner_warn(s, XKB_WARNING_UNKNOWN_CHAR_ESCAPE_SEQUENCE,
                        "unknown escape sequence (\\%c) in string literal", ch);
            /* Ignore. */
        }
        goto string;
    }

    keyname: {
        char ch = scanner_next(s);
        /* Closing quite. */
        if (ch == '>') {
            const char *start = s->s + s->token_pos + 1;
            size_t len = s->pos - s->token_pos - 2;
            yylval->atom = xkb_atom_intern(s->ctx, start, len);
            return KEYNAME;
        }
        /* Normal character. */
        else if (is_graph(ch) && ch != '>') {
            goto keyname;
        }
        /* Unallowed character or unexpected EOF. */
        else {
            s->pos--;
            scanner_err(s, XKB_LOG_MESSAGE_NO_ID,
                        "unterminated key name literal");
            return ERROR_TOK;
        }
    }

    identifier: {
        char ch = scanner_next(s);
        /* Identifier character. */
        if (is_alnum(ch) || ch == '_') {
            goto identifier;
        }
        /* Terminate identifier. */
        else {
            s->pos--;
            const char *start = s->s + s->token_pos;
            size_t len = s->pos - s->token_pos;
            int keyword = keyword_to_token(start, len);
            if (keyword >= 0)
                return keyword;
            yylval->str = strndup(start, len);
            if (!yylval->str)
                return ERROR_TOK;
            return IDENT;
        }
    }

    number: {
        char ch = scanner_next(s);
        /* Digit. */
        if (is_alnum(ch)) {
            scanner_buf_append(s, ch);
            goto number;
        }
        /* Decimal separator. */
        else if (ch == '.') {
            scanner_buf_append(s, ch);
            goto number_float;
        }
        /* Terminate number. */
        else {
            s->pos--;
            if (!scanner_buf_append(s, '\0')) {
                scanner_err(s, XKB_ERROR_MALFORMED_NUMBER_LITERAL,
                            "number literal too long");
                return ERROR_TOK;
            }
            errno = 0;
            char *endptr;
            int base = (s->buf_pos > 2 && s->buf[0] == '0' && s->buf[1] == 'x') ? 16 : 10;
            int64_t val = strtoul(s->buf, &endptr, base);
            if (endptr != s->buf + s->buf_pos - 1 || errno != 0) {
                scanner_err(s, XKB_ERROR_MALFORMED_NUMBER_LITERAL,
                            "malformed number literal");
                return ERROR_TOK;
            }
            yylval->num = val;
            return INTEGER;
        }
    }

    number_float: {
        char ch = scanner_next(s);
        /* Digit. */
        if (is_alnum(ch)) {
            scanner_buf_append(s, ch);
            goto number_float;
        }
        /* Terminate number. */
        else {
            s->pos--;
            if (!scanner_buf_append(s, '\0')) {
                scanner_err(s, XKB_ERROR_MALFORMED_NUMBER_LITERAL,
                            "number literal too long");
                return ERROR_TOK;
            }
            errno = 0;
            char *endptr;
            double val = strtod(s->buf, &endptr);
            if (endptr != s->buf + s->buf_pos - 1 || errno != 0) {
                scanner_err(s, XKB_ERROR_MALFORMED_NUMBER_LITERAL,
                            "malformed number literal");
                return ERROR_TOK;
            }
            /* The parser currently just ignores floats, so the cast is
                * fine - the value doesn't matter. */
            yylval->num = (int64_t) val;
            return FLOAT;
        }
    }
}

XkbFile *
XkbParseString(struct xkb_context *ctx, const char *string, size_t len,
               const char *file_name, const char *map)
{
    struct scanner scanner;
    scanner_init(&scanner, ctx, string, len, file_name, NULL);

    /* Basic detection of wrong character encoding.
       The first character relevant to the grammar must be ASCII:
       whitespace, section, comment */
    if (!scanner_check_supported_char_encoding(&scanner)) {
        scanner_err(&scanner, XKB_LOG_MESSAGE_NO_ID,
                    "This could be a file encoding issue. "
                    "Supported encodings must be backward compatible with ASCII.");
        scanner_err(&scanner, XKB_LOG_MESSAGE_NO_ID,
                    "E.g. ISO/CEI 8859 and UTF-8 are supported "
                    "but UTF-16, UTF-32 and CP1026 are not.");
        return NULL;
    }

    return parse(ctx, &scanner, map);
}

XkbFile *
XkbParseFile(struct xkb_context *ctx, FILE *file,
             const char *file_name, const char *map)
{
    bool ok;
    XkbFile *xkb_file;
    char *string;
    size_t size;

    ok = map_file(file, &string, &size);
    if (!ok) {
        log_err(ctx, XKB_LOG_MESSAGE_NO_ID,
                "Couldn't read XKB file %s: %s\n",
                file_name, strerror(errno));
        return NULL;
    }

    xkb_file = XkbParseString(ctx, string, size, file_name, map);
    unmap_file(string, size);
    return xkb_file;
}
