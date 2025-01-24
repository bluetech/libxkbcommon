/*
 * Copyright (c) 1994 by Silicon Graphics Computer Systems, Inc.
 * SPDX-License-Identifier: HPND
 */
#pragma once

#include "bump.h"
#include "ast.h"

ExprDef *
ExprCreateString(struct bump *bump, xkb_atom_t str);

ExprDef *
ExprCreateInteger(struct bump *bump, int ival);

ExprDef *
ExprCreateFloat(struct bump *bump);

ExprDef *
ExprCreateBoolean(struct bump *bump, bool set);

ExprDef *
ExprCreateKeyName(struct bump *bump, xkb_atom_t key_name);

ExprDef *
ExprCreateIdent(struct bump *bump, xkb_atom_t ident);

ExprDef *
ExprCreateUnary(struct bump *bump, enum stmt_type op, ExprDef *child);

ExprDef *
ExprCreateBinary(struct bump *bump, enum stmt_type op, ExprDef *left, ExprDef *right);

ExprDef *
ExprCreateFieldRef(struct bump *bump, xkb_atom_t element, xkb_atom_t field);

ExprDef *
ExprCreateArrayRef(struct bump *bump, xkb_atom_t element, xkb_atom_t field, ExprDef *entry);

ExprDef *
ExprEmptyList(struct bump *bump);

ExprDef *
ExprCreateAction(struct bump *bump, xkb_atom_t name, ExprDef *args);

ExprDef *
ExprCreateActionList(struct bump *bump, ExprDef *actions);

ExprDef *
ExprCreateKeysymList(struct bump *bump, xkb_keysym_t sym);

ExprDef *
ExprAppendKeysymList(struct bump *bump, ExprDef *list, xkb_keysym_t sym);

KeycodeDef *
KeycodeCreate(struct bump *bump, xkb_atom_t name, int64_t value);

KeyAliasDef *
KeyAliasCreate(struct bump *bump, xkb_atom_t alias, xkb_atom_t real);

VModDef *
VModCreate(struct bump *bump, xkb_atom_t name, ExprDef *value);

VarDef *
VarCreate(struct bump *bump, ExprDef *name, ExprDef *value);

VarDef *
BoolVarCreate(struct bump *bump, xkb_atom_t ident, bool set);

InterpDef *
InterpCreate(struct bump *bump, xkb_keysym_t sym, ExprDef *match);

KeyTypeDef *
KeyTypeCreate(struct bump *bump, xkb_atom_t name, VarDef *body);

SymbolsDef *
SymbolsCreate(struct bump *bump, xkb_atom_t keyName, VarDef *symbols);

GroupCompatDef *
GroupCompatCreate(struct bump *bump, unsigned group, ExprDef *def);

ModMapDef *
ModMapCreate(struct bump *bump, xkb_atom_t modifier, ExprDef *keys);

LedMapDef *
LedMapCreate(struct bump *bump, xkb_atom_t name, VarDef *body);

LedNameDef *
LedNameCreate(struct bump *bump, unsigned ndx, ExprDef *name, bool virtual);

IncludeStmt *
IncludeCreate(struct bump *bump, struct xkb_context *ctx, char *str,
              enum merge_mode merge);

XkbFile *
XkbFileCreate(struct bump *bump, enum xkb_file_type type, char *name,
              ParseCommon *defs, enum xkb_map_flags flags);
