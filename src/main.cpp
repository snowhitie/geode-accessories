/*
 *  Accessories — Geode SDK 5.8.2 / GD 2.208
 *  ------------------------------------------------------------------------
 *  Single-file implementation, as requested. Read the top-of-file notes
 *  before building — a few things are called out explicitly because they
 *  depend on details of your local Bindings.hpp that I can't verify from
 *  here, and I'd rather flag them than have you hit a silent wrong-result
 *  bug later.
 *
 *  NOTES / ASSUMPTIONS (please check against your GeneratedSource):
 *  ------------------------------------------------------------------------
 *  1) Gamemode detection on PlayerObject uses the bool flags
 *         m_isShip, m_isBird, m_isBall, m_isDart, m_isRobot, m_isSpider, m_isSwing
 *     These are the conventional names across GD binding sets. If your
 *     Bindings.hpp names them differently, update `getCurrentModeKey()`
 *     only — nothing else depends on the exact names.
 *
 *  2) Accessory-to-icon tracking: the accessory containers are parented
 *     directly to PlayerObject and inherit its rotation/scale automatically
 *     through the normal scene-graph transform (no manual mirroring needed
 *     for that part). On top of that, update(float) explicitly applies an
 *     instant 180° flip (flying modes: ship/ufo/jetpack/robot/spider) or a
 *     horizontal mirror via scaleX (grounded modes) when gravity is flipped,
 *     since GD does not represent that particular flip through PlayerObject's
 *     own top-level transform. This depends on a member named `m_isUpsideDown`
 *     — see the NOTE right above where it's used if your Bindings.hpp calls
 *     it something else.
 *     This will NOT capture micro-animation baked into the individual icon
 *     sub-sprites (cube roll spin, spider leg movement, robot run cycle),
 *     because that would require child-sprite member names I can't
 *     guarantee are correct for 2.208 without your headers in front of me.
 *     If you tell me the exact member names (or paste the relevant part
 *     of PlayerObject's binding), I can wire this up precisely.
 *
 *  3) No std::filesystem anywhere — directory scanning uses raw platform
 *     APIs: Win32 FindFirstFileA/FindNextFileA on Windows, dirent.h/stat on
 *     Android (selected at compile time via GEODE_IS_WINDOWS). The Android
 *     path is untested on-device (no Android build environment available
 *     here) — it's the standard POSIX approach, but please report back if
 *     scanning behaves oddly there. Also note Android's storage sandboxing
 *     may make "drop a folder next to the game" much less convenient than
 *     on Windows; where exactly `dirs::getGameDir()` resolves to on your
 *     device, and whether it's writable/browsable from a file manager
 *     without extra permissions, is outside what I can verify from here.
 *
 *  4) JSON persistence uses a tiny hand-rolled reader/writer defined in
 *     this file (namespace tjson) instead of guessing at matjson's exact
 *     API surface for this SDK version — zero external dependency, and
 *     it only needs to round-trip the fixed schema below.
 *
 *  5) No lambdas are passed to any CCMenuItem — every callback is a real
 *     class member wired with menu_selector(...).
 */

#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GJGarageLayer.hpp>
#include <Geode/binding/GJRobotSprite.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#if defined(GEODE_IS_WINDOWS)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <optional>

using namespace geode::prelude;

// =============================================================================================
//  Tiny JSON (schema-specific, hand rolled, no external deps, no std::filesystem)
// =============================================================================================
namespace tjson {

    struct Value {
        enum class Type { Null, Object, Array, String, Number, Bool };
        Type type = Type::Null;
        std::map<std::string, Value> obj;
        std::vector<Value> arr;
        std::string str;
        double num = 0.0;
        bool bl = false;

        static Value makeObject() { Value v; v.type = Type::Object; return v; }
        static Value makeString(std::string const& s) { Value v; v.type = Type::String; v.str = s; return v; }
        static Value makeNumber(double n) { Value v; v.type = Type::Number; v.num = n; return v; }
        static Value makeBool(bool b) { Value v; v.type = Type::Bool; v.bl = b; return v; }

