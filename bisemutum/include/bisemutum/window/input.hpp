#pragma once

#include <cstdint>

namespace bi::input {

enum class Mouse : uint8_t {
    left,
    right,
    middle,
};

// values are the same as those in GLFW
enum class Keyboard : uint16_t {
    space = ' ',
    quot = '\'',
    comma = ',',
    minus = '-',
    period = '.',
    slash = '/',
    number_0 = '0',
    number_1 = '1',
    number_2 = '2',
    number_3 = '3',
    number_4 = '4',
    number_5 = '5',
    number_6 = '6',
    number_7 = '7',
    number_8 = '8',
    number_9 = '9',
    semicolon = ';',
    equal = '=',
    a = 'a',
    b = 'b',
    c = 'c',
    d = 'd',
    e = 'e',
    f = 'f',
    g = 'g',
    h = 'h',
    i = 'i',
    j = 'j',
    k = 'k',
    l = 'l',
    m = 'm',
    n = 'n',
    o = 'o',
    p = 'p',
    q = 'q',
    r = 'r',
    s = 's',
    t = 't',
    u = 'u',
    v = 'v',
    w = 'w',
    x = 'x',
    y = 'y',
    z = 'z',
    left_bracket = '[',
    back_slash = '\\',
    right_bracket = ']',
    grave_accent = '`',
    escape = 256,
    enter,
    tab,
    backspace,
    insert,
    delete_,
    right,
    left,
    down,
    up,
    page_up,
    page_down,
    home,
    end,
    caps_lock = 280,
    scroll_lock,
    number_lock,
    print_screen,
    pause,
    f1 = 290,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f9,
    f10,
    f11,
    f12,
    f13,
    f14,
    f15,
    f16,
    f17,
    f18,
    f19,
    f20,
    f21,
    f22,
    f23,
    f24,
    f25,
    keypad_0 = 320,
    keypad_1,
    keypad_2,
    keypad_3,
    keypad_4,
    keypad_5,
    keypad_6,
    keypad_7,
    keypad_8,
    keypad_9,
    keypad_decimal,
    keypad_divide,
    keypad_multiply,
    keypad_subtracy,
    keypad_add,
    keypad_enter,
    keypad_equal,
    left_shift = 340,
    left_control,
    left_alt,
    left_super,
    right_shift,
    right_control,
    right_alt,
    right_super,
    menu,
};

enum class KeyState {
    press,
    release,
    repeat,
};

}
