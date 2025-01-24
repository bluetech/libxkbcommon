/*
 * For HPND
 * Copyright (c) 1994 by Silicon Graphics Computer Systems, Inc.
 *
 * For MIT:
 * Copyright © 2012 Intel Corporation
 * Copyright © 2012 Ran Benita <ran234@gmail.com>
 *
 * SPDX-License-Identifier: HPND AND MIT
 *
 * Author: Daniel Stone <daniel@fooishbar.org>
 * Author: Ran Benita <ran234@gmail.com>
 */

#include "bump.h"
#include "config.h"

#include "xkbcomp-priv.h"
#include "ast-build.h"
#include "include.h"

static ExprDef *
ExprCreate(struct bump *bump, enum stmt_type op, size_t size)
{
    ExprDef *expr = bump_aligned_alloc(bump, alignof(ExprDef), size);
    if (!expr)
        return NULL;

    expr->common.type = op;
    expr->common.next = NULL;

    return expr;
}

ExprDef *
ExprCreateString(struct bump *bump, xkb_atom_t str)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_STRING_LITERAL, sizeof(ExprString));
    if (!expr)
        return NULL;
    expr->string.str = str;
    return expr;
}

ExprDef *
ExprCreateInteger(struct bump *bump, int ival)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_INTEGER_LITERAL, sizeof(ExprInteger));
    if (!expr)
        return NULL;
    expr->integer.ival = ival;
    return expr;
}

ExprDef *
ExprCreateFloat(struct bump *bump)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_FLOAT_LITERAL, sizeof(ExprFloat));
    if (!expr)
        return NULL;
    return expr;
}

ExprDef *
ExprCreateBoolean(struct bump *bump, bool set)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_BOOLEAN_LITERAL, sizeof(ExprBoolean));
    if (!expr)
        return NULL;
    expr->boolean.set = set;
    return expr;
}

ExprDef *
ExprCreateKeyName(struct bump *bump, xkb_atom_t key_name)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_KEYNAME_LITERAL, sizeof(ExprKeyName));
    if (!expr)
        return NULL;
    expr->key_name.key_name = key_name;
    return expr;
}

ExprDef *
ExprCreateIdent(struct bump *bump, xkb_atom_t ident)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_IDENT, sizeof(ExprIdent));
    if (!expr)
        return NULL;
    expr->ident.ident = ident;
    return expr;
}

ExprDef *
ExprCreateUnary(struct bump *bump, enum stmt_type op, ExprDef *child)
{
    ExprDef *expr = ExprCreate(bump, op, sizeof(ExprUnary));
    if (!expr)
        return NULL;
    expr->unary.child = child;
    return expr;
}

ExprDef *
ExprCreateBinary(struct bump *bump, enum stmt_type op, ExprDef *left, ExprDef *right)
{
    ExprDef *expr = ExprCreate(bump, op, sizeof(ExprBinary));
    if (!expr)
        return NULL;

    expr->binary.left = left;
    expr->binary.right = right;

    return expr;
}

ExprDef *
ExprCreateFieldRef(struct bump *bump, xkb_atom_t element, xkb_atom_t field)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_FIELD_REF, sizeof(ExprFieldRef));
    if (!expr)
        return NULL;
    expr->field_ref.element = element;
    expr->field_ref.field = field;
    return expr;
}

ExprDef *
ExprCreateArrayRef(struct bump *bump, xkb_atom_t element, xkb_atom_t field, ExprDef *entry)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_ARRAY_REF, sizeof(ExprArrayRef));
    if (!expr)
        return NULL;
    expr->array_ref.element = element;
    expr->array_ref.field = field;
    expr->array_ref.entry = entry;
    return expr;
}

ExprDef *
ExprEmptyList(struct bump *bump)
{
    return ExprCreate(bump, STMT_EXPR_EMPTY_LIST, sizeof(ParseCommon));
}

ExprDef *
ExprCreateAction(struct bump *bump, xkb_atom_t name, ExprDef *args)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_ACTION_DECL, sizeof(ExprAction));
    if (!expr)
        return NULL;
    expr->action.name = name;
    expr->action.args = args;
    return expr;
}