        Value& operator[](std::string const& key) {
            if (type != Type::Object) { type = Type::Object; }
            return obj[key];
        }

        bool has(std::string const& key) const {
            return type == Type::Object && obj.find(key) != obj.end();
        }

        double asNumber(double def = 0.0) const { return type == Type::Number ? num : def; }
        bool asBool(bool def = false) const { return type == Type::Bool ? bl : def; }
        std::string asString(std::string const& def = "") const { return type == Type::String ? str : def; }
    };

    inline void writeEscaped(std::ostringstream& out, std::string const& s) {
        out << '"';
        for (char c : s) {
            if (c == '"' || c == '\\') out << '\\' << c;
            else if (c == '\n') out << "\\n";
            else out << c;
        }
        out << '"';
    }

    inline void write(std::ostringstream& out, Value const& v) {
        switch (v.type) {
            case Value::Type::Object: {
                out << '{';
                bool first = true;
                for (auto const& [k, val] : v.obj) {
                    if (!first) out << ',';
                    first = false;
                    writeEscaped(out, k);
                    out << ':';
                    write(out, val);
                }
                out << '}';
                break;
            }
            case Value::Type::Array: {
                out << '[';
                for (size_t i = 0; i < v.arr.size(); i++) {
                    if (i) out << ',';
                    write(out, v.arr[i]);
                }
                out << ']';
                break;
            }
            case Value::Type::String:
                writeEscaped(out, v.str);
                break;
            case Value::Type::Number:
                out << v.num;
                break;
            case Value::Type::Bool:
                out << (v.bl ? "true" : "false");
                break;
            default:
                out << "null";
        }
    }

    inline std::string dump(Value const& v) {
        std::ostringstream out;
        write(out, v);
        return out.str();
    }

    // ---- minimal recursive-descent parser ----
    struct Parser {
        std::string const& s;
        size_t i = 0;
        Parser(std::string const& src) : s(src) {}

        void skipWs() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++; }

        Value parseValue() {
            skipWs();
            if (i >= s.size()) return Value{};
            char c = s[i];
            if (c == '{') return parseObject();
            if (c == '[') return parseArray();
            if (c == '"') return parseString();
            if (c == 't' || c == 'f') return parseBool();
            return parseNumber();
        }

        Value parseObject() {
            Value v = Value::makeObject();
            i++; // {
            skipWs();
            if (i < s.size() && s[i] == '}') { i++; return v; }
            while (true) {
                skipWs();
                Value key = parseString();
                skipWs();
                if (i < s.size() && s[i] == ':') i++;
                Value val = parseValue();
                v.obj[key.str] = val;
                skipWs();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == '}') { i++; break; }
                break;
            }
            return v;
        }

        Value parseArray() {
            Value v; v.type = Value::Type::Array;
            i++; // [
            skipWs();
            if (i < s.size() && s[i] == ']') { i++; return v; }
            while (true) {
                Value val = parseValue();
                v.arr.push_back(val);
                skipWs();
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == ']') { i++; break; }
                break;
            }
            return v;
        }

        Value parseString() {
            Value v; v.type = Value::Type::String;
            if (i >= s.size() || s[i] != '"') return v;
            i++;
            std::string out;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    i++;
                    if (s[i] == 'n') out += '\n';
                    else out += s[i];
                } else {
                    out += s[i];
                }
                i++;
            }
            if (i < s.size()) i++; // closing quote
            v.str = out;
            return v;
        }

        Value parseBool() {
            Value v; v.type = Value::Type::Bool;
            if (s.compare(i, 4, "true") == 0) { v.bl = true; i += 4; }
            else if (s.compare(i, 5, "false") == 0) { v.bl = false; i += 5; }
            return v;
        }

        Value parseNumber() {
            Value v; v.type = Value::Type::Number;
            size_t start = i;
            while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E')) i++;
            try { v.num = std::stod(s.substr(start, i - start)); } catch (...) { v.num = 0.0; }
            return v;
        }
    };

    inline Value parse(std::string const& src) {
        Parser p(src);
        return p.parseValue();
    }
}

