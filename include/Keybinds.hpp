#pragma once

#include <cctype>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "DataFormat.hpp"

// A set of physical keys that must all be held simultaneously.
struct KeyChord {
    std::vector<SDL_Scancode> keys;
};

// Returns true if every key in the chord is currently held.
// An empty chord never matches.
inline bool ChordHeld(const bool* keyState, const KeyChord& chord) {
    if (chord.keys.empty()) return false;
    for (SDL_Scancode sc : chord.keys) {
        if (sc == SDL_SCANCODE_UNKNOWN) continue;
        if (!keyState[sc]) return false;
    }
    return true;
}

// Returns true if `eventScancode` is one of the chord's keys and every other
// key in the chord is currently held.  Use this for KEYDOWN event matching.
// An empty chord never matches.
inline bool ChordPressed(SDL_Scancode eventScancode, const bool* keyState, const KeyChord& chord) {
    if (chord.keys.empty()) return false;
    bool hasTrigger = false;
    for (SDL_Scancode sc : chord.keys) {
        if (sc == eventScancode) { hasTrigger = true; continue; }
        if (sc == SDL_SCANCODE_UNKNOWN) continue;
        if (!keyState[sc]) return false;
    }
    return hasTrigger;
}

// Key token string → SDL_Scancode
// Token format (mirrors keybinds.data spec):
//   Modifiers  : LC RC LS RS LA RA   (F = fn, maps to UNKNOWN)
//   Misc       : ES CL B EN D T SP
//   Function   : F1 F2 ... F12
//   Letters    : a b c ... z  (lowercase only)
//   Digits     : 0 1 ... 9
//   Symbols    : - = [ ] ; ' ` , . /  \\ (backslash)
//   Escaped    : \< \> \+ \\  (used as literal key chars inside <...>)
inline SDL_Scancode KeyTokenToScancode(const std::string& token) {
    if (token.empty()) return SDL_SCANCODE_UNKNOWN;

    if (token == "LC") return SDL_SCANCODE_LCTRL;
    if (token == "RC") return SDL_SCANCODE_RCTRL;
    if (token == "LS") return SDL_SCANCODE_LSHIFT;
    if (token == "RS") return SDL_SCANCODE_RSHIFT;
    if (token == "LA") return SDL_SCANCODE_LALT;
    if (token == "RA") return SDL_SCANCODE_RALT;

    if (token == "ES") return SDL_SCANCODE_ESCAPE;
    if (token == "CL") return SDL_SCANCODE_CAPSLOCK;
    if (token == "B")  return SDL_SCANCODE_BACKSPACE;
    if (token == "EN") return SDL_SCANCODE_RETURN;
    if (token == "D")  return SDL_SCANCODE_DELETE;
    if (token == "T")  return SDL_SCANCODE_TAB;
    if (token == "SP") return SDL_SCANCODE_SPACE;

    if (token.size() >= 2 && token[0] == 'F') {
        bool allDigits = true;
        for (std::size_t i = 1; i < token.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(token[i]))) { allDigits = false; break; }
        if (allDigits) {
            int n = std::stoi(token.substr(1));
            if (n >= 1 && n <= 12)
                return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + (n - 1));
        }
    }

    if (token.size() == 1) {
        const char c = token[0];
        if (c >= 'a' && c <= 'z')
            return static_cast<SDL_Scancode>(SDL_SCANCODE_A + (c - 'a'));
        if (c >= '1' && c <= '9')
            return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (c - '1'));
        if (c == '0') return SDL_SCANCODE_0;
        if (c == '-')  return SDL_SCANCODE_MINUS;
        if (c == '=')  return SDL_SCANCODE_EQUALS;
        if (c == '[')  return SDL_SCANCODE_LEFTBRACKET;
        if (c == ']')  return SDL_SCANCODE_RIGHTBRACKET;
        if (c == ';')  return SDL_SCANCODE_SEMICOLON;
        if (c == '\'') return SDL_SCANCODE_APOSTROPHE;
        if (c == '`')  return SDL_SCANCODE_GRAVE;
        if (c == ',')  return SDL_SCANCODE_COMMA;
        if (c == '.')  return SDL_SCANCODE_PERIOD;
        if (c == '/')  return SDL_SCANCODE_SLASH;
        if (c == '\\') return SDL_SCANCODE_BACKSLASH;
        if (c == '<' || c == '>') return SDL_SCANCODE_NONUSBACKSLASH;
        if (c == '+')             return SDL_SCANCODE_EQUALS;
    }

    return SDL_SCANCODE_UNKNOWN;
}