ExprDef *
ExprCreateActionList(struct bump *bump, ExprDef *actions)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_ACTION_LIST, sizeof(ExprActionList));
    if (!expr)
        return NULL;
    expr->actions.actions = actions;
    return expr;
}

ExprDef *
ExprCreateKeysymList(struct bump *bump, xkb_keysym_t sym)
{
    ExprDef *expr = ExprCreate(bump, STMT_EXPR_KEYSYM_LIST, sizeof(ExprKeysymList));
    if (!expr)
        return NULL;
    if (sym == XKB_KEY_NoSymbol) {
        /* Discard NoSymbol */
        expr->keysym_list.num_syms = 0;
        expr->keysym_list.syms = NULL;
    } else {
        expr->keysym_list.syms = bump_new(bump, *expr->keysym_list.syms);
        if (!expr->keysym_list.syms) {
            return NULL;
        }
        expr->keysym_list.num_syms = 1;
        expr->keysym_list.syms[0] = sym;
    }
    return expr;
}

ExprDef *
ExprAppendKeysymList(struct bump *bump, ExprDef *expr, xkb_keysym_t sym)
{
    if (sym == XKB_KEY_NoSymbol) {
        /* Discard NoSymbol */
    } else {
        ExprKeysymList *kl = &expr->keysym_list;
        xkb_keysym_t *old = kl->syms;
        kl->syms = bump_aligned_alloc(bump, alignof(xkb_keysym_t), (kl->num_syms + 1) * sizeof(*kl->syms));
        for (unsigned i = 0; i < kl->num_syms; i++)
            kl->syms[i] = old[i];
        kl->syms[kl->num_syms++] = sym;
    }
    return expr;
}

KeycodeDef *
KeycodeCreate(struct bump *bump, xkb_atom_t name, int64_t value)
{
    KeycodeDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_KEYCODE;
    def->common.next = NULL;
    def->name = name;
    def->value = value;

    return def;
}

KeyAliasDef *
KeyAliasCreate(struct bump *bump, xkb_atom_t alias, xkb_atom_t real)
{
    KeyAliasDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_ALIAS;
    def->common.next = NULL;
    def->alias = alias;
    def->real = real;

    return def;
}

VModDef *
VModCreate(struct bump *bump, xkb_atom_t name, ExprDef *value)
{
    VModDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_VMOD;
    def->common.next = NULL;
    def->name = name;
    def->value = value;

    return def;
}

VarDef *
VarCreate(struct bump *bump, ExprDef *name, ExprDef *value)
{
    VarDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_VAR;
    def->common.next = NULL;
    def->name = name;
    def->value = value;

    return def;
}

VarDef *
BoolVarCreate(struct bump *bump, xkb_atom_t ident, bool set)
{
    ExprDef *name, *value;
    VarDef *def;
    if (!(name = ExprCreateIdent(bump, ident))) {
        return NULL;
    }
    if (!(value = ExprCreateBoolean(bump, set))) {
        return NULL;
    }
    if (!(def = VarCreate(bump, name, value))) {
        return NULL;
    }
    return def;
}

InterpDef *
InterpCreate(struct bump *bump, xkb_keysym_t sym, ExprDef *match)
{
    InterpDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_INTERP;
    def->common.next = NULL;
    def->sym = sym;
    def->match = match;
    def->def = NULL;

    return def;
}

KeyTypeDef *
KeyTypeCreate(struct bump *bump, xkb_atom_t name, VarDef *body)
{
    KeyTypeDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_TYPE;
    def->common.next = NULL;
    def->merge = MERGE_DEFAULT;
    def->name = name;
    def->body = body;

    return def;
}

SymbolsDef *
SymbolsCreate(struct bump *bump, xkb_atom_t keyName, VarDef *symbols)
{
    SymbolsDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_SYMBOLS;
    def->common.next = NULL;
    def->merge = MERGE_DEFAULT;
    def->keyName = keyName;
    def->symbols = symbols;

    return def;
}

