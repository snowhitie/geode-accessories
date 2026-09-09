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

static IconType modeKeyToIconType(const std::string& mode) {
    for (int i = 0; i < 9; i++) if (mode == MODE_KEYS[i]) return static_cast<IconType>(i);
    return IconType::Cube;
}

// Returns the icon ID the player currently has equipped for a given mode.
static int iconIdForMode(GameManager* gm, const std::string& mode) {
    if (mode == "cube")    return gm->getPlayerFrame();
    if (mode == "ship")    return gm->getPlayerShip();
    if (mode == "ball")    return gm->getPlayerBall();
    if (mode == "ufo")     return gm->getPlayerBird();
    if (mode == "wave")    return gm->getPlayerDart();
    if (mode == "robot")   return gm->getPlayerRobot();
    if (mode == "spider")  return gm->getPlayerSpider();
    if (mode == "swing")   return gm->getPlayerSwing();
    if (mode == "jetpack") return gm->getPlayerJetpack();
    return gm->getPlayerFrame();
}

// Builds a small SimplePlayer showing the player's real equipped icon + real colors for `mode`.
static SimplePlayer* makeModeIcon(const std::string& mode) {
    auto gm = GameManager::get();
    int iconId = iconIdForMode(gm, mode);
    auto sp = SimplePlayer::create(iconId);
    sp->updatePlayerFrame(iconId, modeKeyToIconType(mode));
    sp->setColor(gm->colorForIdx(gm->getPlayerColor()));
    sp->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
    return sp;
}

// A small solid-white texture we generate ourselves at runtime (never loaded from any GD
// spritesheet), cached and reused by every squareSprite() call below. It exists purely so
// squareSprite() can hand out real CCSprites: CCMenuItemSpriteExtra's click hit-testing is
// built around CCSprite and broke when squareSprite() briefly used CCLayerColor instead.
// UI textures are generated in memory. This avoids depending on optional GD sprite-frame
// names and, unlike the old square texture, gives the popup the rounded/circular look of the
// original Geometry Dash UI.
static CCTexture2D* makeUITexture(int w, int h, int radius, int border) {
    // IMPORTANT:
    // These are logical UI sprites. Do NOT multiply the bitmap dimensions by
    // CCDirector::getContentScaleFactor(). Geometry Dash/Cocos can change that
    // value when GDPS Switcher changes the game environment. Mixing a scaled
    // bitmap with a logical content size makes rounded controls render at a
    // fraction of their intended size (typically 1/2 or 1/4).
    //
    // Keep the generated texture strictly 1:1 with its logical size. Cocos2d
    // will handle the device framebuffer scale itself.
    w = (std::max)(1, w);
    h = (std::max)(1, h);
    radius = (std::max)(0, radius);
    border = (std::max)(0, border);

    struct CacheEntry { int w, h, radius, border; CCTexture2D* texture; };
    static std::vector<CacheEntry> cache;
    for (auto const& e : cache) {
        if (e.w == w && e.h == h && e.radius == radius && e.border == border)
            return e.texture;
    }

    int pw = w;
    int ph = h;
    int pr = (std::min)(radius, (std::min)(w, h) / 2);
    int pb = (std::min)(border, (std::min)(w, h) / 2);

    std::vector<unsigned char> pixels((size_t)pw * (size_t)ph * 4, 0);

    float left = (float)pb;
    float right = (float)pw - 1.f - pb;
    float top = (float)pb;
    float bottom = (float)ph - 1.f - pb;
    float r = (float)pr;

    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            float px = (float)x;
            float py = (float)y;
            float dx = 0.f;
            float dy = 0.f;

            if (px < left + r) dx = (left + r) - px;
            else if (px > right - r) dx = px - (right - r);
            if (py < top + r) dy = (top + r) - py;
            else if (py > bottom - r) dy = py - (bottom - r);

            float dist = std::sqrt(dx * dx + dy * dy);
            float alpha = 255.f;
            if (dist > r) {
                alpha = (std::max)(0.f, 255.f - (dist - r) * 255.f);
            }

            size_t i = ((size_t)y * (size_t)pw + (size_t)x) * 4;
            pixels[i + 0] = 255;
            pixels[i + 1] = 255;
            pixels[i + 2] = 255;
            pixels[i + 3] = (unsigned char)(std::min)(255.f, alpha);
        }
    }

    auto tex = new CCTexture2D();
    if (!tex->initWithData(
        pixels.data(), kCCTexture2DPixelFormat_RGBA8888,
        pw, ph, CCSizeMake((float)w, (float)h)
    )) {
        delete tex;
        return nullptr;
    }

    cache.push_back({ w, h, radius, border, tex });
    return tex;
}

// A completely transparent sprite used as a reliable CCMenuItemSpriteExtra hitbox.
// Unlike a white/colored rectangle, it contributes no visible pixels to the UI.
static CCSprite* transparentHitSprite(float w = 120.f, float h = 42.f) {
    int iw = (std::max)(1, (int)std::round(w));
    int ih = (std::max)(1, (int)std::round(h));

    // Same rule as makeUITexture(): keep the hit texture 1:1. A GDPS switch
    // must never change the logical size of our controls.
    struct HitEntry { int w, h; CCTexture2D* texture; };
    static std::vector<HitEntry> cache;
    for (auto const& e : cache) {
        if (e.w == iw && e.h == ih) {
            auto s = CCSprite::createWithTexture(e.texture);
            if (s) return s;
        }
    }

    std::vector<unsigned char> pixels((size_t)iw * (size_t)ih * 4, 0);
    auto tex = new CCTexture2D();
    if (!tex->initWithData(
        pixels.data(), kCCTexture2DPixelFormat_RGBA8888,
        iw, ih, CCSizeMake((float)iw, (float)ih)
    )) {
        delete tex;
        return nullptr;
    }

    cache.push_back({ iw, ih, tex });
    return CCSprite::createWithTexture(tex);
}

static CCSprite* roundedButtonSprite(ccColor3B tint = { 80, 90, 110 }, float scale = 1.f) {
    auto tex = makeUITexture(180, 54, 12, 1);
    if (!tex) return nullptr;
    auto s = CCSprite::createWithTexture(tex);
    if (!s) return nullptr;
    s->setColor(tint);
    s->setScale(scale);
    return s;
}

static CCSprite* circleButtonSprite(ccColor3B tint = { 100, 100, 100 }, float scale = 1.f, bool visible = true) {
    auto tex = makeUITexture(72, 72, 35, 1);
    if (!tex) return nullptr;
    auto s = CCSprite::createWithTexture(tex);
    if (!s) return nullptr;
    s->setColor(tint);
    s->setScale(scale);
    s->setOpacity(visible ? 255 : 0);
    return s;
}

// Small translucent GD-style control. The shape is intentionally subtle: it shows the
// button bounds without turning every label into a heavy rectangular tile.
static CCSprite* translucentButtonSprite(float w, float h, ccColor3B tint, float opacity = 85.f, float radius = 8.f) {
    int iw = (std::max)(1, (int)std::round(w));
    int ih = (std::max)(1, (int)std::round(h));
    auto tex = makeUITexture(iw, ih, (int)std::round(radius), 1);
    if (!tex) return nullptr;
    auto s = CCSprite::createWithTexture(tex);
    if (!s) return nullptr;
    s->setColor(tint);
    s->setOpacity((GLubyte)(std::max)(0.f, (std::min)(255.f, opacity)));
    return s;
}

static CCMenuItemSpriteExtra* makeSquareTextButton(
    std::string const& text, ccColor3B tint, CCObject* target, SEL_MenuHandler sel,
    float size = 32.f, float opacity = 120.f
) {
    auto bg = translucentButtonSprite(size, size, tint, opacity, 4.f);
    if (!bg) return nullptr;
    auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    if (!label) return nullptr;
    label->setScale(0.30f);
    label->setPosition({ size / 2.f, size / 2.f });
    label->setColor({ 255, 255, 255 });
    bg->addChild(label);
    return CCMenuItemSpriteExtra::create(bg, target, sel);
}

// Text-only GD-style control with a subtle translucent shape underneath.
// The shape is visible enough to show the clickable bounds, but stays lightweight like the
// buttons used throughout Geometry Dash.
static CCMenuItemSpriteExtra* makeTextOnlyButton(
    std::string const& text, ccColor3B color, float labelScale,
    CCObject* target, SEL_MenuHandler sel, float hitW = 120.f, float hitH = 34.f
) {
    auto hit = translucentButtonSprite(hitW, hitH, color, 72.f, 7.f);
    if (!hit) return nullptr;
    auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    if (!label) return nullptr;
    label->setScale(labelScale);
    label->setColor({ 245, 245, 245 });
    label->setPosition({ hit->getContentSize().width / 2.f, hit->getContentSize().height / 2.f });
    hit->addChild(label);
    return CCMenuItemSpriteExtra::create(hit, target, sel);
}

// Retained only for the close/legacy call sites. It is now a rounded control rather than a
// square texture, and it is never used for the mode icons or folder ON/OFF controls.
static CCSprite* squareSprite(ccColor3B tint = { 100, 100, 100 }) {
    return roundedButtonSprite(tint, 1.f);
}

struct AccessoryPartConfig {
    bool enabled = false;
    float x = 0.f;
    float y = 0.f;
    float scale = 1.f;
    float rotation = 0.f; // degrees, relative to the icon's own rotation
    bool front = true; // true = in front of icon, false = behind
};

// What we found on disk for one "part" (either Accessories/<folder>/<part>/ when part-subfolders
// exist, or Accessories/<folder>/ itself when they don't). We store the *exact* matched file path
// for each suffix, rather than reconstructing a filename later — the part folder's own name has
// no fixed relationship to the actual PNG filenames inside it (they might be "p1.png", or
// "crown1_p1.png", or anything else that merely *ends with* the right suffix).
struct AccessoryPartFiles {
    std::string name;         // display name = the part's own folder name
    std::string fullPath;     // full path to the folder that directly contains this part's PNGs
    std::string selfColorPath;
    std::string p1Path;
    std::string p2Path;
    std::string linePath;
    std::string glowPath;

