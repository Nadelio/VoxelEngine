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
    readChord("pause",                out.pause);
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

// ─── Serialization helpers ────────────────────────────────────────────────

// SDL_Scancode → the token string used in keybinds.data.
// Returns an empty string for unknown scancodes.
inline std::string ScancodeToKeyToken(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_LCTRL:        return "LC";
        case SDL_SCANCODE_RCTRL:        return "RC";
        case SDL_SCANCODE_LSHIFT:       return "LS";
        case SDL_SCANCODE_RSHIFT:       return "RS";
        case SDL_SCANCODE_LALT:         return "LA";
        case SDL_SCANCODE_RALT:         return "RA";
        case SDL_SCANCODE_ESCAPE:       return "ES";
        case SDL_SCANCODE_CAPSLOCK:     return "CL";
        case SDL_SCANCODE_BACKSPACE:    return "B";
        case SDL_SCANCODE_RETURN:       return "EN";
        case SDL_SCANCODE_DELETE:       return "D";
        case SDL_SCANCODE_TAB:          return "T";
        case SDL_SCANCODE_SPACE:        return "SP";
        default: break;
    }
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12)
        return std::string("F") + std::to_string(sc - SDL_SCANCODE_F1 + 1);
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return std::string(1, static_cast<char>('a' + (sc - SDL_SCANCODE_A)));
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return std::string(1, static_cast<char>('1' + (sc - SDL_SCANCODE_1)));
    if (sc == SDL_SCANCODE_0)            return "0";
    if (sc == SDL_SCANCODE_MINUS)        return "-";
    if (sc == SDL_SCANCODE_EQUALS)       return "=";
    if (sc == SDL_SCANCODE_LEFTBRACKET)  return "[";
    if (sc == SDL_SCANCODE_RIGHTBRACKET) return "]";
    if (sc == SDL_SCANCODE_SEMICOLON)    return ";";
    if (sc == SDL_SCANCODE_APOSTROPHE)   return "'";
    if (sc == SDL_SCANCODE_GRAVE)        return "`";
    if (sc == SDL_SCANCODE_COMMA)        return ",";
    if (sc == SDL_SCANCODE_PERIOD)       return ".";
    if (sc == SDL_SCANCODE_SLASH)        return "/";
    if (sc == SDL_SCANCODE_BACKSLASH)    return "\\\\";
    return {};
}

// KeyChord → the <tok+tok+...> literal used in keybinds.data.
inline std::string ChordToDataString(const KeyChord& chord) {
    if (chord.keys.empty()) return "<>";
    std::string s = "<";
    for (std::size_t i = 0; i < chord.keys.size(); ++i) {
        if (i > 0) s += '+';
        const std::string tok = ScancodeToKeyToken(chord.keys[i]);
        s += tok.empty() ? "?" : tok;
    }
    s += '>';
    return s;
}

// KeyChord → a human-readable display string, e.g. "Left Alt + F4".
inline std::string ChordToDisplayString(const KeyChord& chord) {
    if (chord.keys.empty()) return "(none)";
    std::string s;
    for (std::size_t i = 0; i < chord.keys.size(); ++i) {
        if (i > 0) s += " + ";
        if (const char* name = SDL_GetScancodeName(chord.keys[i]))
            s += name;
        else
            s += '?';
    }
    return s;
}

// Write all keybinds to a .data file. Returns false on I/O failure.
inline bool SaveKeybinds(const std::string& path, const Keybinds& kb) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;

    auto write = [&](const char* name, const KeyChord& chord) {
        std::fprintf(f, "%-24s : %s\n", name, ChordToDataString(chord).c_str());
    };

    std::fprintf(f, "# Keybinds – auto-saved by the settings menu.\n");
    std::fprintf(f, "# See the original keybinds.data for the token reference.\n\n");
    std::fprintf(f, "# general\n");
    write("quit",                 kb.quit);
    write("pause",                kb.pause);
    write("move_forward",         kb.move_forward);
    write("move_back",            kb.move_back);
    write("move_left",            kb.move_left);
    write("move_right",           kb.move_right);
    write("jump",                 kb.jump);
    write("crouch",               kb.crouch);
    write("crawl_toggle",         kb.crawl_toggle);
    std::fprintf(f, "\n# hotbar\n");
    for (int i = 0; i < 9; ++i) {
        const std::string key = "hotbar_" + std::to_string(i + 1);
        write(key.c_str(), kb.hotbar[i]);
    }
    std::fprintf(f, "\n# debug\n");
    write("debug_toggle",         kb.debug_toggle);
    write("debug_wireframe",      kb.debug_wireframe);
    write("debug_block",          kb.debug_block);
    write("debug_face",           kb.debug_face);
    write("debug_data",           kb.debug_data);
    write("debug_wireframe_only", kb.debug_wireframe_only);
    write("debug_stance",         kb.debug_stance);
    write("debug_velocity",       kb.debug_velocity);
    write("debug_reload",         kb.debug_reload);
    write("debug_save",           kb.debug_save);
    write("debug_load",           kb.debug_load);
    std::fclose(f);
    return true;
}