GroupCompatDef *
GroupCompatCreate(struct bump *bump, unsigned group, ExprDef *val)
{
    GroupCompatDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_GROUP_COMPAT;
    def->common.next = NULL;
    def->merge = MERGE_DEFAULT;
    def->group = group;
    def->def = val;

    return def;
}

ModMapDef *
ModMapCreate(struct bump *bump, xkb_atom_t modifier, ExprDef *keys)
{
    ModMapDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_MODMAP;
    def->common.next = NULL;
    def->merge = MERGE_DEFAULT;
    def->modifier = modifier;
    def->keys = keys;

    return def;
}

LedMapDef *
LedMapCreate(struct bump *bump, xkb_atom_t name, VarDef *body)
{
    LedMapDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_LED_MAP;
    def->common.next = NULL;
    def->merge = MERGE_DEFAULT;
    def->name = name;
    def->body = body;

    return def;
}

LedNameDef *
LedNameCreate(struct bump *bump, unsigned ndx, ExprDef *name, bool virtual)
{
    LedNameDef *def = bump_new(bump, *def);
    if (!def)
        return NULL;

    def->common.type = STMT_LED_NAME;
    def->common.next = NULL;
    def->merge = MERGE_DEFAULT;
    def->ndx = ndx;
    def->name = name;
    def->virtual = virtual;

    return def;
}

IncludeStmt *
IncludeCreate(struct bump *bump, struct xkb_context *ctx, char *str, enum merge_mode merge)
{
    IncludeStmt *incl, *first;
    char *stmt, *tmp;
    char nextop;

    incl = first = NULL;
    tmp = str;
    stmt = str ? bump_strdup(bump, str) : NULL;
    while (tmp && *tmp)
    {
        char *file = NULL, *map = NULL, *extra_data = NULL;

        if (!ParseIncludeMap(bump, &tmp, &file, &map, &nextop, &extra_data))
            goto err;

        /*
         * Given an RMLVO (here layout) like 'us,,fr', the rules parser
         * will give out something like 'pc+us+:2+fr:3+inet(evdev)'.
         * We should just skip the ':2' in this case and leave it to the
         * appropriate section to deal with the empty group.
         */
        if (isempty(file)) {
            continue;
        }

        if (first == NULL) {
            first = incl = bump_new(bump, *first);
        } else {
            incl->next_incl = bump_new(bump, *incl->next_incl);
            incl = incl->next_incl;
        }

        if (!incl) {
            break;
        }

        incl->common.type = STMT_INCLUDE;
        incl->common.next = NULL;
        incl->merge = merge;
        incl->stmt = NULL;
        incl->file = file;
        incl->map = map;
        incl->modifier = extra_data;
        incl->next_incl = NULL;

        if (nextop == MERGE_AUGMENT_PREFIX)
            merge = MERGE_AUGMENT;
        else
            merge = MERGE_OVERRIDE;
    }

    if (first)
        first->stmt = stmt;

    return first;

err:
    log_err(ctx, XKB_ERROR_INVALID_INCLUDE_STATEMENT,
            "Illegal include statement \"%s\"; Ignored\n", stmt);
    return NULL;
}

XkbFile *
XkbFileCreate(struct bump *bump, enum xkb_file_type type, char *name,
              ParseCommon *defs, enum xkb_map_flags flags)
{
    XkbFile *file;

    file = bump_new(bump, *file);
    if (!file)
        return NULL;
    memset(file, 0, sizeof(*file));

    XkbEscapeMapName(name);
    file->bump = bump;
    file->file_type = type;
    if (name) {
        file->name = name;
    } else {
        file->name = bump_strdup(bump, "(unnamed)");
    }
    file->defs = defs;
    file->flags = flags;

    return file;
}