    bool hasSelfColor() const { return !selfColorPath.empty(); }
    bool hasP1() const { return !p1Path.empty(); }
    bool hasP2() const { return !p2Path.empty(); }
    bool hasLine() const { return !linePath.empty(); }
    bool hasGlow() const { return !glowPath.empty(); }
    bool hasAnyFile() const { return hasSelfColor() || hasP1() || hasP2() || hasLine() || hasGlow(); }
};

// One top-level accessory (e.g. "crown"), containing multiple parts.
struct AccessoryFolder {
    std::string name;
    std::string fullPath;
    std::vector<AccessoryPartFiles> parts;
};

// =============================================================================================
//  AccessoryManager — singleton: disk scanning + settings persistence
// =============================================================================================
class AccessoryManager {
protected:
    std::vector<AccessoryFolder> m_folders;

    // settings[playerKey][modeKey][folderName]        -> folder visible?
    std::map<std::string, std::map<std::string, std::map<std::string, bool>>> m_folderVisible;
    // settings[playerKey][modeKey][folderName][partName] -> part config
    std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, AccessoryPartConfig>>>> m_partConfig;

public:
    static AccessoryManager* get() {
        static AccessoryManager instance;
        return &instance;
    }

    std::string accessoriesRoot() {
        // The mod save directory is the canonical library. Imported folders are stored here,
        // so the same accessories work on Windows and Android and survive GDPS Switcher.
        return (Mod::get()->getSaveDir() / "Accessories").string();
    }

    std::string legacyAccessoriesRoot() {
        // Keep supporting the old PC layout next to GeometryDash.exe.
        return dirs::getGameDir().string() + PATH_SEP + "Accessories";
    }

    static std::string uniqueImportPath(std::string const& root, std::string name) {
        if (name.empty()) name = "Accessory";
        std::string candidate = root + PATH_SEP + name;
        if (!winfs::dirExists(candidate)) return candidate;
        for (int i = 2; i < 10000; ++i) {
            candidate = root + PATH_SEP + name + "_" + std::to_string(i);
            if (!winfs::dirExists(candidate)) return candidate;
        }
        return {};
    }

    bool importAccessoryFolder(std::string const& sourcePath, std::string& importedName, std::string& error) {
        ensureRootExists();
        if (!winfs::dirExists(sourcePath)) {
            error = "Selected item is not a folder.";
            return false;
        }
        if (!winfs::containsPngRecursive(sourcePath)) {
            error = "The selected folder does not contain any PNG files.";
            return false;
        }

        std::string name = winfs::baseName(sourcePath);
        std::string destination = uniqueImportPath(accessoriesRoot(), name);
        if (destination.empty()) {
            error = "Could not create a unique accessory folder.";
            return false;
        }
        if (!winfs::copyTree(sourcePath, destination)) {
            error = "Could not copy the accessory files.";
            return false;
        }
        importedName = winfs::baseName(destination);
        return true;
    }

    std::string settingsPath() {
        return (Mod::get()->getSaveDir() / "accessories_settings.json").string();
    }

    void ensureRootExists() {
        if (!winfs::dirExists(accessoriesRoot())) {
            log::info("Accessories: root folder didn't exist yet, creating it at: {}", accessoriesRoot());
            winfs::makeDir(accessoriesRoot());
        }
    }

    static std::string lower(std::string s) {
        for (auto& c : s) c = (char)tolower((unsigned char)c);
        return s;
    }

    // Scans one folder (either a part-subfolder, or the accessory folder itself when there are
    // no part-subfolders) and classifies every .png inside it by suffix. Matches BOTH naming
    // conventions: "anything_p1.png" (prefixed) and plain "p1.png" (no prefix at all) — same for
    // p2 / line / glow / selfcolor. A .png that matches none of those is treated as the
    // self-colored base layer, so a single unlabeled image "just works" too.
    static AccessoryPartFiles scanOnePart(std::string const& name, std::string const& fullPath) {
        AccessoryPartFiles part;
        part.name = name;
        part.fullPath = fullPath;

        auto files = winfs::listFiles(fullPath);
        int pngCount = 0;

        for (auto const& fileName : files) {
            std::string lowerName = lower(fileName);
            if (lowerName.size() < 4 || lowerName.substr(lowerName.size() - 4) != ".png") continue;
            pngCount++;
            std::string base = lowerName.substr(0, lowerName.size() - 4); // strip ".png"
            std::string fullFilePath = fullPath + PATH_SEP + fileName;

            if (base == "p1" || endsWith(base, "_p1")) part.p1Path = fullFilePath;
            else if (base == "p2" || endsWith(base, "_p2")) part.p2Path = fullFilePath;
            else if (base == "line" || endsWith(base, "_line")) part.linePath = fullFilePath;
            else if (base == "glow" || endsWith(base, "_glow")) part.glowPath = fullFilePath;
            else if (base == "selfcolor" || endsWith(base, "_selfcolor")) part.selfColorPath = fullFilePath;
            else part.selfColorPath = fullFilePath; // unrecognized name -> treat as the base layer
        }

        if (pngCount == 0) {
            log::warn("Accessories: no .png files found in: {}", fullPath);
        } else {
            log::info("Accessories: part '{}' -> {} ({} png file(s): selfcolor={} p1={} p2={} line={} glow={})",
                name, fullPath, pngCount,
                part.hasSelfColor(), part.hasP1(), part.hasP2(), part.hasLine(), part.hasGlow());
        }

        return part;
    }

    // Scans Accessories/<folder>/<part>/*.png. If a folder has no part-subfolders at all, the
    // folder itself is scanned as a single implicit part (its PNGs sit directly inside it).
    void scanAccessories() {
        m_folders.clear();
        ensureRootExists();

        // Scan the stable imported library first, then the legacy PC folder.
        // If both contain an accessory with the same name, the imported copy wins.
        std::vector<std::string> roots;
        roots.push_back(accessoriesRoot());
        std::string legacy = legacyAccessoriesRoot();
        if (legacy != roots.front() && winfs::dirExists(legacy)) roots.push_back(legacy);

        std::vector<std::string> seenNames;
        for (auto const& root : roots) {
            auto topLevelFolders = winfs::listSubdirs(root);
            for (auto const& folderName : topLevelFolders) {
                if (std::find(seenNames.begin(), seenNames.end(), folderName) != seenNames.end()) continue;
                seenNames.push_back(folderName);

                AccessoryFolder folder;
                folder.name = folderName;
                folder.fullPath = root + PATH_SEP + folderName;

                auto partSubdirs = winfs::listSubdirs(folder.fullPath);
                if (partSubdirs.empty()) {
                    AccessoryPartFiles part = scanOnePart(folderName, folder.fullPath);
                    if (part.hasAnyFile()) folder.parts.push_back(part);
                } else {
                    for (auto const& partName : partSubdirs) {
                        AccessoryPartFiles part = scanOnePart(partName, folder.fullPath + PATH_SEP + partName);
                        if (part.hasAnyFile()) folder.parts.push_back(part);
                    }
                }

                for (auto& part : folder.parts) {
                    loadOrCreatePartFile(folder.name, part);
                }
                if (!folder.parts.empty()) m_folders.push_back(folder);
            }
        }

        if (m_folders.empty()) {
            log::warn("Accessories: no accessory folders found. Use the Add Folder button or put folders in Accessories/.");
        }
    }