// =============================================================================================
//  Win32-only directory scanning (no std::filesystem, no external libs)
// =============================================================================================
// Path separator differs per platform: Windows paths use backslashes, Android/POSIX use forward
// slashes. Every place in this file that joins path segments uses this instead of a literal.
#if defined(GEODE_IS_WINDOWS)
static const char* const PATH_SEP = "\\";
#else
static const char* const PATH_SEP = "/";
#endif

namespace winfs {

#if defined(GEODE_IS_WINDOWS)

    inline bool dirExists(std::string const& path) {
        DWORD attr = GetFileAttributesA(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline bool fileExists(std::string const& path) {
        DWORD attr = GetFileAttributesA(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline void makeDir(std::string const& path) {
        CreateDirectoryA(path.c_str(), nullptr);
    }

    // Returns the names (not full paths) of subdirectories directly inside `path`.
    inline std::vector<std::string> listSubdirs(std::string const& path) {
        std::vector<std::string> out;
        std::string pattern = path + "\\*";
        WIN32_FIND_DATAA data;
        HANDLE h = FindFirstFileA(pattern.c_str(), &data);
        if (h == INVALID_HANDLE_VALUE) return out;
        do {
            std::string name = data.cFileName;
            if (name == "." || name == "..") continue;
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                out.push_back(name);
            }
        } while (FindNextFileA(h, &data));
        FindClose(h);
        return out;
    }

    // Returns the file names (not full paths) directly inside `path`.
    inline std::vector<std::string> listFiles(std::string const& path) {
        std::vector<std::string> out;
        std::string pattern = path + "\\*";
        WIN32_FIND_DATAA data;
        HANDLE h = FindFirstFileA(pattern.c_str(), &data);
        if (h == INVALID_HANDLE_VALUE) return out;
        do {
            std::string name = data.cFileName;
            if (name == "." || name == "..") continue;
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                out.push_back(name);
            }
        } while (FindNextFileA(h, &data));
        FindClose(h);
        return out;
    }

#else // Android / other POSIX-ish targets — same four functions, built on dirent.h + sys/stat.h
      // instead of WinAPI. Untested on-device (no Android build environment available here);
      // this is the standard, well-established POSIX directory-scanning approach, but please
      // report back if anything behaves unexpectedly on Android.

    inline bool dirExists(std::string const& path) {
        struct stat st{};
        if (stat(path.c_str(), &st) != 0) return false;
        return S_ISDIR(st.st_mode);
    }

    inline bool fileExists(std::string const& path) {
        struct stat st{};
        if (stat(path.c_str(), &st) != 0) return false;
        return S_ISREG(st.st_mode);
    }

    inline void makeDir(std::string const& path) {
        mkdir(path.c_str(), 0755);
    }

    inline std::vector<std::string> listSubdirs(std::string const& path) {
        std::vector<std::string> out;
        DIR* dir = opendir(path.c_str());
        if (!dir) return out;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            std::string full = path + "/" + name;
            struct stat st{};
            if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                out.push_back(name);
            }
        }
        closedir(dir);
        return out;
    }

    inline std::vector<std::string> listFiles(std::string const& path) {
        std::vector<std::string> out;
        DIR* dir = opendir(path.c_str());
        if (!dir) return out;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            std::string full = path + "/" + name;
            struct stat st{};
            if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                out.push_back(name);
            }
        }
        closedir(dir);
        return out;
    }