XkbFile *
XkbFileFromComponents(struct bump *bump, struct xkb_context *ctx,
                      const struct xkb_component_names *kkctgs)
{
    char *const components[] = {
        kkctgs->keycodes, kkctgs->types,
        kkctgs->compat, kkctgs->symbols,
    };
    enum xkb_file_type type;
    IncludeStmt *include = NULL;
    XkbFile *file = NULL;
    ParseCommon *defs = NULL, *defsLast = NULL;

    for (type = FIRST_KEYMAP_FILE_TYPE; type <= LAST_KEYMAP_FILE_TYPE; type++) {
        include = IncludeCreate(bump, ctx, components[type], MERGE_DEFAULT);
        if (!include)
            goto err;

        file = XkbFileCreate(bump, type, NULL, (ParseCommon *) include, 0);
        if (!file) {
            goto err;
        }

        if (!defs)
            defsLast = defs = &file->common;
        else
            defsLast = defsLast->next = &file->common;
    }

    file = XkbFileCreate(bump, FILE_TYPE_KEYMAP, NULL, defs, 0);
    if (!file)
        goto err;

    return file;

err:
    return NULL;
}

static const char *xkb_file_type_strings[_FILE_TYPE_NUM_ENTRIES] = {
    [FILE_TYPE_KEYCODES] = "xkb_keycodes",
    [FILE_TYPE_TYPES] = "xkb_types",
    [FILE_TYPE_COMPAT] = "xkb_compatibility",
    [FILE_TYPE_SYMBOLS] = "xkb_symbols",
    [FILE_TYPE_GEOMETRY] = "xkb_geometry",
    [FILE_TYPE_KEYMAP] = "xkb_keymap",
    [FILE_TYPE_RULES] = "rules",
};

const char *
xkb_file_type_to_string(enum xkb_file_type type)
{
    if (type >= _FILE_TYPE_NUM_ENTRIES)
        return "unknown";
    return xkb_file_type_strings[type];
}

static const char *stmt_type_strings[_STMT_NUM_VALUES] = {
    [STMT_UNKNOWN] = "unknown statement",
    [STMT_INCLUDE] = "include statement",
    [STMT_KEYCODE] = "key name definition",
    [STMT_ALIAS] = "key alias definition",
    [STMT_EXPR_STRING_LITERAL] = "string literal expression",
    [STMT_EXPR_INTEGER_LITERAL] = "integer literal expression",
    [STMT_EXPR_FLOAT_LITERAL] = "float literal expression",
    [STMT_EXPR_BOOLEAN_LITERAL] = "boolean literal expression",
    [STMT_EXPR_KEYNAME_LITERAL] = "key name expression",
    [STMT_EXPR_IDENT] = "identifier expression",
    [STMT_EXPR_ACTION_DECL] = "action declaration expression",
    [STMT_EXPR_FIELD_REF] = "field reference expression",
    [STMT_EXPR_ARRAY_REF] = "array reference expression",
    [STMT_EXPR_EMPTY_LIST] = "empty list expression",
    [STMT_EXPR_KEYSYM_LIST] = "keysym list expression",
    [STMT_EXPR_ACTION_LIST] = "action list expression",
    [STMT_EXPR_ADD] = "addition expression",
    [STMT_EXPR_SUBTRACT] = "substraction expression",
    [STMT_EXPR_MULTIPLY] = "multiplication expression",
    [STMT_EXPR_DIVIDE] = "division expression",
    [STMT_EXPR_ASSIGN] = "assignment expression",
    [STMT_EXPR_NOT] = "logical negation expression",
    [STMT_EXPR_NEGATE] = "arithmetic negation expression",
    [STMT_EXPR_INVERT] = "bitwise inversion expression",
    [STMT_EXPR_UNARY_PLUS] = "unary plus expression",
    [STMT_VAR] = "variable definition",
    [STMT_TYPE] = "key type definition",
    [STMT_INTERP] = "symbol interpretation definition",
    [STMT_VMOD] = "virtual modifiers definition",
    [STMT_SYMBOLS] = "key symbols definition",
    [STMT_MODMAP] = "modifier map declaration",
    [STMT_GROUP_COMPAT] = "group declaration",
    [STMT_LED_MAP] = "indicator map declaration",
    [STMT_LED_NAME] = "indicator name declaration",
};

const char *
stmt_type_to_string(enum stmt_type type)
{
    if (type >= _STMT_NUM_VALUES)
        return NULL;
    return stmt_type_strings[type];
}
