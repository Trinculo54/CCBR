// CCBR (Can't Crash, Busy Racing) for Forza Horizon 2 (Azimuth): your car is a ghost for the length of a
// speed zone, so you drive straight through traffic and Drivatars, and every other car is see-through
// meanwhile. Offline only: online, the ghost would only be one on your own screen.
//
// How it works (game addresses below, for the exe of 24 Nov 2015):
//  - The speed camera events: ARSpeedCamera sends them through its entity's event queue, with one of two
//    send functions (both hooked). Each event is a global symbol, checked against the code that sends it.
//  - The player's car is the ForzaCar of kind 0 (PLAYER_LOCAL), seen by two of ForzaCar's getters.
//  - The ghost is a collidable request on the player's car with a preset of CCBR's own, "ccbr" (collision
//    off), which it adds to the game's collidablepresets.xml (read from media\GameTunableSettings.zip) through
//    the platform's mediaoverride. Without it, the game's own "intersection" preset does the same.
//  - The other cars: the game's per-car alpha (game+0x28C590, which ForzaCar's per-frame update hands to the
//    car's presentation) is capped for Drivatars and traffic while the ghost is on. That works whichever
//    request is last in a car's list: the 29 AI cars are a pool that the game puts back on the road further
//    ahead, and doing so puts the AI's own request (ai_ow) back at the end of the list. It also zeroes that
//    request's alpha until the AI's next fade update, which hides the car for a frame or so (the request
//    set's update calls game+0xF649F0); for a visible car, that hide and its alpha are held back for up to
//    0.15 s (HoldBlinks), so cars don't blink out.
// Settings: the MODS page and fh2-mods\CCBR.ini. Everything is logged to fh2-mods\platform.log as
// "[CCBR] ..."; Debug=1 adds every event, and a census of the other cars at the start of each zone, 3 s in
// and at its end.
#include "fh2mod.h"

#include <windows.h>

#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#ifndef CCBR_VERSION
#define CCBR_VERSION "0.0.0"
#endif