#endif
    // Portable recursive copy helpers used by the in-game "Add Folder" importer.
    // We intentionally copy into the mod save directory so imported accessories survive
    // GDPS Switcher changes and do not depend on Android's game-directory permissions.
    inline bool makeDirsRecursive(std::string const& path) {
        if (path.empty() || dirExists(path)) return true;
        auto pos = path.find_last_of("/\\");
        if (pos != std::string::npos) {
            std::string parent = path.substr(0, pos);
            if (!parent.empty() && !dirExists(parent) && !makeDirsRecursive(parent)) return false;
        }
#if defined(GEODE_IS_WINDOWS)
        return CreateDirectoryA(path.c_str(), nullptr) || dirExists(path);
#else
        return mkdir(path.c_str(), 0755) == 0 || dirExists(path);
#endif
    }

    inline bool copyFile(std::string const& src, std::string const& dst) {
        std::ifstream in(src, std::ios::binary);
        if (!in) return false;
        auto slash = dst.find_last_of("/\\");
        if (slash != std::string::npos && !makeDirsRecursive(dst.substr(0, slash))) return false;
        std::ofstream out(dst, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << in.rdbuf();
        return in.good() || in.eof();
    }

    inline bool copyTree(std::string const& src, std::string const& dst) {
        if (!dirExists(src)) return false;
        if (!makeDirsRecursive(dst)) return false;
        for (auto const& fileName : listFiles(src)) {
            if (!copyFile(src + PATH_SEP + fileName, dst + PATH_SEP + fileName)) return false;
        }
        for (auto const& dirName : listSubdirs(src)) {
            if (!copyTree(src + PATH_SEP + dirName, dst + PATH_SEP + dirName)) return false;
        }
        return true;
    }

    inline bool containsPngRecursive(std::string const& path) {
        for (auto const& fileName : listFiles(path)) {
            std::string n = fileName;
            std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            if (n.size() >= 4 && n.substr(n.size() - 4) == ".png") return true;
        }
        for (auto const& dirName : listSubdirs(path)) {
            if (containsPngRecursive(path + PATH_SEP + dirName)) return true;
        }
        return false;
    }

    inline std::string baseName(std::string const& path) {
        if (path.empty()) return "Accessory";
        size_t end = path.size();
        while (end > 0 && (path[end - 1] == '/' || path[end - 1] == '\\')) --end;
        size_t pos = path.find_last_of("/\\", end == 0 ? 0 : end - 1);
        if (pos == std::string::npos) return path.substr(0, end);
        return path.substr(pos + 1, end - pos - 1);
    }

#endif
}


// =============================================================================================
//  Data model
// =============================================================================================

// The nine gamemode keys, in the order requested for the top row of buttons. This order also
// matches the IconType enum ordering (Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing, Jetpack).
static const char* MODE_KEYS[9] = {
    "cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing", "jetpack"
};

// NOTE: We used to reference made-up sprite-frame names here (e.g. "player_cube_001.png") for
// the mode-row button icons. Those frames don't exist in GD's texture sheets, which is exactly
// why they rendered as the pink/black "missing texture" checkerboard. Real per-icon textures
// aren't loose files you can request by an arbitrary name — the only reliable way to show a
// mode's icon is to build a real SimplePlayer with the player's currently-equipped icon ID for
// that mode and their real colors, same as the game itself does. See modeKeyToIconType() /
// iconIdForMode() / makeModeIcon() below, used when building the mode row and the preview.

// GD's PlayerObject has no dedicated "is jetpack" flag — per Geode's own binding docs, the only
// flight-related bools are m_isShip/m_isBird/m_isRobot/m_isSpider/m_isSwing (and friends);
// jetpack is just m_isShip == true with a different equipped icon. That means in real gameplay,
// getCurrentModeKey() (see the PlayerObject hook below) can only ever report "ship" while flying
// with a jetpack — it has no way to know it's specifically a jetpack. So the "jetpack" tab in the
// editor has to write to and read from the SAME storage bucket as "ship", or anything configured
// under "jetpack" would simply never apply in an actual level. The mode row still shows the
// correct jetpack icon (that part IS knowable, from GameManager's equipped-icon id), only the
// saved accessory settings are shared with ship.
static std::string configModeKey(std::string const& mode) {
    return mode == "jetpack" ? "ship" : mode;
}

static IconType modeKeyTo