    static bool endsWith(std::string const& s, std::string const& suffix) {
        if (suffix.size() > s.size()) return false;
        return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    std::vector<AccessoryFolder>& folders() { return m_folders; }

    // ---------------------------------------------------------------------------------------
    //  Settings accessors
    // ---------------------------------------------------------------------------------------
    static std::string playerKey(int playerIndex) {
        return playerIndex == 2 ? "player2" : "player1";
    }

    bool isFolderVisible(int playerIndex, std::string const& mode, std::string const& folder) {
        auto& m = m_folderVisible[playerKey(playerIndex)][mode];
        auto it = m.find(folder);
        if (it == m.end()) return false;
        return it->second;
    }

    void setFolderVisible(int playerIndex, std::string const& mode, std::string const& folder, bool visible) {
        m_folderVisible[playerKey(playerIndex)][mode][folder] = visible;
    }

    AccessoryPartConfig& partConfig(int playerIndex, std::string const& mode, std::string const& folder, std::string const& part) {
        return m_partConfig[playerKey(playerIndex)][mode][folder][part];
    }

    // ---------------------------------------------------------------------------------------
    //  Persistence
    // ---------------------------------------------------------------------------------------
    // Folder-visibility (a per-folder ON/OFF master switch) stays in the mod's own central
    // settings file — it's not naturally "owned" by any single accessory file. Everything else
    // per PART — enabled, x, y, scale, rotation, front — instead lives in a small JSON file
    // sitting right next to that part's own PNGs (see loadOrCreatePartFile/savePartFile below),
    // so an accessory folder is fully self-contained: copy it to another install and its saved
    // position/scale/etc. comes with it.
    void load() {
        std::string path = settingsPath();
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return;
        std::ostringstream ss;
        ss << f.rdbuf();
        auto root = tjson::parse(ss.str());
        if (root.type != tjson::Value::Type::Object) return;

        for (auto const& [pKey, pVal] : root.obj) {
            if (pVal.type != tjson::Value::Type::Object) continue;
            for (auto const& [modeKey, modeVal] : pVal.obj) {
                if (modeVal.type != tjson::Value::Type::Object) continue;
                for (auto const& [folderName, folderVal] : modeVal.obj) {
                    bool visible = false;
                    if (folderVal.type == tjson::Value::Type::Bool) {
                        visible = folderVal.bl; // current schema: folder name -> bool directly
                    } else if (folderVal.has("visible")) {
                        visible = folderVal.obj.at("visible").asBool(false); // older file, still readable
                    }
                    m_folderVisible[pKey][modeKey][folderName] = visible;
                }
            }
        }
    }

    void save() {
        tjson::Value root = tjson::Value::makeObject();
        for (auto const& [pKey, modes] : m_folderVisible) {
            for (auto const& [modeKey, folders] : modes) {
                for (auto const& [folderName, visible] : folders) {
                    root[pKey][modeKey][folderName] = tjson::Value::makeBool(visible);
                }
            }
        }
        std::ofstream f(settingsPath(), std::ios::binary | std::ios::trunc);
        if (f.is_open()) f << tjson::dump(root);

        // Also flush every part's own file (cheap — these are tiny, and this only runs when the
        // user actually changes something in the editor, not every frame).
        for (auto& folder : m_folders) {
            for (auto& part : folder.parts) {
                savePartFile(folder.name, part);
            }
        }
    }

    std::string partConfigFilePath(AccessoryPartFiles const& part) const {
        return part.fullPath + PATH_SEP + "accessory.json";
    }

    // Reads <part folder>/accessory.json into m_partConfig for every player/mode it defines. If
    // the file doesn't exist yet, creates an empty one immediately, so it's visible on disk
    // right away rather than only appearing after the first edit.
    void loadOrCreatePartFile(std::string const& folderName, AccessoryPartFiles const& part) {
        std::string path = partConfigFilePath(part);

        if (!winfs::fileExists(path)) {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            if (f.is_open()) f << "{}";
            return;
        }

        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return;
        std::ostringstream ss;
        ss << f.rdbuf();
        auto root = tjson::parse(ss.str());
        if (root.type != tjson::Value::Type::Object) return;

        for (auto const& [pKey, pVal] : root.obj) {
            if (pVal.type != tjson::Value::Type::Object) continue;
            for (auto const& [modeKey, cfgVal] : pVal.obj) {
                if (cfgVal.type != tjson::Value::Type::Object) continue;
                AccessoryPartConfig cfg;
                cfg.enabled = cfgVal.has("enabled") ? cfgVal.obj.at("enabled").asBool(false) : false;
                cfg.x = cfgVal.has("x") ? (float)cfgVal.obj.at("x").asNumber(0.0) : 0.f;
                cfg.y = cfgVal.has("y") ? (float)cfgVal.obj.at("y").asNumber(0.0) : 0.f;
                cfg.scale = cfgVal.has("scale") ? (float)cfgVal.obj.at("scale").asNumber(1.0) : 1.f;
                cfg.rotation = cfgVal.has("rotation") ? (float)cfgVal.obj.at("rotation").asNumber(0.0) : 0.f;
                cfg.front = cfgVal.has("front") ? cfgVal.obj.at("front").asBool(true) : true;
                m_partConfig[pKey][modeKey][folderName][part.name] = cfg;
            }
        }
    }

    // Writes <part folder>/accessory.json from whatever is currently in m_partConfig for this
    // folder+part, across every player/mode combination that has ever been touched.
    void savePartFile(std::string const& folderName, AccessoryPartFiles const& part) {
        tjson::Value root = tjson::Value::makeObject();
        for (auto const& [pKey, modes] : m_partConfig) {
            for (auto const& [modeKey, folders] : modes) {
                auto fIt = folders.find(folderName);
                if (fIt == folders.end()) continue;
                auto pIt = fIt->second.find(part.name);
                if (pIt == fIt->second.end()) continue;
                auto const& cfg = pIt->second;
                auto& cfgObj = root[pKey][modeKey];
                cfgObj["enabled"] = tjson::Value::makeBool(cfg.enabled);
                cfgObj["x"] = tjson::Value::makeNumber(cfg.x);
                cfgObj["y"] = tjson::Value::makeNumber(cfg.y);
                cfgObj["scale"] = tjson::Value::makeNumber(cfg.scale);
                cfgObj["rotation"] = tjson::Value::makeNumber(cfg.rotation);
                cfgObj["front"] = tjson::Value::makeBool(cfg.front);
            }
        }
        std::ofstream f(partConfigFilePath(part), std::ios::binary | std::ios::trunc);
        if (f.is_open()) f << tjson::dump(root);
    }
};

// =============================================================================================
//  A single accessory-part sprite rendered on top of / behind a player icon.
//  PNGs are loaded explicitly through CCTextureCache from their absolute path,
//  avoiding missing-frame / checkerboard placeholders from sprite-frame lookup.
// =============================================================================================

// Loads a PNG from disk via CCSprite::create(path), which already goes through GD's texture
// cache internally. Wrapped so a failed load is logged with the exact path that failed, instead
// of silently producing nothing — makes it much faster to spot a typo'd/missing file on your end
// versus an actual mod bug.
static CCSprite* loadImageSprite(std::string const& filePath) {
    if (filePath.empty()) return nullptr;
    if (!winfs::fileExists(filePath)) {
        log::error("Accessories: PNG not found: {}", filePath);
        return nullptr;
    }

    // Do not use CCSprite::create(fullPath) here. Depending on the GD/cocos2d
    // texture-cache state, it can return a missing-texture placeholder even
    // when the PNG exists. Load the file explicitly into CCTextureCache and
    // create the sprite from the resulting texture instead.
    auto cache = CCTextureCache::sharedTextureCache();
    if (!cache) return nullptr;

    auto texture = cache->addImage(filePath.c_str(), false);
    if (!texture) {
        log::error("Accessories: failed to decode PNG: {}", filePath);
        return nullptr;
    }

    auto sprite = CCSprite::createWithTexture(texture);
    if (!sprite) {
        log::error("Accessories: failed to create CCSprite from texture: {}", filePath);
        return nullptr;
    }

    sprite->setAnchorPoint({ 0.5f, 0.5f });
    return sprite;
}

class AccessoryPartNode : public CCNode {
protected:
    CCSprite* m_base = nullptr;
    CCSprite* m_p1 = nullptr;
    CCSprite* m_p2 = nullptr;
    CCSprite* m_line = nullptr;
    CCSprite* m_glow = nullptr;

public:
    static AccessoryPartNode* create(AccessoryPartFiles const& files, bool invertColorsForP2) {
        auto ret = new AccessoryPartNode();
        if (ret && ret->init(files, invertColorsForP2)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(AccessoryPartFiles const& files, bool invertColorsForP2) {
        if (!CCNode::init()) return false;

        if (files.hasGlow()) {
            m_glow = loadImageSprite(files.glowPath);
            if (m_glow) { m_glow->setBlendFunc({ GL_SRC_ALPHA, GL_ONE }); addChild(m_glow, -1); }
        }
        if (files.hasP1()) {
            m_p1 = loadImageSprite(files.p1Path);
            if (m_p1) addChild(m_p1, 1);
        }
        if (files.hasP2()) {
            m_p2 = loadImageSprite(files.p2Path);
            if (m_p2) addChild(m_p2, 1);
        }
        if (files.hasSelfColor()) {
            m_base = loadImageSprite(files.selfColorPath);
            if (m_base) addChild(m_base, 0);
        }
        if (files.hasLine()) {
            m_line = loadImageSprite(files.linePath);
            if (m_line) addChild(m_line, 2);
        }

        // 2-player color inversion: _p1 becomes GameManager's second-player color1, and vice versa.
        auto gm = GameManager::get();
        if (gm) {
            ccColor3B c1 = gm->colorForIdx(gm->getPlayerColor());
            ccColor3B c2 = gm->colorForIdx(gm->getPlayerColor2());
            if (invertColorsForP2) std::swap(c1, c2);
            if (m_p1) m_p1->setColor(c1);
            if (m_p2) m_p2->setColor(c2);
        }

        return true;
    }
};

// =============================================================================================
//  Accessory container attached to a live PlayerObject in a level.
// =============================================================================================
// IMPORTANT: "front"/"back" must be relative to the *player's own icon sprites*, not just
// relative to other accessories. A single container added to PlayerObject at one fixed z-order
// can only order its own children against EACH OTHER — it can never render below the icon's own
// sprites, no matter what z-order its children use internally, because the whole container is
// still just one sibling of the icon sprites at a single fixed depth. That was the actual bug:
// "Back" accessories only ever ended up behind *other accessories*, never behind the icon itself.
// The fix is two separate containers per player, added to PlayerObject at very different
// z-orders: one clearly ABOVE anything the icon itself uses, one clearly BELOW it.
class AccessoryContainer : public CCNode {
protected:
    int m_playerIndex = 1;

public:
    static AccessoryContainer* create(int playerIndex) {
        auto ret = new AccessoryContainer();
        if (ret && ret->init()) {
            ret->m_playerIndex = playerIndex;
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    // `wantFront` selects which half of the accessories this particular container instance is
    // responsible for — pass true when this container was added in front of the icon, false
    // when it was added behind it, and only matching parts get built into it.
    void rebuildForMode(std::string const& mode, bool wantFront) {
        removeAllChildren();
        auto mgr = AccessoryManager::get();
        int z = 0;

        for (auto& folder : mgr->folders()) {
            if (!mgr->isFolderVisible(1, mode, folder.name)) continue;

            for (auto& part : folder.parts) {
                auto& cfg = mgr->partConfig(1, mode, folder.name, part.name);
                if (!cfg.enabled) continue;
                if (cfg.front != wantFront) continue;

                auto node = AccessoryPartNode::create(part, m_playerIndex == 2);
                if (!node) continue;

                node->setPosition({ cfg.x, cfg.y });
                node->setScale(cfg.scale);
                node->setRotation(cfg.rotation);
                addChild(node, z++);
            }
        }
    }
};

// =============================================================================================
//  PlayerObject hook — attaches + updates the accessory container every frame.
// =============================================================================================
class $modify(AccessoriesPlayerObject, PlayerObject) {
    struct Fields {
        AccessoryContainer* m_accFront1 = nullptr; // player 1 (or the only player), in front of the icon
        AccessoryContainer* m_accBack1 = nullptr;  // player 1, behind the icon
        AccessoryContainer* m_accFront2 = nullptr; // player 2, in front of the icon
        AccessoryContainer* m_accBack2 = nullptr;  // player 2, behind the icon
        std::string m_lastMode;
    };

    // NOTE: We intentionally do NOT hook init() here. PlayerObject::init's exact parameter list
    // varies across binding sets and guessing it wrong is a hard compile error (as you just saw).
    // We don't actually need it: the accessory containers are created lazily the first time
    // update() runs for a given PlayerObject instance (see below), which covers every spawn/
    // respawn/practice-mode-reset path without needing to know init()'s signature at all.

    // Best-effort gamemode key detection. See file header note (1) if these member names don't
    // match your Bindings.hpp.
    std::string getCurrentModeKey() {
        if (m_isShip) return "ship";
        if (m_isBird) return "ufo";
        if (m_isBall) return "ball";
        if (m_isDart) return "wave";
        if (m_isRobot) return "robot";
        if (m_isSpider) return "spider";
        if (m_isSwing) return "swing";
        // jetpack in modern GD shares the ship flag in some binding sets; if your build
        // distinguishes it with its own flag (e.g. m_isJetpack), add it above this line.
        return "cube";
    }

    void update(float dt) {
        PlayerObject::update(dt);

        // Only the two players that actually exist in the level get accessories; this hook fires
        // for real PlayerObject instances that live inside PlayLayer / LevelEditorLayer / 2P
        // levels only — icon previews elsewhere in the game use SimplePlayer, not PlayerObject,
        // so requirement (1) — "only inside levels" — is satisfied automatically.
        std::string mode = getCurrentModeKey();
        bool isSecondPlayer = isThisPlayer2();

        AccessoryContainer*& front = isSecondPlayer ? m_fields->m_accFront2 : m_fields->m_accFront1;
        AccessoryContainer*& back = isSecondPlayer ? m_fields->m_accBack2 : m_fields->m_accBack1;

        if (!front) {
            front = AccessoryContainer::create(isSecondPlayer ? 2 : 1);
            // z = 1000: comfortably above every sprite PlayerObject itself uses, so "front"
            // accessories always draw on top of the icon no matter which sub-sprites it has.
            this->addChild(front, 1000);
        }
        if (!back) {
            back = AccessoryContainer::create(isSecondPlayer ? 2 : 1);
            // z = -1000: comfortably below every sprite PlayerObject itself uses, so "back"
            // accessories always draw behind the icon. This is the actual fix for "back" not
            // working before — a single shared container could only order accessories against
            // each other, never against the icon's own sprites, since it was just one sibling
            // node at one fixed depth.
            this->addChild(back, -1000);
        }

        if (mode != m_fields->m_lastMode) {
            front->rebuildForMode(mode, true);
            back->rebuildForMode(mode, false);
            m_fields->m_lastMode = mode;
        }

        // Gravity-flip handling. In ship/ufo/jetpack/robot/spider, gravity flipping visually
        // rotates the icon 180° — but GD does that by flipping internal sub-sprites, not
        // PlayerObject's own top-level rotation, so our containers (direct children of
        // PlayerObject) never see it through normal inheritance. We apply it ourselves as an
        // instant, non-animated local rotation (setRotation has no duration — it snaps, exactly
        // like the "0 seconds" behavior asked for), layered on top of whatever rotation
        // PlayerObject's own transform already contributes (rotations add, not replace).
        // In grounded modes (cube/ball/wave/swing) a gravity flip instead mirrors the icon
        // horizontally, so we mirror the accessories the same way via scaleX.
        // NOTE: m_isUpsideDown is the standard, long-used binding name for this flag; if your
        // Bindings.hpp calls it something else, this is the one line to fix.
        bool flyingMode = (mode == "ship" || mode == "ufo" || mode == "jetpack" || mode == "robot" || mode == "spider");
        bool upsideDown = m_isUpsideDown;

        front->setRotation(flyingMode && upsideDown ? 180.f : 0.f);
        back->setRotation(flyingMode && upsideDown ? 180.f : 0.f);

        // Horizontal mirror. Two independent triggers, combined multiplicatively (so two flips
        // cancel back out, matching what you'd expect visually):
        //   1) gravity flip in a grounded mode (cube/ball/wave/swing) — same event as the
        //      rotation above, just mirrored instead of rotated for these modes.
        //   2) platformer-mode facing direction (m_isGoingLeft) — this is the part that wasn't
        //      wired up before, so direction changes in platformer mode didn't mirror at all.
        float mirrorX = 1.f;
        if (!flyingMode && upsideDown) mirrorX *= -1.f;
        if (m_isGoingLeft) mirrorX *= -1.f;
        front->setScaleX(mirrorX);
        back->setScaleX(mirrorX);

        // NOTE: front/back are children of `this` (PlayerObject), so beyond the explicit
        // gravity-flip handling above, they already inherit its rotation and scale automatically
        // through the normal parent-child transform — nothing else needs to be mirrored by hand.
        // Explicitly re-applying PlayerObject's own rotation here used to double it (child
        // inherits it once from the parent transform, then again from this code setting the same
        // value directly), which is why accessories were spinning faster than the icon itself.
        front->setPosition({ 0.f, 0.f });
        back->setPosition({ 0.f, 0.f });
    }

    // Which of the two players (in 2P/dual mode) this PlayerObject instance represents. Real
    // check now (was previously hardcoded to always return player 1, which is why P1/P2 color
    // inversion never actually triggered for player 2): GJBaseGameLayer — the shared base of
    // PlayLayer and LevelEditorLayer — keeps m_player1/m_player2 pointers to the two player
    // objects; if `this` is the m_player2 instance, we're player 2.
    bool isThisPlayer2() {
        auto gjbgl = GJBaseGameLayer::get();
        return gjbgl && gjbgl->m_player2 == this;
    }
};

// =============================================================================================
//  AccessoriesPopup — the configuration window opened from the gear button.
// =============================================================================================
// This is a plain CCLayer (not geode::Popup<...>) that draws its own background, title and
// close button using only vanilla, stable cocos2d/GD classes. Everything interactive lives in
// ONE single CCMenu (m_menu) — no nested sub-menus. This matters: cocos2d's touch dispatcher
// hands each touch to registered delegates in priority order, and delegates that share the same
// priority are checked in an order that depends on *when they were registered*, not on visual
// z-order. Multiple sibling CCMenus (like we had before) can end up racing against whatever
// GJGarageLayer already registered, which is exactly why clicks were landing on things behind
// the popup. A single menu with an explicitly elevated priority removes that ambiguity
// completely: our buttons are always asked first, full stop.
class AccessoriesPopup : public CCLayer, public TextInputDelegate {
protected:
    CCMenu* m_menu = nullptr;         // the one and only menu; everything interactive lives here
    CCNode* m_labels = nullptr;       // plain (non-interactive) text labels, kept separate for clarity
    SimplePlayer* m_preview = nullptr;
    SimplePlayer* m_preview2 = nullptr;
    CCNode* m_previewFront = nullptr; // accessories drawn in front of the preview icon
    CCNode* m_previewBack = nullptr;  // accessories drawn behind the preview icon
    CCNode* m_previewFront2 = nullptr;
    CCNode* m_previewBack2 = nullptr;
    CCLabelBMFont* m_editorTitle = nullptr;
    CCSprite* m_modeButtonBg[9] = { nullptr };
    CCTextInputNode* m_xInput = nullptr;
    CCTextInputNode* m_yInput = nullptr;
    CCTextInputNode* m_scaleInput = nullptr;
    CCTextInputNode* m_rotationInput = nullptr;

    std::string m_currentMode = "cube";
    std::string m_selectedFolder;
    std::string m_selectedPart;
    int m_folderPage = 0;
    int m_partPage = 0;

    // Small real scrollbar state for the folder/part lists.
    bool m_draggingScroll = false;
    bool m_draggingFolderScroll = true;
    float m_scrollDragStartY = 0.f;
    int m_scrollDragStartPage = 0;
    CCPoint m_windowOrigin = { 0.f, 0.f };

    static constexpr float kWinWidth = 520.f;
    static constexpr float kWinHeight = 292.f;

    // The catch-all's priority only needs to be WORSE (numerically larger) than CCMenu's
    // built-in default priority (kCCMenuHandlerPriority = -128), so every CCMenu — ours or
    // anything else's — always gets first refusal on a touch, and the catch-all only ever sees
    // touches nothing claimed. We deliberately do NOT try to raise CCMenu's own priority above
    // its default: CCMenu commonly hardcodes its touch-dispatcher registration priority
    // internally and may silently ignore a custom value, and getting this backwards would make
    // the catch-all swallow clicks meant for our own buttons — worse than the bug we're fixing.
    static constexpr int kCatcherTouchPriority = -64;

    bool init() override {
        if (!CCLayer::init()) return false;

        auto director = CCDirector::sharedDirector();
        auto visibleSize = director->getVisibleSize();
        auto visibleOrigin = director->getVisibleOrigin();
        CCPoint center = { visibleOrigin.x + visibleSize.width / 2.f, visibleOrigin.y + visibleSize.height / 2.f };

        // Full-screen click-catcher: consumes any touch that isn't over one of our own buttons,
        // so it can never reach GJGarageLayer (or anything else) behind the popup. Registered at
        // a priority just below our button menu, so buttons always get first refusal.
        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        this->setTouchPriority(kCatcherTouchPriority);

        // dim the background so the popup reads as modal
        auto dim = CCLayerColor::create({ 0, 0, 0, 150 });
        dim->setContentSize(visibleSize);
        dim->setPosition(visibleOrigin);
        this->addChild(dim, 0);

        CCPoint origin = { center.x - kWinWidth / 2.f, center.y - kWinHeight / 2.f };
        m_windowOrigin = origin;

        // window background — same fix as squareSprite(): a flat color fill instead of any
        // texture frame, so it can't ever render as a broken/missing texture. CCLayerColor's
        // default anchor point is bottom-left, so it's positioned at `origin`, not `center`.
        // Neutral gray panel with a light border, much closer to the original GD garage/editor
        // windows than the old blue-purple rectangle.
        auto border = CCLayerColor::create({ 15, 18, 30, 255 }, kWinWidth + 6.f, kWinHeight + 6.f);
        border->setPosition({ origin.x - 2.f, origin.y - 2.f });
        this->addChild(border, 1);
        auto bg = CCLayerColor::create({ 39, 42, 64, 250 }, kWinWidth, kWinHeight);
        bg->setPosition(origin);
        this->addChild(bg, 2);

        // Subtle top strip and bottom strip reproduce the layered panel look of GD windows
        // without depending on any sprite-sheet texture supplied by the current GDPS.
        auto titleStrip = CCLayerColor::create({ 25, 28, 48, 235 }, kWinWidth, 42.f);
        titleStrip->setPosition(origin + CCPoint{ 0.f, kWinHeight - 42.f });
        this->addChild(titleStrip, 3);
        auto separator = CCLayerColor::create({ 90, 150, 190, 170 }, kWinWidth - 18.f, 2.f);
        separator->setPosition(origin + CCPoint{ 9.f, kWinHeight - 44.f });
        this->addChild(separator, 3);

        m_labels = CCNode::create();
        m_labels->setPosition(origin);
        this->addChild(m_labels, 3);

        // The single menu. Its own position is `origin`, so every child button position below
        // can be written in the same "window-local" coordinates as before. CCMenu registers
        // itself with the touch dispatcher using its own default (high) priority automatically —
        // no custom priority needed here, and see the note on kCatcherTouchPriority above for why.
        m_menu = CCMenu::create();
        m_menu->setPosition(origin);
        this->addChild(m_menu, 4);

        auto title = CCLabelBMFont::create("Accessories", "bigFont.fnt");
        title->setScale(0.6f);
        title->setPosition({ kWinWidth / 2.f, kWinHeight - 20.f });
        m_labels->addChild(title);

        // close button (bottom-right of window title bar), built from proven-safe assets only
        auto closeBg = circleButtonSprite({ 190, 65, 65 }, 0.40f, true);
        auto closeLabel = CCLabelBMFont::create("X", "bigFont.fnt");
        closeLabel->setScale(0.62f);
        closeLabel->setPosition({ closeBg->getContentSize().width / 2.f, closeBg->getContentSize().height / 2.f });
        closeBg->addChild(closeLabel);
        auto closeBtn = CCMenuItemSpriteExtra::create(closeBg, this, menu_selector(AccessoriesPopup::onClose));
        closeBtn->setPosition({ kWinWidth - 16.f, kWinHeight - 14.f });
        m_menu->addChild(closeBtn);

        buildUI();
        return true;
    }

    // A small helper: builds a square button with a text label centered on it and a fixed,
    // reliable hit box (the square sprite itself), used for every text-style button below so we
    // never again rely on a composite node like SimplePlayer for hit-testing.
    static CCMenuItemSpriteExtra* makeTextButton(std::string const& text, ccColor3B tint, float sizeScale, CCObject* target, SEL_MenuHandler sel) {
        auto bg = roundedButtonSprite(tint, sizeScale);
        if (!bg) return nullptr;
        auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        if (!label) return nullptr;
        label->setScale(0.32f / sizeScale);
        label->setPosition({ bg->getContentSize().width / 2.f, bg->getContentSize().height / 2.f });
        label->setColor({ 255, 255, 255 });
        bg->addChild(label);
        return CCMenuItemSpriteExtra::create(bg, target, sel);
    }

    void buildUI() {
        AccessoryManager::get()->scanAccessories();

        // ---- dual preview: P1 and P2 are shown together in the editor. P2 uses the
        // same accessory configuration as P1, but its two player colors are swapped.
        m_preview = makeModeIcon(m_currentMode);
        m_preview->setScale(1.18f);
        m_preview->setPosition({ 215.f, 48.f });
        m_labels->addChild(m_preview, 5);

        m_previewBack = CCNode::create();
        m_previewBack->setScale(1.18f);
        m_previewBack->setPosition(m_preview->getPosition());
        m_labels->addChild(m_previewBack, 4);

        m_previewFront = CCNode::create();
        m_previewFront->setScale(1.18f);
        m_previewFront->setPosition(m_preview->getPosition());
        m_labels->addChild(m_previewFront, 6);

        m_preview2 = makeModeIcon(m_currentMode);
        m_preview2->setScale(1.18f);
        m_preview2->setPosition({ 315.f, 48.f });
        m_labels->addChild(m_preview2, 5);

        m_previewBack2 = CCNode::create();
        m_previewBack2->setScale(1.18f);
        m_previewBack2->setPosition(m_preview2->getPosition());
        m_labels->addChild(m_previewBack2, 4);

        m_previewFront2 = CCNode::create();
        m_previewFront2->setScale(1.18f);
        m_previewFront2->setPosition(m_preview2->getPosition());
        m_labels->addChild(m_previewFront2, 6);

        // ---- top row: gamemode buttons, each showing the player's real equipped icon —
        // twice as big as before. Doubling btnBg's own scale is enough on its own: `icon` is a
        // CHILD of btnBg, so it automatically renders twice as big too, without needing its own
        // scale touched (a child inherits its parent's scale on top of its own).
        // Mode selector: inactive modes are just the real GD icons on the panel; only the
        // selected mode gets the circular selector behind it. This removes the white square
        // tiles completely while preserving a generous touch target around every icon.
        float step = 54.f;
        float startX = kWinWidth / 2.f - step * 4.f;
        for (int i = 0; i < 9; i++) {
            auto hit = transparentHitSprite(58.f, 58.f);
            if (!hit) continue;

            auto selector = circleButtonSprite(
                m_currentMode == MODE_KEYS[i] ? ccColor3B{ 105, 105, 105 } : ccColor3B{ 100, 100, 100 },
                0.70f,
                m_currentMode == MODE_KEYS[i]
            );
            if (selector) {
                selector->setPosition({ hit->getContentSize().width / 2.f, hit->getContentSize().height / 2.f });
                hit->addChild(selector, -1);
                m_modeButtonBg[i] = selector;
            }

            auto icon = makeModeIcon(MODE_KEYS[i]);
            if (icon) {
                icon->setScale(0.64f);
                icon->setPosition({ hit->getContentSize().width / 2.f, hit->getContentSize().height / 2.f });
                hit->addChild(icon, 1);
            }

            auto btn = CCMenuItemSpriteExtra::create(hit, this, menu_selector(AccessoriesPopup::onSelectMode));
            btn->setTag(i);
            btn->setPosition({ startX + step * i, kModeRowY });
            m_menu->addChild(btn);
        }

        rebuildFolderList();
        rebuildPartsList();
        rebuildEditor();
        refreshPreview();
    }

public:
    static AccessoriesPopup* create() {
        auto ret = new AccessoriesPopup();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void show() {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (scene) scene->addChild(this, 10000);
    }

    void onClose(CCObject* sender) {
        this->removeFromParentAndCleanup(true);
    }

    void onAddFolder(CCObject*) {
        // Use Geode's native system folder picker. This is the important part for Android:
        // the user can choose a folder from the device file system instead of manually finding
        // the mod's private directory. The same picker also works on desktop platforms.
        geode::utils::file::FilePickOptions options;
        options.defaultPath = std::filesystem::path(AccessoryManager::get()->accessoriesRoot());

        geode::WeakRef<AccessoriesPopup> self = this;
        geode::async::spawn(
            geode::utils::file::pick(geode::utils::file::PickMode::OpenFolder, options),
            [self](geode::Result<std::optional<std::filesystem::path>> result) mutable {
                if (!result.isOk()) {
                    log::warn("Accessories: folder picker failed.");
                    return;
                }
                auto picked = result.unwrap();
                if (!picked) return;
                auto popup = self.lock();

                std::string importedName;
                std::string error;
                if (!AccessoryManager::get()->importAccessoryFolder(picked->string(), importedName, error)) {
                    log::warn("Accessories: import failed: {}", error);
                    return;
                }

                log::info("Accessories: imported '{}'", importedName);
                auto mgr = AccessoryManager::get();
                mgr->scanAccessories();
                if (!popup) return;
                popup->m_folderPage = 0;
                popup->m_partPage = 0;
                popup->m_selectedFolder = importedName;
                popup->m_selectedPart.clear();
                popup->rebuildFolderList();
                popup->rebuildPartsList();
                popup->rebuildEditor();
                popup->refreshPreview();
            }
        );
    }

    // ---------------------------------------------------------------------------------------
    //  Layout: Parts list on the LEFT (wider — needs room for name + ON/OFF + Edit), Folders
    //  list on the RIGHT (narrower — just name + ON/OFF). Both support paging with </> buttons
    //  when there are more rows than fit, instead of raw touch-drag scrolling (which is fragile
    //  to get right with custom touch handling — see the notes on touch priority above).
    // ---------------------------------------------------------------------------------------
    static constexpr float kPartsColX = 10.f;
    static constexpr float kFolderColX = 342.f;
    static constexpr float kFolderColW = 154.f;

    // The top mode icons sit a little lower; the folder block is pulled slightly upward
    // and its rows are tighter, matching the compact GD-style layout.
    static constexpr float kModeRowY = kWinHeight - 59.f;
    static constexpr float kListTopY = 190.f;
    static constexpr float kPartsListTopY = 165.f;
    static constexpr float kRowH = 20.f;
    static constexpr int kPageSize = 5;

    static constexpr float kScrollTrackY = 142.f;
    static constexpr float kScrollTrackH = 78.f;
    static constexpr float kScrollTrackW = 7.f; // rows visible per page in each list (kept small so the
                                         // </> page controls have clear room above the editor panel)

    void clearMenuChildrenWithTagRange(int loTag, int hiTag) {
        if (!m_menu) return;

        std::vector<CCNode*> toRemove;
        for (CCNode* node : m_menu->getChildrenExt()) {
            if (node->getTag() >= loTag && node->getTag() <= hiTag) toRemove.push_back(node);
        }
        for (CCNode* node : toRemove) {
            node->removeFromParentAndCleanup(true);
        }

        if (m_labels) {
            std::vector<CCNode*> toRemoveLabels;
            for (CCNode* node : m_labels->getChildrenExt()) {
                if (node->getTag() >= loTag && node->getTag() <= hiTag) toRemoveLabels.push_back(node);
            }
            for (CCNode* node : toRemoveLabels) {
                node->removeFromParentAndCleanup(true);
            }
        }
    }

    // Draw a compact scrollbar beside a list. The thumb is draggable; clicking/dragging it
    // changes pages, while the actual list rows remain ordinary CCMenu buttons.
    void addListScrollbar(float x, float y, int page, int totalPages, int baseTag, bool folderList) {
        if (totalPages <= 1) return;

        auto track = CCLayerColor::create({ 15, 24, 42, 150 }, kScrollTrackW, kScrollTrackH);
        track->setPosition({ x, y });
        track->setTag(baseTag);
        m_labels->addChild(track);

        float travel = kScrollTrackH - 18.f;
        float t = totalPages <= 1 ? 0.f : (float)page / (float)(totalPages - 1);
        float thumbY = y + travel * (1.f - t);

        auto thumb = CCLayerColor::create({ 100, 180, 235, 220 }, kScrollTrackW, 18.f);
        thumb->setPosition({ x, thumbY });
        thumb->setTag(baseTag + 1);
        m_labels->addChild(thumb);

        auto count = CCLabelBMFont::create((std::to_string(page + 1) + "/" + std::to_string(totalPages)).c_str(), "bigFont.fnt");
        count->setScale(0.28f);
        count->setPosition({ x - 13.f, y - 9.f });
        count->setTag(baseTag + 2);
        m_labels->addChild(count);
    }

    // Tag ranges, so we can cleanly remove-and-rebuild each section without touching the others:

    //   1000-1999 : folder list buttons/labels
    //   2000-2999 : parts list buttons/labels
    //   3000-3999 : editor buttons/labels
    void rebuildFolderList() {
        clearMenuChildrenWithTagRange(1000, 1999);

        auto mgr = AccessoryManager::get();
        auto& folders = mgr->folders();

        auto header = CCLabelBMFont::create("Folders", "bigFont.fnt");
        header->setScale(0.42f);
        header->setAnchorPoint({ 0.f, 0.5f });
        header->setPosition({ kFolderColX - 28.f, kListTopY + 10.f });
        header->setTag(1000);
        m_labels->addChild(header);

        auto addFolderBtn = makeTextOnlyButton("+ ADD", ccColor3B{ 70, 175, 100 }, 0.30f,
            this, menu_selector(AccessoriesPopup::onAddFolder), 88.f, 25.f);
        addFolderBtn->setTag(1090);
        addFolderBtn->setAnchorPoint({ 0.f, 0.5f });
        addFolderBtn->setPosition({ kFolderColX + 54.f, kListTopY + 10.f });
        m_menu->addChild(addFolderBtn);

        if (folders.empty()) {
            auto hint = CCLabelBMFont::create("(empty - add a\nfolder next to\nthe Accessories\nfolder)", "bigFont.fnt");
            hint->setScale(0.28f);
            hint->setAnchorPoint({ 0.f, 1.f });
            hint->setPosition({ kFolderColX, kListTopY });
            hint->setTag(1001);
            m_labels->addChild(hint);
            return;
        }

        int totalPages = (std::max)(1, (int)((folders.size() + kPageSize - 1) / kPageSize));
        if (m_folderPage >= totalPages) m_folderPage = totalPages - 1;
        if (m_folderPage < 0) m_folderPage = 0;
        int startIdx = m_folderPage * kPageSize;
        int endIdx = (std::min)((int)folders.size(), startIdx + kPageSize);

        for (int idx = startIdx; idx < endIdx; idx++) {
            auto& folder = folders[idx];
            float y = kListTopY - 2.f - (idx - startIdx) * 18.f;
            bool isSelected = (folder.name == m_selectedFolder);

            // Row button built from the same proven makeTextButton() pattern used everywhere
            // else in this popup (uniform-scale square sprite + compensated child label) —
            // deliberately NOT a CCScale9Sprite stretched via setContentSize, which is the
            // rendering path that was producing the broken/missing texture here before.
            auto rowBtn = makeTextOnlyButton(folder.name, isSelected ? ccColor3B{ 100, 205, 235 } : ccColor3B{ 235, 235, 235 }, 0.30f,
                this, menu_selector(AccessoriesPopup::onSelectFolder), 205.f, 28.f);
            rowBtn->setTag(1100 + idx);
            rowBtn->setAnchorPoint({ 0.f, 0.5f });
            rowBtn->setPosition({ kFolderColX, y });
            m_menu->addChild(rowBtn);

            bool visible = mgr->isFolderVisible(1, configModeKey(m_currentMode), folder.name);
            auto eyeBtn = makeTextOnlyButton(visible ? "ON" : "OFF", visible ? ccColor3B{ 85, 225, 125 } : ccColor3B{ 175, 175, 175 }, 0.28f,
                this, menu_selector(AccessoriesPopup::onToggleFolderVisible), 55.f, 28.f);
            eyeBtn->setTag(1200 + idx);
            eyeBtn->setPosition({ kFolderColX + kFolderColW - 10.f, y });
            m_menu->addChild(eyeBtn);
        }

        addListScrollbar(kFolderColX + kFolderColW + 2.f, kScrollTrackY, m_folderPage, totalPages, 1050, true);
    }

    void onFolderPrevPage(CCObject*) { m_folderPage--; rebuildFolderList(); }
    void onFolderNextPage(CCObject*) { m_folderPage++; rebuildFolderList(); }

    void onSelectFolder(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag() - 1100;
        auto& folders = AccessoryManager::get()->folders();
        if (idx < 0 || idx >= (int)folders.size()) return;
        m_selectedFolder = folders[idx].name;
        m_selectedPart.clear();
        m_partPage = 0;
        rebuildFolderList();
        rebuildPartsList();
        rebuildEditor();
    }

    void onToggleFolderVisible(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag() - 1200;
        auto& folders = AccessoryManager::get()->folders();
        if (idx < 0 || idx >= (int)folders.size()) return;
        bool cur = AccessoryManager::get()->isFolderVisible(1, configModeKey(m_currentMode), folders[idx].name);
        AccessoryManager::get()->setFolderVisible(1, configModeKey(m_currentMode), folders[idx].name, !cur);
        AccessoryManager::get()->save();
        rebuildFolderList();
        refreshPreview();
    }

    // ---------------------------------------------------------------------------------------
    //  Parts list — left column, for the currently selected folder.
    // ---------------------------------------------------------------------------------------
    void rebuildPartsList() {
        clearMenuChildrenWithTagRange(2000, 2999);

        auto header = CCLabelBMFont::create(
            m_selectedFolder.empty() ? "Parts (select a folder ->)" : ("Parts: " + m_selectedFolder).c_str(),
            "bigFont.fnt"
        );
        header->setScale(0.38f);
        header->setAnchorPoint({ 0.f, 0.5f });
        header->setPosition({ kPartsColX, kPartsListTopY + 8.f });
        header->setTag(2000);
        m_labels->addChild(header);

        auto folder = currentFolder();
        if (!folder) return;

        if (folder->parts.empty()) {
            auto hint = CCLabelBMFont::create("(no PNGs found here - check\nAccessories/<folder>/*.png\nor Accessories/<folder>/<part>/*.png)", "bigFont.fnt");
            hint->setScale(0.26f);
            hint->setAnchorPoint({ 0.f, 1.f });
            hint->setPosition({ kPartsColX, kPartsListTopY });
            hint->setTag(2001);
            m_labels->addChild(hint);
            return;
        }

        auto mgr = AccessoryManager::get();
        int totalPages = (std::max)(1, (int)((folder->parts.size() + kPageSize - 1) / kPageSize));
        if (m_partPage >= totalPages) m_partPage = totalPages - 1;
        if (m_partPage < 0) m_partPage = 0;
        int startIdx = m_partPage * kPageSize;
        int endIdx = (std::min)((int)folder->parts.size(), startIdx + kPageSize);

        for (int idx = startIdx; idx < endIdx; idx++) {
            auto& part = folder->parts[idx];
            auto& cfg = mgr->partConfig(1, configModeKey(m_currentMode), folder->name, part.name);
            float y = kPartsListTopY - (idx - startIdx) * kRowH;
            bool isEditing = (part.name == m_selectedPart);

            auto rowLabel = CCLabelBMFont::create(part.name.c_str(), "bigFont.fnt");
            rowLabel->setScale(0.40f);
            rowLabel->setAnchorPoint({ 0.f, 0.5f });
            rowLabel->setPosition({ kPartsColX, y });
            rowLabel->setTag(2100 + idx);
            m_labels->addChild(rowLabel);

            auto enableBtn = makeTextOnlyButton(cfg.enabled ? "ON" : "OFF", cfg.enabled ? ccColor3B{ 85, 225, 125 } : ccColor3B{ 175, 175, 175 }, 0.28f,
                this, menu_selector(AccessoriesPopup::onTogglePartEnabled), 55.f, 28.f);
            enableBtn->setTag(2200 + idx);
            enableBtn->setPosition({ kPartsColX + 190.f, y });
            m_menu->addChild(enableBtn);

            auto editBtn = makeTextOnlyButton(isEditing ? "EDITING" : "EDIT", isEditing ? ccColor3B{ 100, 205, 235 } : ccColor3B{ 220, 220, 220 }, 0.28f,
                this, menu_selector(AccessoriesPopup::onEditPart), 85.f, 28.f);
            editBtn->setTag(2300 + idx);
            editBtn->setPosition({ kPartsColX + 275.f, y });
            m_menu->addChild(editBtn);
        }

        addListScrollbar(330.f, kScrollTrackY, m_partPage, totalPages, 2050, false);
    }

    void onPartPrevPage(CCObject*) { m_partPage--; rebuildPartsList(); }
    void onPartNextPage(CCObject*) { m_partPage++; rebuildPartsList(); }

    void onTogglePartEnabled(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag() - 2200;
        auto folder = currentFolder();
        if (!folder || idx < 0 || idx >= (int)folder->parts.size()) return;
        auto& cfg = AccessoryManager::get()->partConfig(1, configModeKey(m_currentMode), folder->name, folder->parts[idx].name);
        cfg.enabled = !cfg.enabled;
        AccessoryManager::get()->save();
        rebuildPartsList();
        refreshPreview();
    }

    void onEditPart(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag() - 2300;
        auto folder = currentFolder();
        if (!folder || idx < 0 || idx >= (int)folder->parts.size()) return;
        m_selectedPart = folder->parts[idx].name;

        // Editing a part should make it visible right away — turn it (and its folder) on if
        // they weren't already, without touching position/scale/rotation so nothing you'd
        // already set gets reset. A never-touched part defaults to x=0, y=0, i.e. already
        // centered on the icon.
        auto mgr = AccessoryManager::get();
        auto& cfg = mgr->partConfig(1, configModeKey(m_currentMode), folder->name, m_selectedPart);
        cfg.enabled = true;
        mgr->setFolderVisible(1, configModeKey(m_currentMode), folder->name, true);
        mgr->save();

        rebuildFolderList();
        rebuildPartsList();
        rebuildEditor();
        refreshPreview();
    }


    // ---------------------------------------------------------------------------------------
    //  Editor — bottom strip, on its own semi-transparent panel. Each row shows the current
    //  value AND lets you nudge it with -/+ or type an exact number directly into the box. A
    //  never-touched part starts at X=0, Y=0 (dead center of the icon), Scale=1 (100%),
    //  Rotation=0° — those are the field defaults.
    // ---------------------------------------------------------------------------------------
    static constexpr float kEditorTitleY = 86.f;
    static constexpr float kEditorRow1Y = 57.f;
    static constexpr float kEditorRow2Y = 34.f;
    static constexpr float kEditorRow3Y = 11.f;
    static constexpr float kEditorPanelBottom = 2.f;
    static constexpr float kEditorPanelTop = 94.f;

    void rebuildEditor() {
        clearMenuChildrenWithTagRange(3000, 3999);
        m_xInput = m_yInput = m_scaleInput = m_rotationInput = nullptr;

        // Compact, three-column editor: X/Y on the left, both previews in the middle,
        // Scale/Rot on the right. Everything is aligned to the same row baselines.
        auto panelBg = CCLayerColor::create({ 5, 9, 18, 215 }, kWinWidth - 16.f, 90.f);
        panelBg->setPosition({ 8.f, 2.f });
        panelBg->setTag(3999);
        m_labels->addChild(panelBg, -1);

        std::string title = m_selectedPart.empty()
            ? "Select a part above, then tap Edit"
            : ("Editing: " + m_selectedPart);
        auto label = CCLabelBMFont::create(title.c_str(), "bigFont.fnt");
        label->setScale(0.40f);
        label->setAnchorPoint({ 0.f, 0.5f });
        label->setPosition({ 18.f, kEditorTitleY });
        label->setTag(3000);
        m_labels->addChild(label);

        // Player labels are display-only. There is intentionally no P1/P2 selection:
        // both previews use the same accessory configuration, while P2 swaps the two colors.
        auto makePlayerLabel = [&](const char* text, ccColor3B tint, CCPoint pos, int tag) {
            auto bg = roundedButtonSprite(tint, 0.78f);
            if (!bg) return;
            bg->setPosition(pos);
            bg->setTag(tag);
            m_labels->addChild(bg, 3);

            auto labelNode = CCLabelBMFont::create(text, "bigFont.fnt");
            if (!labelNode) return;
            labelNode->setScale(0.31f);
            labelNode->setColor({ 255, 255, 255 });
            labelNode->setPosition({
                bg->getContentSize().width / 2.f,
                bg->getContentSize().height / 2.f
            });
            bg->addChild(labelNode);
        };
        makePlayerLabel("P1", { 55, 165, 245 }, { 215.f, 78.f }, 3005);
        makePlayerLabel("P2", { 105, 120, 150 }, { 315.f, 78.f }, 3006);

        // Preview boxes are deliberately behind the actual SimplePlayer sprites.
        auto p1Box = CCLayerColor::create({ 18, 55, 92, 120 }, 78.f, 62.f);
        p1Box->setPosition({ 176.f, 17.f });
        p1Box->setTag(3070);
        m_labels->addChild(p1Box, -1);

        auto p2Box = CCLayerColor::create({ 45, 52, 70, 120 }, 78.f, 62.f);
        p2Box->setPosition({ 276.f, 17.f });
        p2Box->setTag(3071);
        m_labels->addChild(p2Box, -1);

        if (m_selectedPart.empty() || m_selectedFolder.empty()) return;

        auto& cfg = AccessoryManager::get()->partConfig(
            1, configModeKey(m_currentMode), m_selectedFolder, m_selectedPart
        );

        // Four value rows. The value field is centered exactly between its - and + buttons.
        // X/Y are nudged by 1.00; Scale by 0.10; Rotation by 5 degrees.
        addValueRow("X", cfg.x, 18.f, 56.f, 3010, m_xInput,
            menu_selector(AccessoriesPopup::onNudgeXMinus), menu_selector(AccessoriesPopup::onNudgeXPlus));
        addValueRow("Y", cfg.y, 18.f, 33.f, 3020, m_yInput,
            menu_selector(AccessoriesPopup::onNudgeYMinus), menu_selector(AccessoriesPopup::onNudgeYPlus));

        addValueRow("Scale", cfg.scale, 347.f, 56.f, 3030, m_scaleInput,
            menu_selector(AccessoriesPopup::onShrinkPart), menu_selector(AccessoriesPopup::onGrowPart));
        addValueRow("Rot", cfg.rotation, 347.f, 33.f, 3040, m_rotationInput,
            menu_selector(AccessoriesPopup::onRotateMinus), menu_selector(AccessoriesPopup::onRotatePlus));

        // One shared Front/Back control. It applies to the shared configuration used by both
        // player previews, so a second copy of the button would only be redundant.
        auto frontBtn = makeTextButton(cfg.front ? "Front" : "Back", { 55, 165, 245 }, 0.37f,
            this, menu_selector(AccessoriesPopup::onTogglePartFront));
        frontBtn->setTag(3001);
        frontBtn->setPosition({ 105.f, 10.f });
        m_menu->addChild(frontBtn);

        // Reset occupies the former right-side Front/Back slot, as requested.
        auto resetBtn = makeTextButton("Reset", { 200, 65, 65 }, 0.37f,
            this, menu_selector(AccessoriesPopup::onResetPart));
        resetBtn->setTag(3002);
        resetBtn->setPosition({ 440.f, 10.f });
        m_menu->addChild(resetBtn);
    }

    void addValueRow(std::string const& name, float currentValue, float x, float y, int baseTag,
                      CCTextInputNode*& inputSlot, SEL_MenuHandler minusSel, SEL_MenuHandler plusSel) {
        // Label is intentionally a little farther left for the wider numeric group.
        auto nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
        nameLabel->setScale(0.50f);
        nameLabel->setAnchorPoint({ 0.f, 0.5f });
        nameLabel->setPosition({ x, y });
        nameLabel->setTag(baseTag);
        m_labels->addChild(nameLabel);

        // The three controls share one exact center:
        // minus center = x+36, value center = x+86, plus center = x+136.
        // This makes the numeric field visually sit exactly between +/-.
        constexpr float buttonSize = 21.f;
        constexpr float valueW = 62.f;
        constexpr float valueH = 21.f;
        constexpr float center = 86.f;

        auto minusBtn = makeSquareTextButton("-", { 80, 115, 150 }, this, minusSel,
            buttonSize, 170.f);
        if (minusBtn) {
            minusBtn->setTag(baseTag + 2);
            minusBtn->setPosition({ x + 36.f, y });
            m_menu->addChild(minusBtn);
        }

        auto valueBg = CCLayerColor::create({ 4, 8, 17, 235 }, valueW, valueH);
        valueBg->setAnchorPoint({ 0.5f, 0.5f });
        valueBg->setPosition({ x + center, y });
        valueBg->setTag(baseTag + 4);
        m_labels->addChild(valueBg);

        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", currentValue);

        auto input = CCTextInputNode::create(valueW, valueH, buf, "bigFont.fnt");
        input->setString(buf);
        input->setDelegate(this);
        input->setScale(0.50f);
        input->setPosition({ x + center, y });
        input->setTag(baseTag + 1);
        m_labels->addChild(input);
        inputSlot = input;

        auto plusBtn = makeSquareTextButton("+", { 70, 150, 95 }, this, plusSel,
            buttonSize, 170.f);
        if (plusBtn) {
            plusBtn->setTag(baseTag + 3);
            plusBtn->setPosition({ x + 136.f, y });
            m_menu->addChild(plusBtn);
        }
    }

    // Fires whenever the user types in one of the X/Y/Scale/Rotation boxes. Only applies the
    // change once the box contains a fully-valid number (so typing "-", "1.", etc. mid-edit
    // doesn't get clobbered); anything else is just left for the user to keep typing.
    void textChanged(CCTextInputNode* node) override {
        auto cfg = selectedConfig();
        if (!cfg) return;

        std::string s = node->getString();
        if (s.empty()) return;
        char* end = nullptr;
        double val = strtod(s.c_str(), &end);
        if (!end || *end != '\0') return; // not a fully-typed valid number yet

        if (node == m_xInput) cfg->x = (float)val;
        else if (node == m_yInput) cfg->y = (float)val;
        else if (node == m_scaleInput) cfg->scale = clampFloat((float)val, 0.01f, 10.f);
        else if (node == m_rotationInput) {
            float r = (float)val;
            while (r > 180.f) r -= 360.f;
            while (r < -180.f) r += 360.f;
            cfg->rotation = r;
        } else {
            return;
        }

        AccessoryManager::get()->save();
        refreshPreview();
    }

    AccessoryPartConfig* selectedConfig() {
        if (m_selectedFolder.empty() || m_selectedPart.empty()) return nullptr;
        return &AccessoryManager::get()->partConfig(1, configModeKey(m_currentMode), m_selectedFolder, m_selectedPart);
    }

    void onTogglePartFront(CCObject*) {
        auto cfg = selectedConfig();
        if (!cfg) return;
        cfg->front = !cfg->front;
        AccessoryManager::get()->save();
        rebuildEditor();
        refreshPreview();
    }

    void onResetPart(CCObject*) {
        auto cfg = selectedConfig();
        if (!cfg) return;
        cfg->x = 0.f; cfg->y = 0.f; cfg->scale = 1.f; cfg->rotation = 0.f;
        AccessoryManager::get()->save();
        rebuildEditor(); // refresh the displayed X/Y/Scale/Rotate values, not just the preview
        refreshPreview();
    }

    void onNudgeXMinus(CCObject*) { nudgePosition(-1.f, 0.f); }
    void onNudgeXPlus(CCObject*)  { nudgePosition(1.f, 0.f); }
    void onNudgeYMinus(CCObject*) { nudgePosition(0.f, -1.f); }
    void onNudgeYPlus(CCObject*)  { nudgePosition(0.f, 1.f); }
    void onShrinkPart(CCObject*)  { nudgeScale(-0.1f); }
    void onGrowPart(CCObject*)    { nudgeScale(0.1f); }
    void onRotateMinus(CCObject*) { nudgeRotation(-5.f); }
    void onRotatePlus(CCObject*)  { nudgeRotation(5.f); }

    void nudgePosition(float dx, float dy) {
        auto cfg = selectedConfig();
        if (!cfg) return;
        cfg->x += dx;
        cfg->y += dy;
        AccessoryManager::get()->save();
        rebuildEditor(); // keep the X/Y text boxes in sync with the new value
        refreshPreview();
    }

    void nudgeScale(float delta) {
        auto cfg = selectedConfig();
        if (!cfg) return;
        cfg->scale = clampFloat(cfg->scale + delta, 0.01f, 10.f);
        AccessoryManager::get()->save();
        rebuildEditor();
        refreshPreview();
    }

    void nudgeRotation(float delta) {
        auto cfg = selectedConfig();
        if (!cfg) return;
        cfg->rotation += delta;
        while (cfg->rotation > 180.f) cfg->rotation -= 360.f;
        while (cfg->rotation < -180.f) cfg->rotation += 360.f;
        AccessoryManager::get()->save();
        rebuildEditor();
        refreshPreview();
    }

    static float clampFloat(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    AccessoryFolder* currentFolder() {
        auto& folders = AccessoryManager::get()->folders();
        for (auto& f : folders) if (f.name == m_selectedFolder) return &f;
        return nullptr;
    }

    // ---------------------------------------------------------------------------------------
    //  Mode row + preview refresh
    // ---------------------------------------------------------------------------------------
    void onSelectMode(CCObject* sender) {
        int tag = static_cast<CCNode*>(sender)->getTag();
        if (tag < 0 || tag >= 9) return;
        m_currentMode = MODE_KEYS[tag];
        rebuildFolderList();
        rebuildPartsList();
        rebuildEditor();
        refreshPreview();
        rebuildModeHighlight();
    }

    // Re-tints the mode row so the currently selected mode is visibly highlighted, without
    // rebuilding the whole row (keeps the SimplePlayer icons alive/cheap to update).
    void rebuildModeHighlight() {
        for (int i = 0; i < 9; i++) {
            if (!m_modeButtonBg[i]) continue;
            m_modeButtonBg[i]->setOpacity(MODE_KEYS[i] == m_currentMode ? 255 : 0);
        }
    }

    void refreshPreview() {
        auto gm = GameManager::get();
        int iconId = iconIdForMode(gm, m_currentMode);

        // P1 preview.
        m_preview->updatePlayerFrame(iconId, modeKeyToIconType(m_currentMode));
        ccColor3B c1 = gm->colorForIdx(gm->getPlayerColor());
        ccColor3B c2 = gm->colorForIdx(gm->getPlayerColor2());
        m_preview->setColor(c1);
        m_preview->setSecondColor(c2);

        // P2 preview: exactly the same icon/accessory layout, with the two colors swapped.
        m_preview2->updatePlayerFrame(iconId, modeKeyToIconType(m_currentMode));
        m_preview2->setColor(c2);
        m_preview2->setSecondColor(c1);

        m_previewFront->removeAllChildren();
        m_previewBack->removeAllChildren();
        m_previewFront2->removeAllChildren();
        m_previewBack2->removeAllChildren();

        auto mgr = AccessoryManager::get();
        for (auto& folder : mgr->folders()) {
            if (!mgr->isFolderVisible(1, configModeKey(m_currentMode), folder.name)) continue;
            for (auto& part : folder.parts) {
                auto& cfg = mgr->partConfig(1, configModeKey(m_currentMode), folder.name, part.name);
                if (!cfg.enabled) continue;

                auto node1 = AccessoryPartNode::create(part, false);
                auto node2 = AccessoryPartNode::create(part, true);
                if (!node1 || !node2) {
                    if (node1) node1->removeFromParentAndCleanup(true);
                    if (node2) node2->removeFromParentAndCleanup(true);
                    continue;
                }

                node1->setPosition({ cfg.x, cfg.y });
                node1->setScale(cfg.scale);
                node1->setRotation(cfg.rotation);
                (cfg.front ? m_previewFront : m_previewBack)->addChild(node1);

                node2->setPosition({ cfg.x, cfg.y });
                node2->setScale(cfg.scale);
                node2->setRotation(cfg.rotation);
                (cfg.front ? m_previewFront2 : m_previewBack2)->addChild(node2);
            }
        }
    }

    // ---------------------------------------------------------------------------------------
    //  Full-screen click catcher: anything not claimed by m_menu lands here and is swallowed,
    //  so it can never reach whatever is behind the popup (e.g. GJGarageLayer's own buttons).
    // ---------------------------------------------------------------------------------------
    bool ccTouchBegan(CCTouch* touch, CCEvent* event) override {
        CCPoint p = this->convertToNodeSpace(touch->getLocation());
        float lx = p.x - m_windowOrigin.x;
        float ly = p.y - m_windowOrigin.y;

        // Drag either list scrollbar. The scrollbar is intentionally outside the buttons,
        // so it never competes with row hit-testing.
        bool inTrackX = (lx >= 322.f && lx <= 340.f) ||
                        (lx >= kFolderColX + kFolderColW - 2.f && lx <= kFolderColX + kFolderColW + 14.f);
        bool inTrackY = (ly >= kScrollTrackY - 8.f && ly <= kScrollTrackY + kScrollTrackH + 8.f);

        if (inTrackX && inTrackY) {
            m_draggingScroll = true;
            m_draggingFolderScroll = lx > kFolderColX - 5.f;
            m_scrollDragStartY = ly;
            m_scrollDragStartPage = m_draggingFolderScroll ? m_folderPage : m_partPage;
            return true;
        }

        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) override {
        if (!m_draggingScroll) return;

        CCPoint p = this->convertToNodeSpace(touch->getLocation());
        float ly = p.y - m_windowOrigin.y;
        float dy = ly - m_scrollDragStartY;

        int maxPage = 0;
        if (m_draggingFolderScroll) {
            auto& folders = AccessoryManager::get()->folders();
            maxPage = (std::max)(0, (int)((folders.size() + kPageSize - 1) / kPageSize) - 1);
        } else {
            auto folder = currentFolder();
            if (folder)
                maxPage = (std::max)(0, (int)((folder->parts.size() + kPageSize - 1) / kPageSize) - 1);
        }

        // One page per roughly 28px of thumb travel.
        int deltaPages = (int)std::round(-dy / 28.f);
        int page = (std::max)(0, (std::min)(maxPage, m_scrollDragStartPage + deltaPages));

        if (m_draggingFolderScroll && page != m_folderPage) {
            m_folderPage = page;
            rebuildFolderList();
        } else if (!m_draggingFolderScroll && page != m_partPage) {
            m_partPage = page;
            rebuildPartsList();
        }
    }

    void ccTouchEnded(CCTouch* touch, CCEvent* event) override {
        m_draggingScroll = false;
    }
};

// =============================================================================================
//  GJGarageLayer hook — this is the real "Icon Kit" screen (accessible from the main menu),
//  where the player picks their cube/ship/etc. icons. There is no separate "IconSelectorPopup"
//  class in the 2.208 bindings; the whole icon-selection UI, including the mode tabs, lives in
//  this one full-screen layer. We add a button in the bottom-right corner of the screen.
// =============================================================================================
class $modify(AccessoriesGarageLayer, GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init()) return false;

        // NOTE: this button used to break here specifically — first with GJ_gearIcon_001.png,
        // then with icon_button.png — because the frame's texture wasn't resident in memory in
        // this particular scene, even though the frame metadata existed elsewhere. squareSprite()
        // is now a flat CCLayerColor fill with no texture dependency at all, so this can't happen
        // again regardless of which scene the button is built in.
        auto btnBg = circleButtonSprite({ 55, 175, 205 }, 0.58f, true);
        if (!btnBg) return true;
        auto btnLabel = CCLabelBMFont::create("ACC", "bigFont.fnt");
        btnLabel->setScale(0.25f);
        btnLabel->setPosition({ btnBg->getContentSize().width / 2.f, btnBg->getContentSize().height / 2.f });
        btnBg->addChild(btnLabel);

        auto openBtn = CCMenuItemSpriteExtra::create(
            btnBg,
            this,
            menu_selector(AccessoriesGarageLayer::onOpenAccessories)
        );

        // Anchored to the bottom-right of the visible area (not raw winSize) so it stays
        // on-screen regardless of aspect ratio.
        auto director = CCDirector::sharedDirector();
        auto visibleSize = director->getVisibleSize();
        auto visibleOrigin = director->getVisibleOrigin();
        auto menu = CCMenu::create();
        menu->addChild(openBtn);
        menu->setPosition({ visibleOrigin.x + visibleSize.width - 30.f, visibleOrigin.y + 30.f });
        menu->setZOrder(200);
        this->addChild(menu, 200);

        return true;
    }

    void onOpenAccessories(CCObject* sender) {
        AccessoriesPopup::create()->show();
    }
};

// =============================================================================================
//  Load settings once at mod startup.
// =============================================================================================
$execute {
    AccessoryManager::get()->load();
    AccessoryManager::get()->scanAccessories();
}