inline KeyChord ChordFromKeybind(const DataFormat::Keybind& kb) {
    KeyChord chord;
    for (const std::string& token : kb.keys)
        chord.keys.push_back(KeyTokenToScancode(token));
    return chord;
}

// Keybinds – all named actions with their default chords.
struct Keybinds {
    KeyChord quit           = KeyChord{{SDL_SCANCODE_F4, SDL_SCANCODE_RALT}};
    KeyChord pause          = KeyChord{{SDL_SCANCODE_ESCAPE}};
    KeyChord move_forward   = KeyChord{{SDL_SCANCODE_W}};
    KeyChord move_back      = KeyChord{{SDL_SCANCODE_S}};
    KeyChord move_left      = KeyChord{{SDL_SCANCODE_A}};
    KeyChord move_right     = KeyChord{{SDL_SCANCODE_D}};
    KeyChord jump           = KeyChord{{SDL_SCANCODE_SPACE}};
    KeyChord crouch         = KeyChord{{SDL_SCANCODE_LCTRL}};
    KeyChord crawl_toggle   = KeyChord{{SDL_SCANCODE_LCTRL, SDL_SCANCODE_LSHIFT}};

    KeyChord hotbar[9];

    KeyChord debug_toggle         = KeyChord{{SDL_SCANCODE_F3}};
    KeyChord debug_wireframe      = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_W}};
    KeyChord debug_block          = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_B}};
    KeyChord debug_face           = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_F}};
    KeyChord debug_data           = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_D}};
    KeyChord debug_wireframe_only = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_T}};
    KeyChord debug_stance         = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_S}};
    KeyChord debug_velocity       = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_V}};
    KeyChord debug_reload         = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_H}};
    KeyChord debug_save           = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_E}};
    KeyChord debug_load           = KeyChord{{SDL_SCANCODE_F3, SDL_SCANCODE_L}};

    Keybinds() {
        hotbar[0] = KeyChord{{SDL_SCANCODE_1}};
        hotbar[1] = KeyChord{{SDL_SCANCODE_2}};
        hotbar[2] = KeyChord{{SDL_SCANCODE_3}};
        hotbar[3] = KeyChord{{SDL_SCANCODE_4}};
        hotbar[4] = KeyChord{{SDL_SCANCODE_5}};
        hotbar[5] = KeyChord{{SDL_SCANCODE_6}};
        hotbar[6] = KeyChord{{SDL_SCANCODE_7}};
        hotbar[7] = KeyChord{{SDL_SCANCODE_8}};
        hotbar[8] = KeyChord{{SDL_SCANCODE_9}};
    }
};

inline bool LoadKeybinds(const std::string& path, Keybinds& out) {
    const auto doc = DataFormat::ParseFile(path);
    if (!doc) return false;

    auto readChord = [&](const char* key, KeyChord& field) {
        if (const DataFormat::Value* v = doc->Get(key))
            if (v->IsKeybind()) field = ChordFromKeybind(v->AsKeybind());
    };

    readChord("quit",                 out.quit);
    readChord("move_forward",         out.move_forward);
    readChord("move_back",            out.move_back);
    readChord("move_left",            out.move_left);
    readChord("move_right",           out.move_right);
    readChord("jump",                 out.jump);
    readChord("crouch",               out.crouch);
    readChord("crawl_toggle",         out.crawl_toggle);
    readChord("debug_toggle",         out.debug_toggle);
    readChord("debug_wireframe",      out.debug_wireframe);
    readChord("debug_block",          out.debug_block);
    readChord("debug_face",           out.debug_face);
    readChord("debug_data",           out.debug_data);
    readChord("debug_wireframe_only", out.debug_wireframe_only);
    readChord("debug_stance",         out.debug_stance);
    readChord("debug_velocity",       out.debug_velocity);
    readChord("debug_reload",         out.debug_reload);
    readChord("debug_save",           out.debug_save);
    readChord("debug_load",           out.debug_load);

    for (int i = 0; i < 9; ++i) {
        const std::string key = "hotbar_" + std::to_string(i + 1);
        readChord(key.c_str(), out.hotbar[i]);
    }

    return true;
}
