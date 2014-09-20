/*
 * Copyright © 2014 Ran Benita <ran234@gmail.com>
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

/* A testsuite for trying various approaches to shortcuts. */

#include "evdev-scancodes.h"
#include "test.h"

#define SHIFT (1 << 0)
#define LOCK (1 << 1)
#define CTRL (1 << 2)
#define ALT (1 << 3)
#define SUPER (1 << 6)

#define LAYOUT0 0u
#define LAYOUT1 1u
#define LAYOUT2 2u
#define LAYOUT3 3u

#define SIGNIFICANT_MODS (SHIFT | CTRL | ALT | SUPER)

struct shortcut {
    xkb_mod_mask_t mods;
    xkb_keysym_t keysym;
    const char *action;
};

static bool
keysym_is_ascii(xkb_keysym_t keysym)
{
    return xkb_keysym_to_utf32(keysym) <= 127u;
}

static unsigned
popcount(uint32_t mask)
{
    unsigned count = 0;
    for (unsigned i = 0; i < 32; i++)
        if (mask & (1u << i))
            count++;
    return count;
}

#define NEXT_FIRST(i, first) \
    (i == first && first != 0 ? 0 : i + 1 == first ? first + 1 : i + 1)

static xkb_keysym_t
get_keysym_for_shortcut(const struct shortcut *shortcut,
                        struct xkb_keymap *keymap,
                        xkb_layout_index_t layout,
                        xkb_mod_mask_t mods,
                        xkb_keycode_t keycode,
                        xkb_mod_mask_t *consumed_mods_out)
{
    bool should_be_ascii;
    xkb_layout_index_t num_layouts;
    xkb_keysym_t keysym;
    struct xkb_state *try_state;

    // TODO: Shouldn't alloc a new state everytime.
    try_state = xkb_state_new(keymap);
    if (!try_state)
        goto fail;

    should_be_ascii = keysym_is_ascii(shortcut->keysym);

    xkb_state_update_mask(try_state, mods, 0, 0, 0, 0, layout);
    layout = xkb_state_key_get_layout(try_state, keycode);
    num_layouts = xkb_keymap_num_layouts_for_key(keymap, keycode);

    for (xkb_layout_index_t i = layout; i < num_layouts; i = NEXT_FIRST(i, layout)) {
        xkb_state_update_mask(try_state, mods, 0, 0, 0, 0, i);
        keysym = xkb_state_key_get_one_sym(try_state, keycode);
        if (keysym == XKB_KEY_NoSymbol)
            continue;
        if (!should_be_ascii || keysym_is_ascii(keysym)) {
            *consumed_mods_out = xkb_state_key_get_consumed_mods(try_state, keycode);
            goto out;
        }
    }

fail:
    *consumed_mods_out = 0;
    keysym = XKB_KEY_NoSymbol;

out:
    xkb_state_unref(try_state);
    return keysym;
}

static bool
shortcut_match(const struct shortcut *shortcut,
               struct xkb_keymap *keymap,
               xkb_layout_index_t layout,
               xkb_mod_mask_t mods,
               xkb_keycode_t keycode)
{
    xkb_keysym_t keysym;
    xkb_mod_mask_t consumed_mods;

    if ((shortcut->mods & mods) != shortcut->mods)
        return false;

    keysym = get_keysym_for_shortcut(shortcut, keymap, layout,
                                     mods & ~shortcut->mods, keycode,
                                     &consumed_mods);
    if (keysym == shortcut->keysym)
        goto found;

    keysym = get_keysym_for_shortcut(shortcut, keymap, layout,
                                     mods, keycode,
                                     &consumed_mods);
    if (keysym == shortcut->keysym)
        goto found;

    return false;

found:
    return (mods & ~shortcut->mods & ~consumed_mods & SIGNIFICANT_MODS) == 0;
}

static const struct shortcut *
find_matching_shortcut(const struct shortcut *shortcuts,
                       struct xkb_keymap *keymap,
                       xkb_layout_index_t layout,
                       xkb_mod_mask_t mods,
                       xkb_keycode_t keycode)
{
    unsigned best_num_mods = 0;
    const struct shortcut *shortcut, *best_shortcut = NULL;
    for (shortcut = shortcuts; shortcut->action; shortcut++) {
        if (shortcut_match(shortcut, keymap, layout, mods, keycode)) {
            if (!best_shortcut || popcount(shortcut->mods) > best_num_mods) {
                best_shortcut = shortcut;
                best_num_mods = popcount(shortcut->mods);
            }
        }
    }
    return best_shortcut;
}