namespace {

#include "unzip.inc"

// ---- game addresses (forza_durango_release_final.exe, 24 Nov 2015), RVAs from its base -------------

constexpr uintptr_t RVA_CAR_KIND        = 0xF62280;  // 0x140F62280 ForzaCar kind: mov eax,[rcx+4D08h]; ret
constexpr uintptr_t RVA_CAR_REQUESTS    = 0xF60510;  // 0x140F60510 ForzaCar collidable requests: lea rax,[rcx+4C58h]
constexpr uintptr_t RVA_SEND_EVENT      = 0x190BF90; // 0x14190BF90 send an event to an entity (entity, symbol)
constexpr uintptr_t RVA_SEND_EVENT_DATA = 0x190BFD0; // 0x14190BFD0 the same with data (entity, symbol, data)
constexpr uintptr_t RVA_MAKE_REQUEST    = 0xF5B420;  // 0x140F5B420 requests: a handle for a preset, by name
constexpr uintptr_t RVA_ADD_REQUEST     = 0xF73080;  // 0x140F73080 requests: register a handle (takes it over)
constexpr uintptr_t RVA_REMOVE_REQUEST  = 0xF5B890;  // 0x140F5B890 requests: remove one, by preset name
constexpr uintptr_t RVA_FORZACAR_VT     = 0x2C5CB80; // 0x142C5CB80 ForzaCar's vtable (RTTI .?AVForzaCar@@)
constexpr uintptr_t RVA_CAR_DTOR        = 0x289320;  // 0x140289320 ~ForzaCar, from its deleting destructor
                                                     // (vtable slot 0, 0x14028B380), before the car is freed
constexpr uintptr_t RVA_CAR_APPLY       = 0x28D930;  // 0x14028D930 ForzaCar vtable slot 0x130: works out the
                                                     // car's alpha and hands it to its presentation
constexpr uint32_t VT_CAR_APPLY         = 0x130 / 8;
// The car's alpha, from the last request in its list (or a distance fade): float(self, ForzaCar**). Called
// by ForzaCar's per-frame update (slot 0x80, 0x1402A9400), by slot 0x130 and by the ghost display
// (0x1402A5E30); each hands the result to car+0x4D38 slot 0x130 (SetAlphaScalar).
constexpr uintptr_t RVA_CAR_ALPHA       = 0x28C590;  // 0x14028C590
// Hides a car: clears car+0x50F1 and calls car+0x4D38 slot 0x230 (HideCar). Only called by the request set's
// update (0x140F7B770, at 0x140F7BBB2) when the newest request's alpha changes to 0 or below; the opposite,
// 0x140F79330, shows it (slot 0x228). The update then keeps that alpha at set+0x58.
constexpr uintptr_t RVA_CAR_HIDE        = 0xF649F0;  // 0x140F649F0
constexpr uint32_t OFF_SET_APPLIED      = 0x58;
// What lives at car+0x4D38 (for the census): the ICarPresentation interface (+0x10) of a
// TRefCountedObjectThreadSafe<CCarPresentationDeferred>, made by CPresentationDeferred slot 0xE0
// (0x14132D280) in Car::Load (0x140F65AD0). Its +8 is the real presentation, normally a
// TRefCountedObjectThreadSafeWithDeletionInfo<Presentation11::CarPresentation>; that keeps the alpha at
// +0x314 (read by the render side every frame) and its shown flag at +0x318. The Default and Null
// placeholders ignore alpha.
constexpr uintptr_t RVA_DEFERRED_WRAP_VT = 0x2EBEF68; // 0x142EBEF68
constexpr uintptr_t RVA_P11_WRAP_VT      = 0x2EC2B70; // 0x142EC2B70
constexpr uintptr_t RVA_DEFAULT_WRAP_VT  = 0x2EB8C50; // 0x142EB8C50
constexpr uintptr_t RVA_NULL_WRAP_VT     = 0x2EBB478; // 0x142EBB478
constexpr uint32_t OFF_CAR_REQUESTS     = 0x4C58;
constexpr uint32_t OFF_CAR_KIND         = 0x4D08;    // 0 PLAYER_LOCAL, 1 _AI, 2 _TRAFFIC, 3-5 the same remote
constexpr uint32_t OFF_CAR_PRES         = 0x4D38;
constexpr uint32_t OFF_REAL_ALPHA       = 0x314;
constexpr uint32_t OFF_REAL_SHOWN       = 0x318;

// The speed camera events: global symbols (made by static initialisers) and the instruction that sends each,
// "lea rdx,[rip+symbol]" just before the call to one of the two send functions. Offline, the game sends
// nothing before a speed trap (the AR markers need Rivals data), so only speed zones make a ghost.
enum Event { ArEntered, ArExited, TrapPassed, ZoneStarted, ZoneCompleted, ZoneAborted, kEvents };
struct EventDef {
    const char* name;
    uintptr_t symbol; // RVA of the global
    uintptr_t site;   // RVA of the lea that loads it
};
constexpr EventDef kEventDefs[kEvents] = {
    {"ARUI_Trigger_Entered", 0x34BFCA0, 0x6B84F2}, // 0x1434BFCA0, sent at 0x1406B84F2 (ARSpeedCamera)
    {"ARUI_Trigger_Exited", 0x34BFB50, 0x6B992A},  // 0x1434BFB50, sent at 0x1406B992A
    {"SpeedTrapActivated", 0x34BFDD8, 0x6B8A87},   // 0x1434BFDD8, sent with data at 0x1406B8A87
    {"SpeedZoneActivated", 0x34BFF20, 0x6B965E},   // 0x1434BFF20, sent with data at 0x1406B965E
    {"SpeedZoneCompleted", 0x34BFB10, 0x6B8EA8},   // 0x1434BFB10, sent at 0x1406B8EA8
    {"SpeedZoneAborted", 0x34BFDB8, 0x6A2AF7},     // 0x1434BFDB8, sent at 0x1406A2AF7
};

constexpr char kPreset[]        = "ccbr";         // added to collidablepresets.xml (presets_with_ours)
constexpr char kFallback[]      = "intersection"; // the game's own, collision off, no alpha
constexpr char kPresetsZip[]    = "GameTunableSettings.zip";
constexpr char kPresetsFile[]   = "collidablepresets.xml";
constexpr ULONGLONG kMaxGhostMs  = 90000;         // ends a ghost no event ended
constexpr ULONGLONG kMidCensusMs = 3000;          // a census this long into a ghost (Debug=1)
constexpr ULONGLONG kBlinkMs     = 150;           // a car hidden for up to this long keeps its alpha (HoldBlinks)
constexpr ULONGLONG kFlushMs     = 50;            // held hides are looked at this often

// The game's collidablepresets.xml with CCBR's preset added before its closing tag, or "" if that isn't
// there.
std::string presets_with_ours(const std::string& xml) {
    const std::size_t end = xml.rfind("</carcollidablepresets>");
    if (end == std::string::npos) return {};
    if (xml.find("<preset name=\"ccbr\"") != std::string::npos) return xml;
    const char* nl = xml.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    return xml.substr(0, end) + "    <!-- CCBR: the player's car during speed zones -->" + nl + nl +
           "    <preset name=\"ccbr\" collision=\"off\" />" + nl + nl + xml.substr(end);
}

// ---- types the game uses ---------------------------------------------------------------------------

// An MSVC std::string in its short form (up to 15 characters), which the game only reads.
struct GameString {
    char buf[16];
    std::size_t size, capacity;
};
GameString game_string(const char* s) {
    GameString g{};
    g.size = std::strlen(s);
    std::memcpy(g.buf, s, g.size);
    g.capacity = 15;
    return g;
}

struct SharedPtr { // std::shared_ptr<RequestHandle>
    void* ptr;
    void* ctrl;
};

using KindFn     = int (*)(void* car);
using RequestsFn = void* (*)(void* car);
using SendFn     = void* (*)(void* entity, const void* symbol);
using SendDataFn = void* (*)(void* entity, const void* symbol, void* data);
using MakeFn     = SharedPtr* (*)(void* requests, SharedPtr* out, const GameString* preset);
using AddFn      = uint8_t (*)(void* requests, SharedPtr* handle); // handle by value: the game destroys it
using RemoveFn   = uint8_t (*)(void* requests, const GameString* preset);
using DtorFn     = void (*)(void* car);
using AlphaFn    = float (*)(void* self, void** car);
using HideFn     = void (*)(void* car);

// ---- state ------------------------------------------------------------------------------------------

const FH2ModApi* g_api;
FH2Mod* g_mod;
char* g_base;
const void* g_forzacar_vt;
// CCBR.ini [CCBR], and the MODS page. Ghost and SeeThrough count from the next zone; OthersAlpha straight away.
std::atomic<bool> g_ghost_on{true};    // Ghost
std::atomic<bool> g_see_through{true}; // SeeThrough
std::atomic<float> g_alpha{0.5f};      // OthersAlpha
std::atomic<bool> g_hold_blinks{true}; // HoldBlinks
bool g_check_updates = true;           // CheckUpdates
bool g_debug         = false;          // Debug
bool g_preset_file   = false;

KindFn o_kind;
RequestsFn o_requests;
SendFn o_send;
SendDataFn o_send_data;
DtorFn o_dtor;
AlphaFn o_car_alpha;
HideFn o_hide;
MakeFn g_make;
AddFn g_add;
RemoveFn g_remove;

const void* g_symbols[kEvents]; // nullptr: that event couldn't be checked, so it's ignored
const void* g_wrap_vt[4];       // deferred, P11, Default, Null (for the census)

// The player's car, as last seen by the two car hooks (any thread).
std::atomic<void*> g_player{nullptr};
std::atomic<ULONGLONG> g_player_seen{0};
std::atomic<void*> g_player_logged{nullptr};
std::atomic<ULONGLONG> g_player_logged_at{0};

// The ghost, touched only under g_lock by the event hooks (the game's entity update). g_ghost_car is cleared
// by ForzaCar's destructor too.
std::atomic_flag g_lock = ATOMIC_FLAG_INIT;
std::atomic<bool> g_ghosted{false};
std::atomic<void*> g_ghost_car{nullptr};
const char* g_ghost_preset = nullptr;
ULONGLONG g_ghost_since = 0;
bool g_in_zone = false;
bool g_mid_census = false;
std::atomic<DWORD> g_game_thread{0}; // the thread the speed camera events come on

// See-through: while on, the alpha hook caps Drivatars' and traffic's alpha at g_alpha.
std::atomic<bool> g_others_on{false};
std::atomic<long> g_capped{0};       // alpha calls capped this ghost
std::atomic<long> g_zero{0};         // ... where the game said 0 (hiding the car)
std::atomic<long> g_below{0};        // ... already at or below g_alpha (fading in, or another ghost)
std::atomic<long> g_blinks{0};       // a visible car at 0 for less than kBlinkMs
std::atomic<long> g_drops{0};        // ... for longer, so hidden
std::atomic<long> g_hides_held{0};   // the game's hide (0x140F649F0) held back during a blink
std::atomic<long> g_hides_late{0};   // ... applied after all, when the car stayed at 0 or the ghost ended
std::atomic<long> g_hides_undone{0}; // ... dropped, the game having shown the car again meanwhile
std::atomic<long> g_hides_shown{0};  // ... cleared as its alpha came back before the hide was applied
std::atomic<long> g_hides_gone{0};   // ... gone with their car (~ForzaCar)
std::atomic<long> g_hides_passed{0}; // ... let through (the car wasn't visible, or HoldBlinks=0)
std::atomic<ULONGLONG> g_last_flush{0};

// Each other car's alpha while see-through: what CCBR last handed over, and for HoldBlinks, its last
// visible alpha, since when the game has said 0, and whether its hide is held back. A car's newest request
// dropping to 0 (the AI zeroes ai_ow's alpha when it reactivates a pool car, until its next fade update)
// hides it two ways: the alpha handed over every frame (0x14028C590), and the request set's update
// (0x140F7B770), which on a change to 0 calls 0x140F649F0 (HideCar on the presentation) and on a change back
// 0x140F79330 (ShowCar). Cleared when a ghost starts; ~ForzaCar resets a car's entry. Only touched under
// g_blink_lock, which the game's threads only try for.
struct Blink {
    void* car;
    float sent;        // the alpha last handed over
    float shown;       // the last alpha above 0 handed over, or 0 once the car counts as hidden
    ULONGLONG zero_at; // when the game started saying 0 for a visible car (0: it isn't)
    bool hide_held;
};
constexpr int kBlinkSlots = 256; // a power of two
Blink g_blink[kBlinkSlots];
std::atomic_flag g_blink_lock = ATOMIC_FLAG_INIT;

// The other cars (Drivatars, kind 1, and traffic, kind 2), seen by the two car hooks (one call in 16 per
// thread) and dropped by ForzaCar's destructor before the game frees them: a car in here is alive. Only
// touched under g_cars_lock, and the game is never called with it held. Used to hand every car its alpha
// straight away when the ghost starts and ends, and for the census.
constexpr int kCarSlots = 2048; // a power of two; there are 29 AI cars (18 traffic, 11 festival)
struct CarSlot {
    void* car;       // nullptr: empty; kTomb: was a car
    ULONGLONG seen;
};
void* const kTomb = reinterpret_cast<void*>(1);
CarSlot g_cars[kCarSlots];
int g_tombs = 0;
std::atomic_flag g_cars_lock = ATOMIC_FLAG_INIT;
thread_local bool t_cars_locked = false;
std::atomic<int> g_dtor_elsewhere{0}; // cars destroyed on another thread than the game's

void vlog(const char* fmt, va_list ap) {
    char line[512];
    std::vsnprintf(line, sizeof line, fmt, ap);
    g_api->log(g_mod, line);
}
void logf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vlog(fmt, ap);
    va_end(ap);
}
// Only with Debug=1.
void dlogf(const char* fmt, ...) {
    if (!g_debug) return;
    va_list ap;
    va_start(ap, fmt);
    vlog(fmt, ap);
    va_end(ap);
}

// ---- calls into the game, guarded (no objects with destructors in these) ----------------------------