static bool
test_shortcuts(struct xkb_keymap *keymap,
               const struct shortcut *shortcuts, ...)
{
    va_list ap;
    xkb_layout_index_t layout;
    xkb_mod_mask_t mods;
    xkb_keycode_t keycode;
    const char *expected_action, *found_action;
    const struct shortcut *shortcut;
    bool ret = false;

    va_start(ap, shortcuts);

    for (;;) {
        layout = va_arg(ap, xkb_layout_index_t);
        if (layout == XKB_LAYOUT_INVALID)
            break;
        mods = va_arg(ap, xkb_mod_mask_t);
        keycode = va_arg(ap, xkb_keycode_t) + EVDEV_OFFSET;
        expected_action = va_arg(ap, const char *);

        shortcut = find_matching_shortcut(shortcuts, keymap,
                                          layout, mods, keycode);

        found_action = shortcut ? shortcut->action : NULL;
        if (found_action != expected_action &&
            !streq_not_null(found_action, expected_action)) {
            fprintf(stderr, "expected action %s, got %s\n",
                    expected_action ? expected_action : "no match",
                    found_action ? found_action : "no match");
            fprintf(stderr, "layout: %x, mods: %x, scancode: %u\n",
                   layout, mods, keycode - EVDEV_OFFSET);
            goto out;
        }
    }

    ret = true;
out:
    va_end(ap);
    return ret;
}

int
main(void)
{
    struct xkb_context *ctx = test_get_context(0);
    struct xkb_keymap *keymap;

    const struct shortcut shortcuts[] = {
        { CTRL,             XKB_KEY_a,              "SelectAll" },
        { ALT,              XKB_KEY_Tab,            "NextWindow" },
        { SHIFT|ALT,        XKB_KEY_Tab,            "PrevWindow" },
        { CTRL,             XKB_KEY_BackSpace,      "DeleteWord" },
        { CTRL|ALT,         XKB_KEY_BackSpace,      "Terminate" },
        { 0,                XKB_KEY_minus,          "ZoomOut" },
        { 0,                XKB_KEY_equal,          "Equal" },
        { 0,                XKB_KEY_plus,           "ZoomIn" },
        { SHIFT,            XKB_KEY_equal,          "ShiftEqual" },
        { 0,                XKB_KEY_F1,             "Help" },
        { ALT,              XKB_KEY_F4,             "CloseWindow" },
        { CTRL|ALT,         XKB_KEY_F4,             "SwitchVT4" },
        { 0,                XKB_KEY_Break,          "Break" },
        { CTRL,             XKB_KEY_Break,          "CtrlBreak" },
        { SHIFT,            XKB_KEY_dollar,         "ShiftDollar" },
        { SHIFT,            XKB_KEY_semicolon,      "ShiftSemicolon" },
        { 0,                XKB_KEY_NoSymbol,       NULL },
    };

    keymap = test_compile_rules(ctx, NULL, NULL, "us", "", "terminate:ctrl_alt_bksp");
    assert(keymap);
    assert(test_shortcuts(keymap, shortcuts,
        LAYOUT0,    0,                  KEY_B,              NULL,
        LAYOUT0,    CTRL|ALT|SHIFT,     KEY_B,              NULL,

        LAYOUT0,    0,                  KEY_A,              NULL,
        LAYOUT0,    SHIFT,              KEY_A,              NULL,
        LAYOUT0,    CTRL,               KEY_A,              "SelectAll",
        LAYOUT0,    CTRL|ALT,           KEY_A,              NULL,

        LAYOUT0,    0,                  KEY_TAB,            NULL,
        LAYOUT0,    ALT,                KEY_TAB,            "NextWindow",
        LAYOUT0,    SHIFT,              KEY_TAB,            NULL,
        LAYOUT0,    SHIFT|ALT,          KEY_TAB,            "PrevWindow",
        LAYOUT0,    CTRL|ALT,           KEY_TAB,            NULL,
        LAYOUT0,    CTRL|SHIFT|ALT,     KEY_TAB,            NULL,

        LAYOUT0,    0,                  KEY_BACKSPACE,      NULL,
        LAYOUT0,    CTRL,               KEY_BACKSPACE,      "DeleteWord",
        LAYOUT0,    ALT,                KEY_BACKSPACE,      NULL,
        LAYOUT0,    CTRL|ALT,           KEY_BACKSPACE,      "Terminate",
        LAYOUT0,    CTRL|ALT|SUPER,     KEY_BACKSPACE,      NULL,

        LAYOUT0,    0,                  KEY_MINUS,          "ZoomOut",
        LAYOUT0,    CTRL,               KEY_MINUS,          NULL,
        LAYOUT0,    SHIFT,              KEY_MINUS,          NULL,

        LAYOUT0,    0,                  KEY_EQUAL,          "Equal",
        LAYOUT0,    SHIFT,              KEY_EQUAL,          "ShiftEqual",
        LAYOUT0,    CTRL|SHIFT,         KEY_EQUAL,          NULL,

        LAYOUT0,    0,                  KEY_F1,             "Help",
        LAYOUT0,    SHIFT,              KEY_F1,             NULL,

        LAYOUT0,    0,                  KEY_F4,             NULL,
        LAYOUT0,    SHIFT,              KEY_F4,             NULL,
        LAYOUT0,    ALT,                KEY_F4,             "CloseWindow",
        LAYOUT0,    CTRL,               KEY_F4,             NULL,
        LAYOUT0,    CTRL|ALT,           KEY_F4,             "SwitchVT4",
        LAYOUT0,    CTRL|ALT|SUPER,     KEY_F4,             NULL,
        LAYOUT0,    CTRL|SUPER,         KEY_F4,             NULL,

        LAYOUT0,    CTRL,               KEY_PAUSE,          "CtrlBreak",
        LAYOUT0,    SHIFT,              KEY_PAUSE,          NULL,
        LAYOUT0,    CTRL|SHIFT,         KEY_PAUSE,          NULL,

        LAYOUT0,    SHIFT,              KEY_4,              "ShiftDollar",

        XKB_LAYOUT_INVALID));
    xkb_keymap_unref(keymap);

    keymap = test_compile_rules(ctx, NULL, NULL, "us,ru", "", "terminate:ctrl_alt_bksp");
    assert(keymap);
    assert(test_shortcuts(keymap, shortcuts,
        LAYOUT1,    0,                  KEY_B,              NULL,
        LAYOUT1,    CTRL|ALT|SHIFT,     KEY_B,              NULL,

        LAYOUT1,    0,                  KEY_A,              NULL,
        LAYOUT1,    SHIFT,              KEY_A,              NULL,
        LAYOUT1,    CTRL,               KEY_A,              "SelectAll",
        LAYOUT1,    CTRL|ALT,           KEY_A,              NULL,

        LAYOUT1,    0,                  KEY_TAB,            NULL,
        LAYOUT1,    ALT,                KEY_TAB,            "NextWindow",
        LAYOUT1,    SHIFT,              KEY_TAB,            NULL,
        LAYOUT1,    SHIFT|ALT,          KEY_TAB,            "PrevWindow",
        LAYOUT1,    CTRL|ALT,           KEY_TAB,            NULL,
        LAYOUT1,    CTRL|SHIFT|ALT,     KEY_TAB,            NULL,

        LAYOUT1,    0,                  KEY_BACKSPACE,      NULL,
        LAYOUT1,    CTRL,               KEY_BACKSPACE,      "DeleteWord",
        LAYOUT1,    ALT,                KEY_BACKSPACE,      NULL,
        LAYOUT1,    CTRL|ALT,           KEY_BACKSPACE,      "Terminate",
        LAYOUT1,    CTRL|ALT|SUPER,     KEY_BACKSPACE,      NULL,

        LAYOUT1,    0,                  KEY_MINUS,          "ZoomOut",
        LAYOUT1,    CTRL,               KEY_MINUS,          NULL,
        LAYOUT1,    SHIFT,              KEY_MINUS,          NULL,

        LAYOUT1,    0,                  KEY_EQUAL,          "Equal",
        LAYOUT1,    SHIFT,              KEY_EQUAL,          "ShiftEqual",
        LAYOUT1,    CTRL|SHIFT,         KEY_EQUAL,          NULL,

        LAYOUT1,    0,                  KEY_F1,             "Help",
        LAYOUT1,    SHIFT,              KEY_F1,             NULL,

        LAYOUT1,    0,                  KEY_F4,             NULL,
        LAYOUT1,    SHIFT,              KEY_F4,             NULL,
        LAYOUT1,    ALT,                KEY_F4,             "CloseWindow",
        LAYOUT1,    CTRL,               KEY_F4,             NULL,
        LAYOUT1,    CTRL|ALT,           KEY_F4,             "SwitchVT4",
        LAYOUT1,    CTRL|ALT|SUPER,     KEY_F4,             NULL,
        LAYOUT1,    CTRL|SUPER,         KEY_F4,             NULL,

        LAYOUT1,    CTRL,               KEY_PAUSE,          "CtrlBreak",
        LAYOUT1,    SHIFT,              KEY_PAUSE,          NULL,
        LAYOUT1,    CTRL|SHIFT,         KEY_PAUSE,          NULL,

        LAYOUT1,    SHIFT,              KEY_4,              "ShiftSemicolon",

        XKB_LAYOUT_INVALID));
    xkb_keymap_unref(keymap);

    xkb_context_unref(ctx);
}