// clang's __try only catches a fault inside a call it makes: around an inlined memcpy (plain loads) it drops
// the guard. So the copy goes through a pointer the compiler can't see through.
void* (*volatile g_copy)(void*, const void*, std::size_t) = std::memcpy;
__attribute__((noinline)) bool read_ok(const void* p, void* out, std::size_t n) {
    __try {
        g_copy(out, p, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool call_make(void* requests, SharedPtr* out, const GameString* name, DWORD* code) {
    __try {
        g_make(requests, out, name);
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool call_add(void* requests, SharedPtr* handle, uint8_t* ok, DWORD* code) {
    __try {
        *ok = g_add(requests, handle);
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool call_remove(void* requests, const GameString* name, uint8_t* ok, DWORD* code) {
    __try {
        *ok = g_remove(requests, name);
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Drops a handle the game made, as std::shared_ptr's destructor does: the uses at +8 and weak references at
// +0xC of its control block, then its _Destroy (vtable slot 0) and _Delete_this (slot 1). "Not found" still
// comes with a control block (for a null pointer).
bool release_handle(SharedPtr* h, DWORD* code) {
    __try {
        auto* ctrl = static_cast<char*>(h->ctrl);
        h->ptr = h->ctrl = nullptr;
        if (ctrl && InterlockedDecrement(reinterpret_cast<volatile LONG*>(ctrl + 8)) == 0) {
            void* const* vt = *reinterpret_cast<void* const* const*>(ctrl);
            reinterpret_cast<void (*)(void*)>(vt[0])(ctrl);
            if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(ctrl + 12)) == 0)
                reinterpret_cast<void (*)(void*)>(vt[1])(ctrl);
        }
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The car's own slot 0x130, as the game calls it after every request change: it works out the alpha (through
// the hooked alpha function) and hands it over. Through the vtable, so a hook on the slot still runs.
bool call_apply(void* car, DWORD* code) {
    void* const* vt = nullptr;
    void* fn        = nullptr;
    if (!read_ok(car, &vt, sizeof vt) || !vt || !read_ok(vt + VT_CAR_APPLY, &fn, sizeof fn) || !fn) {
        *code = EXCEPTION_ACCESS_VIOLATION;
        return false;
    }
    __try {
        reinterpret_cast<void (*)(void*)>(fn)(car);
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The game's own hide for a car (0x140F649F0), past CCBR's hook.
bool call_hide(void* car, DWORD* code) {
    __try {
        o_hide(car);
        return true;
    } __except (*code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Still the player's car: ForzaCar's vtable and kind 0.
bool still_player_car(void* car) {
    const void* vt = nullptr;
    int kind       = -1;
    return read_ok(car, &vt, sizeof vt) && vt == g_forzacar_vt &&
           read_ok(static_cast<char*>(car) + OFF_CAR_KIND, &kind, sizeof kind) && kind == 0;
}

// ---- the other cars ---------------------------------------------------------------------------------

void cars_lock() {
    while (g_cars_lock.test_and_set(std::memory_order_acquire)) {}
    t_cars_locked = true;
}
bool cars_try_lock() {
    if (g_cars_lock.test_and_set(std::memory_order_acquire)) return false;
    t_cars_locked = true;
    return true;
}
void cars_unlock() {
    t_cars_locked = false;
    g_cars_lock.clear(std::memory_order_release);
}

unsigned slot_of(const void* car) {
    return static_cast<unsigned>((reinterpret_cast<uintptr_t>(car) >> 4) * 0x9E3779B97F4A7C15ull >> 40) &
           (kCarSlots - 1);
}

// The car's slot, or nullptr. Under the lock.
CarSlot* find_car(const void* car) {
    for (unsigned i = slot_of(car), n = 0; n < kCarSlots; i = (i + 1) & (kCarSlots - 1), ++n) {
        if (!g_cars[i].car) return nullptr;
        if (g_cars[i].car == car) return &g_cars[i];
    }
    return nullptr;
}

// Puts back every car without the removed ones in between. Under the lock.
void rebuild_cars() {
    static CarSlot keep[kCarSlots];
    int n = 0;
    for (auto& c : g_cars)
        if (c.car && c.car != kTomb) keep[n++] = c;
    std::memset(g_cars, 0, sizeof g_cars);
    g_tombs = 0;
    for (int k = 0; k < n; ++k)
        for (unsigned i = slot_of(keep[k].car);; i = (i + 1) & (kCarSlots - 1))
            if (!g_cars[i].car) {
                g_cars[i] = keep[k];
                break;
            }
}

void note_car(void* car) {
    if (t_cars_locked) return;
    thread_local unsigned calls = 0;
    if ((++calls & 15) != 0) return;
    if (*static_cast<const void* const*>(car) != g_forzacar_vt || !cars_try_lock()) return;
    const ULONGLONG now = GetTickCount64();
    CarSlot* tomb       = nullptr;
    for (unsigned i = slot_of(car), n = 0; n < kCarSlots; i = (i + 1) & (kCarSlots - 1), ++n) {
        CarSlot& c = g_cars[i];
        if (c.car == car) {
            c.seen = now;
            cars_unlock();
            return;
        }
        if (c.car == kTomb && !tomb) tomb = &c;
        if (!c.car) {
            CarSlot& to = tomb ? *tomb : c;
            if (tomb) --g_tombs;
            to = {car, now};
            break;
        }
    }
    cars_unlock();
}

void d_dtor(void* car) {
    if (car) {
        const DWORD game = g_game_thread.load(std::memory_order_relaxed);
        if (game && GetCurrentThreadId() != game) g_dtor_elsewhere.fetch_add(1, std::memory_order_relaxed);
        const bool mine = t_cars_locked;
        if (!mine) cars_lock();
        if (CarSlot* c = find_car(car)) {
            c->car = kTomb;
            ++g_tombs;
        }
        if (!mine) cars_unlock();
        while (g_blink_lock.test_and_set(std::memory_order_acquire)) {}
        for (auto& b : g_blink) // a new car at this address starts afresh, and a held hide goes with this one
            if (b.car == car) {
                if (b.hide_held) g_hides_gone.fetch_add(1, std::memory_order_relaxed);
                b = {car, 0.0f, 0.0f, 0, false};
            }
        g_blink_lock.clear(std::memory_order_release);
        void* expected = car;
        g_player.compare_exchange_strong(expected, nullptr);
        expected = car;
        g_ghost_car.compare_exchange_strong(expected, nullptr);
    }
    o_dtor(car);
}

bool still_tracked(void* car) {
    cars_lock();
    const bool found = find_car(car) != nullptr;
    cars_unlock();
    return found;
}

// The other cars seen in the last 2 seconds (alive: the destructor hook drops them).
int recent_cars(void** out, int max) {
    const ULONGLONG now = GetTickCount64();
    int n               = 0;
    cars_lock();
    if (g_tombs > kCarSlots / 4) rebuild_cars();
    for (auto& c : g_cars)
        if (c.car && c.car != kTomb && now - c.seen < 2000 && n < max) out[n++] = c.car;
    cars_unlock();
    return n;
}

// Every other car hands its presentation its alpha now (the game does so every frame anyway; this is for
// the moment the ghost starts and ends).
int hand_over_all() {
    static void* cars[kCarSlots];
    const int n = recent_cars(cars, kCarSlots);
    int done    = 0;
    for (int k = 0; k < n; ++k) {
        if (!still_tracked(cars[k])) continue;
        DWORD code = 0;
        if (call_apply(cars[k], &code)) ++done;
        else logf("  see-through: handing %p its alpha crashed (exception 0x%08lX)", cars[k], code);
    }
    return done;
}

// ---- the alpha hook ---------------------------------------------------------------------------------

// A car's entry, or with `add`, a new one; nullptr if its 16 slots are taken. Under g_blink_lock.
Blink* blink_of(void* car, bool add) {
    for (unsigned i = slot_of(car) & (kBlinkSlots - 1), n = 0; n < 16; i = (i + 1) & (kBlinkSlots - 1), ++n) {
        if (g_blink[i].car == car) return &g_blink[i];
        if (!g_blink[i].car) {
            if (!add) return nullptr;
            g_blink[i] = {car, 0.0f, 0.0f, 0, false};
            return &g_blink[i];
        }
    }
    return nullptr;
}

// A held hide, applied now: unless the game has shown the car again since (the request set's last applied
// alpha, set+0x58, is above 0 again), when it is dropped.
bool apply_hide(char* car) {
    float applied = 1.0f;
    if (!read_ok(car + OFF_CAR_REQUESTS + OFF_SET_APPLIED, &applied, sizeof applied) || applied > 0.0f) {
        g_hides_undone.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    DWORD code = 0;
    if (!call_hide(car, &code)) {
        logf("  see-through: hiding %p after all crashed (exception 0x%08lX)", car, code);
        return false;
    }
    g_hides_late.fetch_add(1, std::memory_order_relaxed);
    return true;
}

// Applies the held hides of cars the game has said 0 for longer than kBlinkMs (or, with `all`, every held
// hide: the ghost is ending). A held hide's car is alive: ~ForzaCar drops it first.
void flush_hides(ULONGLONG now, bool all) {
    char* due[64];
    bool expired[64]; // still visible as far as the alpha hook knows: the alpha function hasn't run since
    int n = 0;
    if (all) while (g_blink_lock.test_and_set(std::memory_order_acquire)) {}
    else if (g_blink_lock.test_and_set(std::memory_order_acquire)) return;
    for (auto& b : g_blink) {
        if (!b.car || !b.hide_held || (!all && b.shown > 0.0f && now - b.zero_at <= kBlinkMs)) continue;
        if (n == 64) break; // the rest next time
        expired[n] = !all && b.shown > 0.0f;
        if (expired[n]) {
            b.shown   = 0.0f;
            b.zero_at = 0;
        }
        b.hide_held = false;
        due[n++]    = static_cast<char*>(b.car);
    }
    g_blink_lock.clear(std::memory_order_release);
    for (int k = 0; k < n; ++k)
        if (apply_hide(due[k]) && expired[k]) g_drops.fetch_add(1, std::memory_order_relaxed);
}

// What CCBR hands over for a see-through car (its capped alpha): noted for the census, and with HoldBlinks,
// a visible car the game says 0 for keeps its last alpha for up to kBlinkMs.
float track(char* car, float out) {
    if (g_blink_lock.test_and_set(std::memory_order_acquire)) return out;
    const ULONGLONG now = GetTickCount64();
    if (Blink* b = blink_of(car, true)) {
        if (out > 0.0f) {
            if (b->zero_at) g_blinks.fetch_add(1, std::memory_order_relaxed);
            if (b->hide_held) g_hides_shown.fetch_add(1, std::memory_order_relaxed);
            b->shown     = out;
            b->zero_at   = 0;
            b->hide_held = false; // the game shows it again itself (0x140F79330)
        } else if (b->shown > 0.0f) {
            if (!b->zero_at) b->zero_at = now;
            if (now - b->zero_at <= kBlinkMs) {
                if (g_hold_blinks.load(std::memory_order_relaxed)) out = b->shown;
            } else {
                g_drops.fetch_add(1, std::memory_order_relaxed);
                b->shown   = 0.0f; // hidden: a held hide is applied below
                b->zero_at = 0;
            }
        }
        b->sent = out;
    }
    g_blink_lock.clear(std::memory_order_release);
    ULONGLONG last = g_last_flush.load(std::memory_order_relaxed);
    if (now - last >= kFlushMs && g_last_flush.compare_exchange_strong(last, now)) flush_hides(now, false);
    return out;
}

// The game's alpha for a car, every frame. While the ghost is on, a Drivatar's or traffic car's alpha is
// capped at OthersAlpha; anything at or below it is left (0 is the game hiding the car), apart from a brief
// 0 with HoldBlinks.
float d_car_alpha(void* self, void** pcar) {
    const float alpha = o_car_alpha(self, pcar);
    if (!g_others_on.load(std::memory_order_relaxed) || !pcar) return alpha;
    char* car = static_cast<char*>(*pcar);
    if (!car || *reinterpret_cast<const void* const*>(car) != g_forzacar_vt) return alpha;
    const int kind = *reinterpret_cast<const int*>(car + OFF_CAR_KIND);
    if (kind != 1 && kind != 2) return alpha;
    const float cap = g_alpha.load(std::memory_order_relaxed);
    float out       = alpha;
    if (alpha <= 0.0f) {
        g_zero.fetch_add(1, std::memory_order_relaxed);
    } else if (alpha <= cap) {
        g_below.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_capped.fetch_add(1, std::memory_order_relaxed);
        out = cap;
    }
    return track(car, out);
}

// The game hiding a car because its newest request's alpha changed to 0 (the request set's update, its only
// caller). With HoldBlinks, while see-through, a Drivatar or traffic car that was visible isn't hidden yet:
// if its alpha comes back within kBlinkMs the game shows it again (a no-op); otherwise the hide is applied
// then (flush_hides).
void d_hide(void* car) {
    if (g_others_on.load(std::memory_order_relaxed) && car &&
        *static_cast<const void* const*>(car) == g_forzacar_vt) {
        const int kind = *reinterpret_cast<const int*>(static_cast<char*>(car) + OFF_CAR_KIND);
        if (kind == 1 || kind == 2) {
            bool held = false;
            if (g_hold_blinks.load(std::memory_order_relaxed) &&
                !g_blink_lock.test_and_set(std::memory_order_acquire)) {
                const ULONGLONG now = GetTickCount64();
                Blink* b            = blink_of(car, false);
                if (b && b->shown > 0.0f && (!b->zero_at || now - b->zero_at <= kBlinkMs)) {
                    if (!b->zero_at) b->zero_at = now;
                    b->hide_held = held = true;
                }
                g_blink_lock.clear(std::memory_order_release);
            }
            (held ? g_hides_held : g_hides_passed).fetch_add(1, std::memory_order_relaxed);
            if (held) return;
        }
    }
    o_hide(car);
}

// ---- the census (Debug=1): what the other cars' presentations hold -------------------------------------

// The type name MSVC's RTTI gives a vtable: vtable[-1] is the CompleteObjectLocator, whose +0xC is the
// TypeDescriptor's RVA; the name starts at TypeDescriptor+0x10. "?" if any of it can't be read.
void rtti_name(const void* vt, char* out, std::size_t n) {
    std::snprintf(out, n, "?");
    const char* col = nullptr;
    uint32_t td     = 0;
    if (!vt || !read_ok(static_cast<const char*>(vt) - 8, &col, sizeof col) || !col ||
        !read_ok(col + 0xC, &td, sizeof td) || !td)
        return;
    char name[128] = {};
    if (!read_ok(g_base + td + 0x10, name, sizeof name - 1)) return;
    name[sizeof name - 1] = 0;
    std::snprintf(out, n, "%s", name);
}

// Which of the wrapper vtables: 0 deferred, 1 P11, 2 Default, 3 Null, -1 another.
int wrap_kind(const void* vt) {
    for (int i = 0; i < 4; ++i)
        if (vt && vt == g_wrap_vt[i]) return i;
    return -1;
}

// The preset name of the last request in a car's list: the set at car+0x4C58 has a std::list at +8 (its
// sentinel; the last node is sentinel->prev at +8), whose node holds the request at +0x10; the request's
// +0x10 is its preset entry, whose name is a std::string at +0x20. "?" if it can't be read.
void last_request(char* car, char* out, std::size_t n) {
    std::snprintf(out, n, "?");
    const char* sentinel = nullptr;
    const char* last     = nullptr;
    const char* request  = nullptr;
    const char* preset   = nullptr;
    struct {
        union {
            char buf[16];
            const char* ptr;
        };
        std::size_t size, capacity;
    } name{};
    if (!read_ok(car + OFF_CAR_REQUESTS + 8, &sentinel, sizeof sentinel) || !sentinel ||
        !read_ok(sentinel + 8, &last, sizeof last) || !last)
        return;
    if (last == sentinel) {
        std::snprintf(out, n, "(none)");
        return;
    }
    if (!read_ok(last + 0x10, &request, sizeof request) || !request ||
        !read_ok(request + 0x10, &preset, sizeof preset) || !preset || !read_ok(preset + 0x20, &name, sizeof name) ||
        name.size == 0 || name.size > 40)
        return;
    char text[48] = {};
    if (name.capacity < 16) std::memcpy(text, name.buf, name.size);
    else if (!read_ok(name.ptr, text, name.size)) return;
    for (std::size_t i = 0; i < name.size; ++i)
        if (text[i] < 0x20 || text[i] > 0x7E) return;
    std::snprintf(out, n, "%s", text);
}

// What CCBR last handed a car over (from a copy of the blink table), or -1 if nothing yet.
float sent_to(const Blink* table, const void* car) {
    for (unsigned i = slot_of(car) & (kBlinkSlots - 1), n = 0; n < 16; i = (i + 1) & (kBlinkSlots - 1), ++n) {
        if (table[i].car == car) return table[i].sent;
        if (!table[i].car) break;
    }
    return -1.0f;
}

// Logs what the other cars' presentations hold: the object at car+0x4D38, the real presentation behind it,
// the alpha that one keeps (what the render side draws with) and whether it's shown; what CCBR handed each
// car; and which request is last in each car's list. The render side sets a see-through car's alpha to 1.0
// for a moment every frame while it draws the car's depth (0x141345611, put back at 0x141345677), so a car
// read as solid is read again a millisecond later before it counts as solid.
void census(const char* when) {
    if (!g_debug) return;
    static void* cars[kCarSlots];
    static Blink table[kBlinkSlots];
    const int n = recent_cars(cars, kCarSlots);
    while (g_blink_lock.test_and_set(std::memory_order_acquire)) {}
    std::memcpy(table, g_blink, sizeof table);
    g_blink_lock.clear(std::memory_order_release);
    const float cap = g_alpha.load();
    const float low = cap + 0.001f;
    int iface[5] = {}, real[5] = {}, none = 0, alpha_zero = 0, alpha_low = 0, alpha_mid = 0, alpha_solid = 0,
        shown = 0, solid_handed_low = 0, second_look = 0, handed_zero = 0, handed_low = 0, handed_more = 0,
        handed_none = 0;
    struct Suspect {
        char* inner;
        float sent;
    } suspects[64];
    int ns = 0;
    auto bucket = [&](float a) {
        if (a <= 0.0f) ++alpha_zero;
        else if (a <= low) ++alpha_low;
        else if (a < 0.999f) ++alpha_mid;
        else ++alpha_solid;
    };
    const void* other_vt = nullptr;
    struct Name {
        char text[48];
        int count;
    } last[8];
    int names = 0, more_names = 0;
    for (int k = 0; k < n; ++k) {
        char* car        = static_cast<char*>(cars[k]);
        char* pres       = nullptr;
        const void* vt   = nullptr;
        char* inner      = nullptr;
        const void* rvt  = nullptr;
        const float sent = sent_to(table, car);
        if (sent < 0.0f) ++handed_none;
        else if (sent <= 0.0f) ++handed_zero;
        else if (sent <= low) ++handed_low;
        else ++handed_more;
        if (!read_ok(car + OFF_CAR_PRES, &pres, sizeof pres) || !pres) {
            ++none;
        } else if (read_ok(pres, &vt, sizeof vt)) {
            const int w = wrap_kind(vt);
            ++iface[w < 0 ? 4 : w];
            if (w < 0 && !other_vt) other_vt = vt;
            if (w == 0 && read_ok(pres + 8, &inner, sizeof inner) && inner && read_ok(inner, &rvt, sizeof rvt)) {
                const int r = wrap_kind(rvt);
                ++real[r < 0 ? 4 : r];
                float a         = -1.0f;
                uint8_t visible = 0;
                if (r == 1 && read_ok(inner + OFF_REAL_ALPHA, &a, sizeof a)) {
                    if (a >= 0.999f && ns < 64) suspects[ns++] = {inner, sent};
                    else bucket(a);
                    if (read_ok(inner + OFF_REAL_SHOWN, &visible, sizeof visible) && visible) ++shown;
                }
            }
        }
        char lr[48];
        last_request(car, lr, sizeof lr);
        int j = 0;
        while (j < names && std::strcmp(last[j].text, lr) != 0) ++j;
        if (j == names) {
            if (names < 8) {
                std::snprintf(last[names].text, sizeof last[names].text, "%s", lr);
                last[names++].count = 0;
            } else {
                ++more_names;
                continue;
            }
        }
        ++last[j].count;
    }
    if (ns) {
        LARGE_INTEGER freq, start, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        do {
            YieldProcessor();
            QueryPerformanceCounter(&now);
        } while ((now.QuadPart - start.QuadPart) * 1000 < freq.QuadPart);
        for (int s = 0; s < ns; ++s) {
            float a = -1.0f;
            if (!read_ok(suspects[s].inner + OFF_REAL_ALPHA, &a, sizeof a)) continue;
            if (a < 0.999f) ++second_look;
            else if (suspects[s].sent > 0.0f && suspects[s].sent <= low) ++solid_handed_low;
            bucket(a);
        }
    }
    char lr_text[400] = {};
    for (int j = 0; j < names; ++j) {
        char item[64];
        std::snprintf(item, sizeof item, "%s%s x%d", j ? ", " : "", last[j].text, last[j].count);
        std::strncat(lr_text, item, sizeof lr_text - std::strlen(lr_text) - 1);
    }
    logf("  census (%s): %d other car(s). car+0x4D38: %d deferred, %d other, %d none. Behind the deferred one: %d "
         "Presentation11, %d Default, %d Null, %d other. Their alpha: %d at 0, %d at %.2f or below, %d between, %d "
         "solid (%d of them handed %.2f or below), %d solid at first but not a moment later; %d shown",
         when, n, iface[0], iface[1] + iface[2] + iface[3] + iface[4], none, real[1], real[2], real[3],
         real[0] + real[4], alpha_zero, alpha_low, static_cast<double>(cap), alpha_mid, alpha_solid,
         solid_handed_low, static_cast<double>(cap), second_look, shown);
    logf("    handed by CCBR: %d at %.2f or below, %d at 0, %d above, %d not yet. Last request: %s%s", handed_low,
         static_cast<double>(cap), handed_zero, handed_more, handed_none, lr_text, more_names ? ", and others" : "");
    if (other_vt) {
        char name[128];
        rtti_name(other_vt, name, sizeof name);
        logf("    an unexpected car+0x4D38 vtable: %p (%s)", other_vt, name);
    }
}

// ---- the player's car -------------------------------------------------------------------------------

void note_player(void* car) {
    if (*static_cast<const void* const*>(car) != g_forzacar_vt) return; // a live car: the game just used it
    const ULONGLONG now = GetTickCount64();
    g_player.store(car, std::memory_order_relaxed);
    g_player_seen.store(now, std::memory_order_relaxed);
    if (!g_debug || g_player_logged.load(std::memory_order_relaxed) == car) return;
    if (now - g_player_logged_at.load(std::memory_order_relaxed) < 1000) return; // two kind-0 cars at once
    g_player_logged_at.store(now, std::memory_order_relaxed);
    if (g_player_logged.exchange(car) != car) logf("player's car is now %p (ForzaCar, kind 0)", car);
}

int d_kind(void* car) {
    const int kind = o_kind(car);
    if (car) {
        if (kind == 0) note_player(car);
        else if (kind == 1 || kind == 2) note_car(car);
    }
    return kind;
}

void* d_requests(void* car) {
    void* r = o_requests(car);
    if (car) {
        const int kind = *reinterpret_cast<const int*>(static_cast<char*>(car) + OFF_CAR_KIND);
        if (kind == 0) note_player(car);
        else if (kind == 1 || kind == 2) note_car(car);
    }
    return r;
}

// ---- ghosting ---------------------------------------------------------------------------------------

void others_on() {
    if (!g_see_through.load()) return;
    g_capped = g_zero = g_below = g_blinks = g_drops = 0;
    g_hides_held = g_hides_late = g_hides_undone = g_hides_shown = g_hides_gone = g_hides_passed = 0;
    while (g_blink_lock.test_and_set(std::memory_order_acquire)) {}
    std::memset(g_blink, 0, sizeof g_blink);
    g_blink_lock.clear(std::memory_order_release);
    g_others_on.store(true);
    const int handed = hand_over_all();
    dlogf("  see-through: other cars' alpha capped at %.2f from now (%d handed it straight away; the render side "
          "gets it next frame)",
          static_cast<double>(g_alpha.load()), handed);
    census("start, still drawn as before");
}

void others_off() {
    if (!g_others_on.load()) return;
    census("end");
    g_others_on.store(false);
    flush_hides(GetTickCount64(), true);
    const int handed = hand_over_all();
    dlogf("  see-through: other cars back to the game's own alpha (%d handed it straight away). While on, the "
          "game's alpha was capped %ld time(s), 0 %ld time(s), already low %ld time(s); %ld blink(s) to 0 under "
          "%.2f s (%s), %ld longer drop(s) to 0",
          handed, g_capped.load(), g_zero.load(), g_below.load(), g_blinks.load(), kBlinkMs / 1000.0,
          g_hold_blinks.load() ? "held" : "not held: HoldBlinks=0", g_drops.load());
    dlogf("  see-through: the game hid other cars %ld time(s): %ld held back (%ld shown again before the hide was "
          "due, %ld applied after all, %ld dropped when due as the game had shown the car meanwhile, %ld went with "
          "their car), %ld let through; %d car(s) destroyed on another thread so far",
          g_hides_held.load() + g_hides_passed.load(), g_hides_held.load(), g_hides_shown.load(), g_hides_late.load(),
          g_hides_undone.load(), g_hides_gone.load(), g_hides_passed.load(), g_dtor_elsewhere.load());
}

void ghost(const char* why) {
    if (g_ghosted.load()) return;
    void* car             = g_player.load(std::memory_order_relaxed);
    const ULONGLONG since = GetTickCount64() - g_player_seen.load(std::memory_order_relaxed);
    if (!car || since > 2000) {
        logf("%s, but no player's car was seen in the last 2 seconds: no ghost", why);
        return;
    }
    if (!g_ghost_on.load()) {
        dlogf("%s: no ghost (turned off)", why);
        return;
    }
    void* requests     = static_cast<char*>(car) + OFF_CAR_REQUESTS;
    const char* preset = kPreset;
    GameString name    = game_string(preset);
    SharedPtr handle{};
    DWORD code = 0;
    if (!call_make(requests, &handle, &name, &code)) {
        logf("%s: no ghost, making the request crashed (exception 0x%08lX)", why, code);
        return;
    }
    if (!handle.ptr) {
        if (!release_handle(&handle, &code)) logf("  releasing an empty request crashed (exception 0x%08lX)", code);
        logf("  the game has no \"%s\" preset (%s); using its own \"%s\" instead", kPreset,
             g_preset_file ? "the collidablepresets.xml override wasn't read: is mediaoverride working?"
                           : "the collidablepresets.xml override couldn't be written",
             kFallback);
        preset = kFallback;
        name   = game_string(preset);
        if (!call_make(requests, &handle, &name, &code)) {
            logf("%s: no ghost, making the request crashed (exception 0x%08lX)", why, code);
            return;
        }
        if (!handle.ptr) {
            if (!release_handle(&handle, &code))
                logf("  releasing an empty request crashed (exception 0x%08lX)", code);
            logf("%s: no ghost, the game has no \"%s\" preset either", why, kFallback);
            return;
        }
    }
    uint8_t ok = 0;
    if (!call_add(requests, &handle, &ok, &code)) {
        logf("%s: no ghost, registering the request crashed (exception 0x%08lX)", why, code);
        return;
    }
    g_ghost_car.store(car);
    g_ghost_preset = preset;
    g_ghost_since  = GetTickCount64();
    g_mid_census   = false;
    g_ghosted.store(true);
    if (g_see_through.load())
        logf("%s: your car is a ghost, and the other cars are see-through (%d%% solid)%s", why,
             static_cast<int>(g_alpha.load() * 100.0f + 0.5f), ok ? "" : " (the game said no to the ghost)");
    else
        logf("%s: your car is a ghost%s", why, ok ? "" : " (the game said no)");
    dlogf("  ghosted the player's car %p with preset \"%s\"", car, preset);
    others_on();
}

void unghost(const char* why) {
    if (!g_ghosted.load()) return;
    others_off();
    void* car            = g_ghost_car.exchange(nullptr);
    void* now            = g_player.load(std::memory_order_relaxed);
    const double seconds = (GetTickCount64() - g_ghost_since) / 1000.0;
    g_ghosted.store(false);
    if (!car) {
        logf("%s after %.1f s; your car went away meanwhile, and its ghost with it", why, seconds);
        return;
    }
    if (car != now) {
        logf("%s after %.1f s; your car changed meanwhile (%p to %p), and the old one took its ghost with it", why,
             seconds, car, now);
        return;
    }
    if (!still_player_car(car)) {
        logf("%s after %.1f s; %p no longer looks like your car, so it was left alone", why, seconds, car);
        return;
    }
    GameString name = game_string(g_ghost_preset);
    uint8_t ok      = 0;
    DWORD code      = 0;
    if (!call_remove(static_cast<char*>(car) + OFF_CAR_REQUESTS, &name, &ok, &code)) {
        logf("%s after %.1f s; removing the ghost crashed (exception 0x%08lX)", why, seconds, code);
        return;
    }
    logf("%s after %.1f s: collision back on%s", why, seconds, ok ? "" : " (the ghost was gone already)");
}

thread_local bool t_handling = false; // a game call made while handling an event sent another one

void on_event(int e, void* entity) {
    if (t_handling) {
        dlogf("event %s while handling another one: ignored", kEventDefs[e].name);
        return;
    }
    while (g_lock.test_and_set(std::memory_order_acquire)) {}
    t_handling = true;
    g_game_thread.store(GetCurrentThreadId(), std::memory_order_relaxed);
    void* car             = g_player.load(std::memory_order_relaxed);
    const ULONGLONG since = GetTickCount64() - g_player_seen.load(std::memory_order_relaxed);
    dlogf("event %s (speed camera entity %p; player's car %p, seen %llu ms ago)", kEventDefs[e].name, entity, car,
          static_cast<unsigned long long>(since));
    switch (e) {
        case ArEntered: ghost("approaching a speed camera"); break;
        case ZoneStarted:
            g_in_zone = true;
            ghost("speed zone started");
            break;
        case TrapPassed: dlogf("  speed trap passed; collision comes back when you leave its area"); break;
        case ArExited:
            if (g_in_zone) dlogf("  still in the speed zone: ghost stays");
            else unghost("left the speed camera's area");
            break;
        case ZoneCompleted:
            g_in_zone = false;
            unghost("speed zone completed");
            break;
        case ZoneAborted:
            g_in_zone = false;
            unghost("speed zone aborted");
            break;
        default: break;
    }
    t_handling = false;
    g_lock.clear(std::memory_order_release);
}

// Any other event sent on the game's thread while a ghost is on: the time limit, held hides of cars whose
// alpha the game no longer works out (the alpha hook looks at them otherwise), and with Debug=1 a census a
// few seconds in, when the AI has had time to reactivate cars further down the road.
void while_ghosted() {
    if (t_handling || !g_ghosted.load(std::memory_order_relaxed) ||
        GetCurrentThreadId() != g_game_thread.load(std::memory_order_relaxed))
        return;
    const ULONGLONG now = GetTickCount64();
    const bool on       = g_others_on.load(std::memory_order_relaxed);
    const bool timeout  = now - g_ghost_since >= kMaxGhostMs;
    const bool mid      = g_debug && !g_mid_census && on && now - g_ghost_since >= kMidCensusMs;
    ULONGLONG last      = g_last_flush.load(std::memory_order_relaxed);
    const bool flush    = on && now - last >= kFlushMs;
    if (!timeout && !mid && !flush) return;
    if (g_lock.test_and_set(std::memory_order_acquire)) return; // an event is being handled
    t_handling = true;
    if (timeout) {
        g_in_zone = false;
        unghost("no speed camera event ended the ghost within 90 seconds, so it's over");
    } else {
        if (flush && g_last_flush.compare_exchange_strong(last, now)) flush_hides(now, false);
        if (mid && g_ghosted.load() && !g_mid_census) {
            g_mid_census = true;
            census("3 s in");
        }
    }
    t_handling = false;
    g_lock.clear(std::memory_order_release);
}

int which(const void* symbol) {
    for (int i = 0; i < kEvents; ++i)
        if (g_symbols[i] && symbol == g_symbols[i]) return i;
    return -1;
}

void* d_send(void* entity, const void* symbol) {
    if (const int e = which(symbol); e >= 0) on_event(e, entity);
    else while_ghosted();
    return o_send(entity, symbol);
}

void* d_send_data(void* entity, const void* symbol, void* data) {
    if (const int e = which(symbol); e >= 0) on_event(e, entity);
    else while_ghosted();
    return o_send_data(entity, symbol, data);
}

// ---- setup ------------------------------------------------------------------------------------------

bool code_matches(uintptr_t rva, const unsigned char* want, std::size_t n) {
    unsigned char have[32];
    return n <= sizeof have && read_ok(g_base + rva, have, n) && std::memcmp(have, want, n) == 0;
}

// The symbol a "lea rdx,[rip+disp32]" (48 8D 15) at `site` loads.
const void* symbol_at(uintptr_t site) {
    unsigned char ins[7];
    if (!read_ok(g_base + site, ins, sizeof ins) || ins[0] != 0x48 || ins[1] != 0x8D || ins[2] != 0x15) return nullptr;
    int32_t disp = 0;
    std::memcpy(&disp, ins + 3, 4);
    return g_base + site + 7 + disp;
}

FH2Hook* hook(const char* name, uintptr_t rva, void* detour, void** original) {
    FH2Hook* h = g_api->hook_function(g_mod, name, g_base + rva, detour, original);
    if (!h) logf("couldn't hook %s (see the platform's lines above)", name);
    return h;
}

DWORD WINAPI worker(LPVOID) {
    g_base = static_cast<char*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!g_base) {
        logf("the game's code never became ready (or Azimuth started another game): off");
        return 0;
    }
    static const unsigned char kind[]     = {0x8B, 0x81, 0x08, 0x4D, 0x00, 0x00, 0xC3};
    static const unsigned char requests[] = {0x48, 0x8D, 0x81, 0x58, 0x4C, 0x00, 0x00, 0xC3};
    static const unsigned char send[]     = {0x48, 0x83, 0xEC, 0x48, 0x48, 0x8D, 0x05};
    static const unsigned char send_d[]   = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B, 0xD9};
    static const unsigned char make[]     = {0x48, 0x89, 0x54, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x50};
    static const unsigned char add[]      = {0x48, 0x89, 0x54, 0x24, 0x10, 0x56, 0x57, 0x41, 0x56};
    static const unsigned char remove[]   = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x4C, 0x8B, 0xCA};
    static const unsigned char dtor[]     = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57};
    static const unsigned char apply[]    = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0x05};
    static const unsigned char alpha[]    = {0x48, 0x8B, 0xC4, 0x57, 0x48, 0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00};
    static const unsigned char hide[]     = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
                                             0xD9, 0xC6, 0x81, 0xF1, 0x50, 0x00, 0x00, 0x00};
    struct Check {
        const char* what;
        uintptr_t rva;
        const unsigned char* bytes;
        std::size_t n;
    } checks[] = {
        {"car kind (0x140F62280)", RVA_CAR_KIND, kind, sizeof kind},
        {"car requests (0x140F60510)", RVA_CAR_REQUESTS, requests, sizeof requests},
        {"send event (0x14190BF90)", RVA_SEND_EVENT, send, sizeof send},
        {"send event with data (0x14190BFD0)", RVA_SEND_EVENT_DATA, send_d, sizeof send_d},
        {"make request (0x140F5B420)", RVA_MAKE_REQUEST, make, sizeof make},
        {"register request (0x140F73080)", RVA_ADD_REQUEST, add, sizeof add},
        {"remove request (0x140F5B890)", RVA_REMOVE_REQUEST, remove, sizeof remove},
        {"~ForzaCar (0x140289320)", RVA_CAR_DTOR, dtor, sizeof dtor},
        {"ForzaCar apply collidable state (0x14028D930)", RVA_CAR_APPLY, apply, sizeof apply},
        {"car alpha (0x14028C590)", RVA_CAR_ALPHA, alpha, sizeof alpha},
        {"car hide (0x140F649F0)", RVA_CAR_HIDE, hide, sizeof hide},
    };
    for (const auto& c : checks)
        if (!code_matches(c.rva, c.bytes, c.n)) {
            logf("game code doesn't match at %s: off", c.what);
            return 0;
        }
    g_forzacar_vt = g_base + RVA_FORZACAR_VT;
    void* slot    = nullptr;
    if (!read_ok(g_base + RVA_FORZACAR_VT + VT_CAR_APPLY * 8, &slot, sizeof slot) || slot != g_base + RVA_CAR_APPLY)
        logf("ForzaCar's vtable slot 0x130 isn't game+0x28D930 (another mod hooks it?); calling whatever is there");
    g_wrap_vt[0] = g_base + RVA_DEFERRED_WRAP_VT;
    g_wrap_vt[1] = g_base + RVA_P11_WRAP_VT;
    g_wrap_vt[2] = g_base + RVA_DEFAULT_WRAP_VT;
    g_wrap_vt[3] = g_base + RVA_NULL_WRAP_VT;
    g_make       = reinterpret_cast<MakeFn>(g_base + RVA_MAKE_REQUEST);
    g_add        = reinterpret_cast<AddFn>(g_base + RVA_ADD_REQUEST);
    g_remove     = reinterpret_cast<RemoveFn>(g_base + RVA_REMOVE_REQUEST);
    int events   = 0;
    for (int i = 0; i < kEvents; ++i) {
        const void* s = symbol_at(kEventDefs[i].site);
        if (s == g_base + kEventDefs[i].symbol) {
            g_symbols[i] = s;
            ++events;
        } else {
            logf("event %s: the code that sends it doesn't match (0x%llX); ignored", kEventDefs[i].name,
                 static_cast<unsigned long long>(kEventDefs[i].site + 0x140000000ull));
        }
    }
    if (!g_symbols[ZoneStarted]) {
        logf("can't see speed zones start: off");
        return 0;
    }
    const bool ok = hook("ForzaCar kind (player's car)", RVA_CAR_KIND, reinterpret_cast<void*>(&d_kind),
                         reinterpret_cast<void**>(&o_kind)) &&
                    hook("ForzaCar collidable requests (player's car)", RVA_CAR_REQUESTS,
                         reinterpret_cast<void*>(&d_requests), reinterpret_cast<void**>(&o_requests)) &&
                    hook("send event", RVA_SEND_EVENT, reinterpret_cast<void*>(&d_send),
                         reinterpret_cast<void**>(&o_send)) &&
                    hook("send event with data", RVA_SEND_EVENT_DATA, reinterpret_cast<void*>(&d_send_data),
                         reinterpret_cast<void**>(&o_send_data)) &&
                    hook("~ForzaCar (other cars going away)", RVA_CAR_DTOR, reinterpret_cast<void*>(&d_dtor),
                         reinterpret_cast<void**>(&o_dtor)) &&
                    hook("car alpha (other cars see-through)", RVA_CAR_ALPHA, reinterpret_cast<void*>(&d_car_alpha),
                         reinterpret_cast<void**>(&o_car_alpha)) &&
                    hook("car hide (other cars blinking out)", RVA_CAR_HIDE, reinterpret_cast<void*>(&d_hide),
                         reinterpret_cast<void**>(&o_hide));
    if (!ok) {
        g_api->disable_hooks(g_mod, nullptr);
        logf("not every hook could be made: off");
        return 0;
    }
    if (g_api->enable_hooks(g_mod, nullptr) != 1) {
        g_api->disable_hooks(g_mod, nullptr);
        logf("couldn't switch the hooks on: off");
        return 0;
    }
    logf("ready (%d of %d speed camera events): ghost in speed zones %s, other cars %s", events,
         static_cast<int>(kEvents), g_ghost_on.load() ? "on" : "off",
         g_see_through.load() ? "see-through meanwhile" : "left solid");
    return 0;
}

// ---- settings -----------------------------------------------------------------------------------------

std::wstring ini_path() { return std::wstring(g_api->azimuth_dir) + L"\\fh2-mods\\CCBR.ini"; }

void write_ini(const wchar_t* key, const wchar_t* value) {
    WritePrivateProfileStringW(L"CCBR", key, value, ini_path().c_str());
}

void read_settings() {
    const std::wstring ini = ini_path();
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (FILE* f = _wfopen(ini.c_str(), L"w")) {
            std::fputs("; CCBR (Can't Crash, Busy Racing). The first three are on the MODS page too.\n"
                       "[CCBR]\n"
                       "; 1 = your car is a ghost (no collision) for the length of a speed zone; 0 = it isn't\n"
                       "Ghost=1\n"
                       "; 1 = every other car (Drivatars and traffic) is see-through while your car is a ghost\n"
                       "SeeThrough=1\n"
                       "; how solid the other cars look meanwhile: 0.05 (almost invisible) to 1.0 (solid)\n"
                       "OthersAlpha=0.5\n"
                       "; 1 = a see-through car the game hides for a split second stays as it is instead of "
                       "blinking out\n"
                       "HoldBlinks=1\n"
                       "; 0 = don't check GitHub for a newer version\n"
                       "CheckUpdates=1\n"
                       "; 1 = log every speed camera event and what the other cars look like (for bug reports)\n"
                       "Debug=0\n",
                       f);
            std::fclose(f);
        }
    }
    g_ghost_on       = GetPrivateProfileIntW(L"CCBR", L"Ghost", 1, ini.c_str()) != 0;
    g_see_through    = GetPrivateProfileIntW(L"CCBR", L"SeeThrough", 1, ini.c_str()) != 0;
    g_hold_blinks    = GetPrivateProfileIntW(L"CCBR", L"HoldBlinks", 1, ini.c_str()) != 0;
    g_check_updates  = GetPrivateProfileIntW(L"CCBR", L"CheckUpdates", 1, ini.c_str()) != 0;
    g_debug          = GetPrivateProfileIntW(L"CCBR", L"Debug", 0, ini.c_str()) != 0;
    wchar_t text[32] = {};
    GetPrivateProfileStringW(L"CCBR", L"OthersAlpha", L"0.5", text, 32, ini.c_str());
    const float a = static_cast<float>(_wtof(text));
    g_alpha       = a >= 0.05f && a <= 1.0f ? a : 0.5f;
}

// The MODS page's rows (UI thread): save the value and use it.
void on_switch(void*, const char* key, int value) {
    const bool on = value != 0;
    if (std::strcmp(key, "Ghost") == 0) {
        g_ghost_on = on;
        write_ini(L"Ghost", on ? L"1" : L"0");
    } else if (std::strcmp(key, "SeeThrough") == 0) {
        g_see_through = on;
        write_ini(L"SeeThrough", on ? L"1" : L"0");
    }
}
void on_slide(void*, const char*, float value) {
    const float a = value < 0.05f ? 0.05f : value > 1.0f ? 1.0f : value;
    g_alpha       = a;
    wchar_t text[16];
    std::swprintf(text, 16, L"%.2f", static_cast<double>(a));
    write_ini(L"OthersAlpha", text);
}

void add_rows() {
    if (g_api->api_version < 4) return;
    FH2Setting ghost{};
    ghost.size        = sizeof ghost;
    ghost.key         = "Ghost";
    ghost.label       = "Ghost car in speed zones";
    ghost.description = "Your car drives straight through traffic and Drivatars from the start of a speed zone to "
                        "its end. Counts from the next zone.";
    ghost.value       = g_ghost_on.load() ? 1 : 0;
    ghost.on_change   = on_switch;
    g_api->add_setting(g_mod, &ghost);
    FH2Setting see{};
    see.size        = sizeof see;
    see.key         = "SeeThrough";
    see.label       = "See-through traffic";
    see.description = "While your car is a ghost, every other car is see-through, so you can see what you're "
                      "driving through. Counts from the next zone.";
    see.value       = g_see_through.load() ? 1 : 0;
    see.on_change   = on_switch;
    g_api->add_setting(g_mod, &see);
    if (g_api->api_version < 5) return;
    FH2Setting alpha{};
    alpha.size          = sizeof alpha;
    alpha.key           = "OthersAlpha";
    alpha.label         = "How solid traffic looks";
    alpha.description   = "How solid the other cars look while your car is a ghost: 100 is solid, lower is more "
                          "see-through.";
    alpha.kind          = FH2_SETTING_SLIDER;
    alpha.min           = 0.05f;
    alpha.max           = 1.0f;
    alpha.step          = 0.05f;
    alpha.slider_value  = g_alpha.load();
    alpha.display_scale = 100.0f;
    alpha.decimals      = 0;
    alpha.on_slide      = on_slide;
    g_api->add_setting(g_mod, &alpha);
}

// Started for online play (FH2MP), as platform 2.8.0 tells it: FH2MP.dll in fh2-mods, and its launcher's
// session (FH2MP_TOKEN, or data\bin\fh2mp-session.txt). From platform 2.8.0 on, the platform keeps CCBR out
// of online play itself (offline_only).
bool started_online() {
    const std::wstring dir = g_api->azimuth_dir;
    if (GetFileAttributesW((dir + L"\\fh2-mods\\FH2MP.dll").c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    wchar_t env[8];
    if (GetEnvironmentVariableW(L"FH2MP_TOKEN", env, 8) > 0) return true;
    return GetFileAttributesW((dir + L"\\data\\bin\\fh2mp-session.txt").c_str()) != INVALID_FILE_ATTRIBUTES;
}

// CCBR's preset, added to the game's own collidablepresets.xml through mediaoverride.
void add_preset() {
    const std::wstring zip = std::wstring(g_api->game_dir) + L"\\media\\" +
                             std::wstring(kPresetsZip, kPresetsZip + sizeof kPresetsZip - 1);
    const std::string xml  = g_api->game_dir[0] ? presets_with_ours(unzip_file(zip, kPresetsFile)) : std::string();
    if (xml.empty()) {
        logf("couldn't read the game's collidablepresets.xml (media\\%s); the ghost uses the game's own "
             "\"%s\" preset",
             kPresetsZip, kFallback);
        return;
    }
    g_preset_file = g_api->set_file(g_mod, "GameTunableSettings/collidablepresets.xml", xml.data(),
                                    static_cast<uint32_t>(xml.size())) == 1;
    if (g_preset_file) logf("added the \"%s\" preset (collision off) to collidablepresets.xml", kPreset);
    else
        logf("couldn't add the \"%s\" preset to collidablepresets.xml; the ghost uses the game's own \"%s\" preset",
             kPreset, kFallback);
}

} // namespace

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    g_api = api;
    if (api->api_version < 3) return 1;
    read_settings();
    FH2ModInfo m{};
    m.size             = sizeof m;
    m.name             = "CCBR";
    m.version          = CCBR_VERSION;
    m.github_repo      = "Trinculo54/CCBR";
    m.platform_version = FH2MOD_PLATFORM_VERSION;
    m.check_updates    = g_check_updates ? 1 : 0;
    m.description      = "Can't Crash, Busy Racing: your car is a ghost during speed zones, so you drive straight "
                         "through traffic and Drivatars, and they're see-through meanwhile. Offline only.";
    m.offline_only     = 1;
    if (api->api_version < 9) m.size = offsetof(FH2ModInfo, offline_only);
    if (api->api_version < 6) m.size = offsetof(FH2ModInfo, description);
    g_mod = api->register_mod(&m);
    if (!g_mod) return 0; // platform 2.8.0 and later: started for online play
    if (api->api_version < 9 && started_online()) {
        logf("started for online play (FH2MP): off, since a ghost would only be one on your own screen");
        return 0;
    }
    add_preset();
    add_rows();
    if (HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
