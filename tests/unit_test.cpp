// Unit test of CCBR's ghost and see-through logic: the mod's own source with the game's functions replaced by
// fakes that record which requests each car has and what alpha the game would draw it with. Also its
// settings, the online check and the preset file. Built with -DCCBR_TESTS=ON:
//   unit_test.exe [GameTunableSettings.zip]
// With the game's zip (copied from media), the preset is also added to the game's own collidablepresets.xml.
#include "ccbr.cpp"

#include <filesystem>
#include <map>
#include <set>
#include <vector>

namespace {

int fails = 0;
#define CHECK(x) do { bool ok_ = (x); std::printf("%s %s\n", ok_ ? "ok  " : "FAIL", #x); if (!ok_) ++fails; } while (0)

std::vector<std::string> g_lines;
void fake_log(FH2Mod*, const char* s) {
    g_lines.emplace_back(s);
    std::printf("    | %s\n", s);
}
bool logged(const char* s) {
    for (const auto& l : g_lines)
        if (l.find(s) != std::string::npos) return true;
    return false;
}

// register_mod: keeps what it was given (as much as its size says), and registers or not.
FH2ModInfo g_registered;
bool g_register_ok = true;
FH2Mod* fake_register(const FH2ModInfo* info) {
    g_registered = {};
    std::memcpy(&g_registered, info, info->size < sizeof g_registered ? info->size : sizeof g_registered);
    return g_register_ok ? reinterpret_cast<FH2Mod*>(&g_registered) : nullptr;
}

std::map<void*, std::set<std::string>> g_requests; // per request set (car + 0x4C58)
std::set<void*> g_freed;                          // freed cars: must never be touched
int g_touched_freed = 0;
bool g_presets = true; // the game has CCBR's preset

SharedPtr* fake_make(void* req, SharedPtr* out, const GameString* name) {
    if (g_freed.count(static_cast<char*>(req) - OFF_CAR_REQUESTS)) ++g_touched_freed;
    const std::string n(name->buf, name->size);
    out->ptr  = (g_presets || n == "intersection") ? new std::string(n) : nullptr;
    out->ctrl = nullptr;
    return out;
}
uint8_t fake_add(void* req, SharedPtr* h) {
    if (g_freed.count(static_cast<char*>(req) - OFF_CAR_REQUESTS)) ++g_touched_freed;
    auto* n = static_cast<std::string*>(h->ptr);
    g_requests[req].insert(*n);
    delete n;
    return 1;
}
uint8_t fake_remove(void* req, const GameString* name) {
    if (g_freed.count(static_cast<char*>(req) - OFF_CAR_REQUESTS)) ++g_touched_freed;
    return g_requests[req].erase(std::string(name->buf, name->size)) ? 1 : 0;
}
int fake_kind(void* car) { return *reinterpret_cast<int*>(static_cast<char*>(car) + OFF_CAR_KIND); }
void* fake_requests(void* car) { return static_cast<char*>(car) + OFF_CAR_REQUESTS; }
void* fake_send(void*, const void*) { return nullptr; }
void* fake_send_data(void*, const void*, void*) { return nullptr; }
void fake_dtor(void*) {}

// The game's alpha for each car (1.0, solid, unless set), and ForzaCar's slot 0x130, which works it out
// through the (hooked) alpha function and hands it over, as the game's per-frame update also does.
std::map<void*, float> g_game_alpha;
float fake_car_alpha(void*, void** car) {
    auto it = g_game_alpha.find(*car);
    return it == g_game_alpha.end() ? 1.0f : it->second;
}
std::map<void*, int> g_applies;
std::map<void*, float> g_drawn; // what each car's presentation was last handed
void fake_apply(void* car) {
    if (g_freed.count(car)) ++g_touched_freed;
    ++g_applies[car];
    g_drawn[car] = d_car_alpha(nullptr, &car);
}
float frame(void* car) { return d_car_alpha(nullptr, &car); } // the game's per-frame update for one car

// The game's hide (0x140F649F0) past CCBR's hook, its show, and the request set's update that calls
// them: on a change of the newest request's alpha, hide (0 or below) or show, then keep it at set+0x58.
std::map<void*, int> g_hides, g_shows;
void fake_hide(void* car) {
    if (g_freed.count(car)) ++g_touched_freed;
    ++g_hides[car];
}
float& applied(char* car) { return *reinterpret_cast<float*>(car + OFF_CAR_REQUESTS + OFF_SET_APPLIED); }
void set_update(char* car) {
    void* p         = car;
    const float raw = fake_car_alpha(nullptr, &p);
    if (raw != applied(car)) {
        if (raw <= 0.0f) d_hide(car);
        else ++g_shows[car];
    }
    applied(car) = raw;
}

void* g_vt[64];
char g_sym[kEvents][8];
char g_other_sym[8];
char g_entity[64];

// Presentations, for the census: vtables with an empty slot before them (no RTTI).
void* g_vtables[5][4];
const void* vt(int i) { return &g_vtables[i][1]; }
struct Inner { // the real presentation
    const void* vt;
    char pad[OFF_REAL_ALPHA - 8];
    float alpha;
    uint32_t shown;
};
struct Iface { // what car+0x4D38 points at
    const void* vt;
    Inner* inner;
};

// The request list, for the census: set+8 is the sentinel of a std::list whose last node (sentinel+8)
// holds the request at +0x10; the request's +0x10 is its preset, whose name is a std::string at +0x20.
struct Node {
    Node* next;
    Node* prev;
    void* request;
};
void set_last_request(char* car, const char* name) {
    auto* preset  = new char[0x48]();
    GameString s  = game_string(name);
    std::memcpy(preset + 0x20, &s, sizeof s);
    auto* request = new char[0x20]();
    *reinterpret_cast<char**>(request + 0x10) = preset;
    auto* sentinel = new Node{};
    auto* node     = new Node{sentinel, sentinel, request};
    sentinel->next = sentinel->prev = node;
    *reinterpret_cast<Node**>(car + OFF_CAR_REQUESTS + 8) = sentinel;
}
void set_presentation(char* car, int wrapper, int real, float alpha, bool shown) {
    auto* inner = new Inner{vt(real), {}, alpha, shown ? 1u : 0u};
    auto* iface = new Iface{vt(wrapper), inner};
    *reinterpret_cast<Iface**>(car + OFF_CAR_PRES) = iface;
}

char* new_car(int kind) {
    auto* c = static_cast<char*>(VirtualAlloc(nullptr, 0x5000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    *reinterpret_cast<void**>(c)        = g_vt;
    *reinterpret_cast<int*>(c + OFF_CAR_KIND) = kind;
    g_freed.erase(c); // Windows may hand out a freed car's address again: a new car now
    g_requests.erase(c + OFF_CAR_REQUESTS);
    g_game_alpha.erase(c);
    g_applies.erase(c);
    g_drawn.erase(c);
    g_hides.erase(c);
    g_shows.erase(c);
    applied(c) = 1.0f; // shown
    return c;
}
void seen(char* car, int times = 32) {
    for (int i = 0; i < times; ++i) d_kind(car);
}
bool has_req(char* car, const char* preset) { return g_requests[car + OFF_CAR_REQUESTS].count(preset) != 0; }
void destroy(char* car) {
    d_dtor(car);
    g_freed.insert(car);
    VirtualFree(car, 0, MEM_RELEASE);
}

std::string slurp(const std::filesystem::path& f) {
    std::string s;
    if (FILE* in = _wfopen(f.c_str(), L"rb")) {
        char buf[4096];
        for (std::size_t n; (n = std::fread(buf, 1, sizeof buf, in)) > 0;) s.append(buf, n);
        std::fclose(in);
    }
    return s;
}
void touch(const std::filesystem::path& f) {
    std::filesystem::create_directories(f.parent_path());
    if (FILE* out = _wfopen(f.c_str(), L"wb")) std::fclose(out);
}

} // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    static FH2ModApi api{};
    api.log     = fake_log;
    api.api_version = FH2MOD_API_VERSION;
    g_api       = &api;
    g_debug     = true; // the censuses and every event, for the checks below

    // The preset: added before the closing tag, with the file's own line ends; never twice.
    {
        const std::string crlf = "<carcollidablepresets>\r\n    <preset name=\"ai_ow\" collision=\"on\"/>\r\n"
                                 "</carcollidablepresets>\r\n";
        const std::string with = presets_with_ours(crlf);
        CHECK(with.find("    <preset name=\"ccbr\" collision=\"off\" />\r\n\r\n</carcollidablepresets>") !=
              std::string::npos);
        CHECK(with.find("<preset name=\"ai_ow\" collision=\"on\"/>") != std::string::npos);
        CHECK(presets_with_ours(with) == with);
        CHECK(presets_with_ours("<carcollidablepresets>\n").empty()); // no closing tag
        CHECK(presets_with_ours("").empty());
        const std::string lf = presets_with_ours("<carcollidablepresets>\n</carcollidablepresets>\n");
        CHECK(lf.find("\r") == std::string::npos && lf.find("<preset name=\"ccbr\" collision=\"off\" />\n") != std::string::npos);
    }
    if (argc > 1) {
        const std::string game = unzip_file(fs::path(argv[1]).wstring(), "collidablepresets.xml");
        CHECK(game.find("<preset name=\"intersection\" collision=\"off\"/>") != std::string::npos);
        const std::string ours = presets_with_ours(game);
        CHECK(ours.size() > game.size() && ours.find("<preset name=\"ccbr\" collision=\"off\" />") != std::string::npos);
        CHECK(ours.find("<preset name=\"tag_ghosting\"") != std::string::npos);
        CHECK(unzip_file(fs::path(argv[1]).wstring(), "no-such-file.xml").empty());
    } else {
        std::printf("(no GameTunableSettings.zip given: the game's own presets file isn't tried)\n");
    }
    CHECK(unzip_file(L"no-such-zip.zip", "collidablepresets.xml").empty());

    // Settings: the ini made with the defaults, the MODS page's changes saved to it, and the online check.
    const fs::path dir = fs::absolute("unit-test-azimuth");
    fs::remove_all(dir);
    fs::create_directories(dir / "fh2-mods");
    static const std::wstring dir_w = dir.wstring();
    api.azimuth_dir = dir_w.c_str();
    read_settings();
    const std::string ini = slurp(dir / "fh2-mods" / "CCBR.ini");
    CHECK(ini.find("Ghost=1") != std::string::npos && ini.find("SeeThrough=1") != std::string::npos &&
          ini.find("OthersAlpha=0.5") != std::string::npos && ini.find("HoldBlinks=1") != std::string::npos &&
          ini.find("CheckUpdates=1") != std::string::npos && ini.find("Debug=0") != std::string::npos);
    CHECK(ini.find("; 1 = your car is a ghost") != std::string::npos);
    CHECK(g_ghost_on.load() && g_see_through.load() && g_alpha.load() == 0.5f && g_hold_blinks.load() && !g_debug);
    on_switch(nullptr, "Ghost", 0);
    on_switch(nullptr, "SeeThrough", 0);
    on_slide(nullptr, "OthersAlpha", 0.3f);
    CHECK(!g_ghost_on.load() && !g_see_through.load() && g_alpha.load() == 0.3f);
    read_settings(); // as the next launch would
    CHECK(!g_ghost_on.load() && !g_see_through.load() && g_alpha.load() == 0.3f);
    on_slide(nullptr, "OthersAlpha", 5.0f);
    CHECK(g_alpha.load() == 1.0f);
    on_switch(nullptr, "Ghost", 1);
    on_switch(nullptr, "SeeThrough", 1);
    on_slide(nullptr, "OthersAlpha", 0.5f);
    WritePrivateProfileStringW(L"CCBR", L"OthersAlpha", L"7", (dir / "fh2-mods" / "CCBR.ini").c_str());
    read_settings();
    CHECK(g_ghost_on.load() && g_see_through.load() && g_alpha.load() == 0.5f); // out of range: the default
    CHECK(!started_online());
    touch(dir / "data" / "bin" / "fh2mp-session.txt");
    CHECK(!started_online()); // a session file, but no FH2MP
    touch(dir / "fh2-mods" / "FH2MP.dll");
    CHECK(started_online());
    fs::remove(dir / "data" / "bin" / "fh2mp-session.txt");
    CHECK(!started_online());

    // Registering: offline_only from platform 2.8.0, and as much of FH2ModInfo as an older platform has.
    {
        const FH2ModInfo now = mod_info(9, true);
        CHECK(now.size == sizeof(FH2ModInfo) && now.offline_only == 1 && now.check_updates == 1);
        CHECK(std::strcmp(now.name, "CCBR") == 0 && std::strcmp(now.github_repo, "Trinculo54/CCBR") == 0);
        CHECK(now.description && std::strcmp(now.platform_version, FH2MOD_PLATFORM_VERSION) == 0);
        CHECK(mod_info(8, false).size == offsetof(FH2ModInfo, offline_only) && mod_info(8, false).check_updates == 0);
        CHECK(mod_info(5, true).size == offsetof(FH2ModInfo, description));
        // Platform 2.8.0 online: it keeps CCBR out, and fh2mod_init returns before doing anything.
        api.register_mod = fake_register;
        g_register_ok    = false;
        g_lines.clear();
        CHECK(fh2mod_init(&api) == 0);
        CHECK(g_registered.size == sizeof(FH2ModInfo) && g_registered.offline_only == 1);
        CHECK(g_mod == nullptr && g_lines.empty());
        // A platform from before hooks (2.0 or 2.1): registered for the title screen's notice, with a log line,
        // and CCBR's own state left alone (a newer platform may have started it in the same game).
        WritePrivateProfileStringW(L"CCBR", L"CheckUpdates", L"0", (dir / "fh2-mods" / "CCBR.ini").c_str());
        static FH2ModApi old{};
        old.api_version  = 2;
        old.azimuth_dir  = api.azimuth_dir;
        old.register_mod = fake_register;
        old.log          = fake_log;
        g_register_ok    = true;
        CHECK(fh2mod_init(&old) == 0);
        CHECK(g_registered.size == offsetof(FH2ModInfo, description) && g_registered.check_updates == 0);
        CHECK(std::strcmp(g_registered.name, "CCBR") == 0 && std::strcmp(g_registered.version, CCBR_VERSION) == 0);
        CHECK(logged("needs FH2 Mod Platform 2.2.0 or newer, so it's off") && logged("extract CCBR's zip again"));
        CHECK(g_api == &api && g_mod == nullptr);
        g_register_ok = false;
        CHECK(fh2mod_init(&old) == 1); // not registered: nothing to tell the player through
        CHECK(fh2mod_init(nullptr) == 1);
        api.register_mod = nullptr;
        g_lines.clear();
    }
    fs::remove_all(dir);
    g_debug = true;
    g_vt[VT_CAR_APPLY] = reinterpret_cast<void*>(&fake_apply);
    g_forzacar_vt      = g_vt;
    for (int i = 0; i < 4; ++i) g_wrap_vt[i] = vt(i); // deferred, P11, Default, Null; vt(4) is another kind
    g_make = fake_make, g_add = fake_add, g_remove = fake_remove;
    o_kind = fake_kind, o_requests = fake_requests, o_send = fake_send, o_send_data = fake_send_data;
    o_dtor = fake_dtor, o_car_alpha = fake_car_alpha, o_hide = fake_hide;
    for (int i = 0; i < kEvents; ++i) g_symbols[i] = g_sym[i];

    // Reading memory that isn't there fails rather than crashing.
    {
        void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
        int x = 0, y = 7;
        CHECK(!read_ok(page, &x, sizeof x));
        CHECK(!read_ok(reinterpret_cast<void*>(16), &x, sizeof x));
        CHECK(read_ok(&y, &x, sizeof x) && x == 7);
        DWORD code = 0;
        CHECK(!call_apply(page, &code) && code == EXCEPTION_ACCESS_VIOLATION);
    }

    char* player = new_car(0);
    char* a = new_car(2);       // traffic
    char* b = new_car(1);       // a Drivatar
    char* c = new_car(2);
    char* remote = new_car(4);  // another player's AI: left alone
    d_kind(player);
    seen(a), seen(b), seen(c), seen(remote);
    CHECK(g_player.load() == player);

    // Before a ghost, the game's alpha and hides are left alone.
    CHECK(frame(a) == 1.0f && frame(b) == 1.0f);
    d_hide(a);
    CHECK(g_hides[a] == 1);
    g_hides.erase(a);

    // The census's view of the cars: a with a real presentation drawn solid, b with a Default placeholder,
    // c with nothing yet; a's last request is ai_ow.
    set_presentation(a, 0, 1, 1.0f, true);
    set_presentation(b, 0, 2, 0.0f, false);
    set_last_request(a, "ai_ow");
    set_last_request(b, "default_local");

    // A speed zone starts: the player's car ghosted, the other three handed a capped alpha straight away.
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(has_req(player, "ccbr"));
    CHECK(g_requests[a + OFF_CAR_REQUESTS].empty()); // no request on the others any more
    CHECK(g_applies[a] == 1 && g_applies[b] == 1 && g_applies[c] == 1);
    CHECK(g_applies[player] == 0 && g_applies[remote] == 0);
    CHECK(g_drawn[a] == 0.5f && g_drawn[b] == 0.5f && g_drawn[c] == 0.5f);
    CHECK(logged("see-through: other cars' alpha capped at 0.50 from now (3 handed it straight away; the render "
                 "side gets it next frame)"));
    CHECK(logged("census (start, still drawn as before): 3 other car(s). car+0x4D38: 2 deferred, 0 other, 1 none. "
                 "Behind the deferred one: 1 Presentation11, 1 Default, 0 Null, 0 other. Their alpha: 0 at 0, 0 at "
                 "0.50 or below, 0 between, 1 solid (1 of them handed 0.50 or below), 0 solid at first but not a "
                 "moment later; 1 shown"));
    CHECK(logged("    handed by CCBR: 3 at 0.50 or below, 0 at 0, 0 above, 0 not yet. Last request: "));
    CHECK(logged("ai_ow x1") && logged("default_local x1")); // in the table's order

    // Every frame from now: Drivatars and traffic capped, whichever request the game took the alpha from.
    CHECK(frame(a) == 0.5f && frame(b) == 0.5f);
    g_game_alpha[a] = 0.8f; // fading in
    CHECK(frame(a) == 0.5f);
    g_game_alpha[a] = 0.2f; // fading in, still below the cap: left
    CHECK(frame(a) == 0.2f);
    set_update(a);
    // The AI re-activating it zeroes its request's alpha for a moment: the game hides it (the set's update
    // calls the hide function) and hands over 0. Both held back.
    g_game_alpha[a] = 0.0f;
    set_update(a);
    CHECK(g_hides[a] == 0);
    CHECK(frame(a) == 0.2f);
    g_game_alpha[a] = 1.0f; // back within 0.15 s: the game shows it again, and it was never hidden
    set_update(a);
    CHECK(frame(a) == 0.5f);
    CHECK(g_hides[a] == 0 && g_shows[a] == 2);
    // Hidden for longer (loading, parked): hidden after 0.15 s, its held hide applied then.
    g_game_alpha[a] = 0.0f;
    set_update(a);
    CHECK(frame(a) == 0.5f && g_hides[a] == 0);
    Sleep(200);
    CHECK(frame(a) == 0.0f);
    CHECK(g_hides[a] == 1);
    CHECK(frame(a) == 0.0f && g_hides[a] == 1);
    // HoldBlinks=0: a blink shows, hide and all.
    g_hold_blinks = false;
    g_game_alpha[a] = 1.0f;
    set_update(a);
    CHECK(frame(a) == 0.5f);
    g_game_alpha[a] = 0.0f;
    set_update(a);
    CHECK(g_hides[a] == 2 && frame(a) == 0.0f);
    g_hold_blinks = true;
    g_game_alpha.erase(a);
    set_update(a);
    CHECK(frame(a) == 0.5f);
    // A held hide the game undoes itself, showing the car again before CCBR sees its alpha: dropped.
    g_game_alpha[c] = 0.0f;
    set_update(c);
    g_game_alpha[c] = 1.0f;
    set_update(c);
    Sleep(200);
    d_send(g_entity, g_other_sym); // held hides are looked at on any event too
    CHECK(g_hides[c] == 0);
    // A held hide of a car whose alpha the game no longer works out: applied from the event.
    CHECK(frame(c) == 0.5f);
    g_game_alpha[c] = 0.0f;
    set_update(c);
    CHECK(g_hides[c] == 0);
    Sleep(200);
    d_send(g_entity, g_other_sym);
    CHECK(g_hides[c] == 1);
    g_game_alpha.erase(c);
    set_update(c);
    CHECK(frame(c) == 0.5f);
    // The player's car and other players' cars: their hides and alpha as the game has them.
    d_hide(player), d_hide(remote);
    CHECK(g_hides[player] == 1 && g_hides[remote] == 1);
    CHECK(frame(player) == 1.0f && frame(remote) == 1.0f);
    {
        void* none = nullptr;
        CHECK(d_car_alpha(nullptr, &none) == 1.0f && d_car_alpha(nullptr, nullptr) == 1.0f);
        char other[0x5000] = {}; // not a ForzaCar
        *reinterpret_cast<int*>(other + OFF_CAR_KIND) = 2;
        void* p = other;
        CHECK(d_car_alpha(nullptr, &p) == 1.0f);
    }

    // A pool car the AI re-activates further down the road: its alpha comes from the AI's fade, and is capped
    // all the same. It turns up in the table as the game uses it.
    char* d = new_car(2);
    seen(d);
    g_game_alpha[d] = 1.0f;
    CHECK(frame(d) == 0.5f);

    // A few seconds in, a census on the next event on the game's thread.
    set_presentation(a, 0, 1, 0.5f, true);
    set_presentation(c, 4, 1, 1.0f, true);
    d_send(g_entity, g_other_sym);
    CHECK(!logged("census (3 s in)"));
    g_ghost_since -= kMidCensusMs;
    d_send(g_entity, g_other_sym);
    CHECK(logged("census (3 s in): 4 other car(s). car+0x4D38: 2 deferred, 1 other, 1 none. Behind the deferred "
                 "one: 1 Presentation11, 1 Default, 0 Null, 0 other. Their alpha: 0 at 0, 1 at 0.50 or below, 0 "
                 "between, 0 solid (0 of them handed 0.50 or below), 0 solid at first but not a moment later; 1 "
                 "shown"));
    CHECK(logged("    handed by CCBR: 4 at 0.50 or below, 0 at 0, 0 above, 0 not yet."));
    CHECK(logged("an unexpected car+0x4D38 vtable"));
    const std::size_t lines = g_lines.size();
    d_send(g_entity, g_other_sym);
    CHECK(g_lines.size() == lines); // only once

    // One goes away mid-zone and its memory is freed: never touched again.
    destroy(b);

    // A car whose hide is held when the zone ends: hidden then. One whose hide is held when it goes away: left.
    char* f = new_car(2);
    char* g = new_car(1);
    seen(f), seen(g);
    CHECK(frame(f) == 0.5f && frame(g) == 0.5f);
    g_game_alpha[f] = g_game_alpha[g] = 0.0f;
    set_update(f), set_update(g);
    CHECK(g_hides[f] == 0 && g_hides[g] == 0);
    destroy(g);

    // The zone ends: the game's own alpha again, handed over straight away, the freed cars left alone.
    d_send(g_entity, g_sym[ZoneCompleted]);
    CHECK(!has_req(player, "ccbr"));
    CHECK(g_hides[f] == 1);
    CHECK(g_applies[a] == 2 && g_applies[c] == 2 && g_applies[d] == 1);
    CHECK(g_drawn[a] == 1.0f && g_drawn[c] == 1.0f && g_drawn[d] == 1.0f && g_drawn[f] == 0.0f);
    CHECK(frame(a) == 1.0f && frame(d) == 1.0f);
    CHECK(g_touched_freed == 0);
    CHECK(logged("census (end): 4 other car(s)."));
    CHECK(logged("see-through: other cars back to the game's own alpha (4 handed it straight away). While on, the "
                 "game's alpha was capped "));
    CHECK(logged("2 blink(s) to 0 under 0.15 s (held), 2 longer drop(s) to 0")); // a, and c applied late
    // a's blink shown again; a, c and f applied after all; c's dropped; g's gone with it; a's with HoldBlinks=0.
    CHECK(logged("see-through: the game hid other cars 7 time(s): 6 held back (1 shown again before the hide was "
                 "due, 3 applied after all, 1 dropped when due as the game had shown the car meanwhile, 1 went with "
                 "their car), 1 let through; 0 car(s) destroyed on another thread"));
    CHECK(g_hides_held == g_hides_shown + g_hides_late + g_hides_undone + g_hides_gone);
    CHECK(logged("speed zone completed after ") && logged(": collision back on"));
    g_game_alpha.erase(f);
    set_update(f);

    // The player's car goes away while ghosted: nothing removed from it, the others back to normal.
    d_kind(player);
    seen(a), seen(c), seen(d); // in the game, cars are seen all the time
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(frame(a) == 0.5f);
    destroy(player);
    d_send(g_entity, g_sym[ZoneAborted]);
    CHECK(logged("your car went away meanwhile"));
    CHECK(frame(a) == 1.0f);
    CHECK(g_touched_freed == 0);

    // A ghost no event ends: over after 90 seconds.
    char* player2 = new_car(0);
    d_kind(player2);
    seen(a);
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(has_req(player2, "ccbr") && frame(a) == 0.5f);
    g_ghost_since -= kMaxGhostMs;
    d_send(g_entity, g_other_sym);
    CHECK(logged("no speed camera event ended the ghost within 90 seconds"));
    CHECK(!has_req(player2, "ccbr") && frame(a) == 1.0f);

    // Lots of cars come and go (traffic churn): the table keeps working.
    std::vector<char*> many;
    for (int round = 0; round < 6; ++round) {
        for (int i = 0; i < 200; ++i) {
            many.push_back(new_car(2));
            seen(many.back());
        }
        for (char* m : many) destroy(m);
        many.clear();
    }
    char* e = new_car(2);
    d_kind(player2);
    seen(e), seen(a);
    const int before = g_applies[a];
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(has_req(player2, "ccbr"));
    CHECK(g_applies[e] == 1 && g_applies[a] == before + 1 && g_drawn[e] == 0.5f);
    d_send(g_entity, g_sym[ZoneCompleted]);
    CHECK(frame(e) == 1.0f);
    CHECK(g_touched_freed == 0);

    // SeeThrough=0: only the player's car.
    g_see_through = false;
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(has_req(player2, "ccbr") && frame(e) == 1.0f);
    d_send(g_entity, g_sym[ZoneCompleted]);
    g_see_through = true;

    // Another OthersAlpha.
    g_alpha = 0.25f;
    seen(e);
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(frame(e) == 0.25f);
    d_send(g_entity, g_sym[ZoneCompleted]);
    g_alpha = 0.5f;

    // No "ccbr" preset (mediaoverride not read): the player falls back to "intersection"; the others are still
    // see-through, since that needs no preset.
    g_presets = false;
    seen(e);
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(has_req(player2, "intersection"));
    CHECK(frame(e) == 0.5f);
    d_send(g_entity, g_sym[ZoneCompleted]);
    CHECK(!has_req(player2, "intersection") && frame(e) == 1.0f);
    g_presets = true;

    // Without Debug, a zone logs two lines: the ghost and its end. No census, no events.
    g_debug = false;
    g_lines.clear();
    seen(e);
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(has_req(player2, "ccbr") && frame(e) == 0.5f);
    d_send(g_entity, g_sym[ZoneCompleted]);
    CHECK(!has_req(player2, "ccbr"));
    CHECK(g_lines.size() == 2);
    CHECK(logged("speed zone started: your car is a ghost, and the other cars are see-through (50% solid)"));
    CHECK(logged("speed zone completed after ") && logged(": collision back on"));
    CHECK(!logged("census") && !logged("event "));
    // Ghost turned off on the MODS page: from the next zone, nothing happens.
    g_ghost_on = false;
    g_lines.clear();
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(!has_req(player2, "ccbr") && frame(e) == 1.0f && g_lines.empty());
    d_send(g_entity, g_sym[ZoneCompleted]);
    CHECK(g_lines.empty());
    g_ghost_on = true;
    // The slider during a ghost: straight away.
    d_send_data(g_entity, g_sym[ZoneStarted], nullptr);
    CHECK(frame(e) == 0.5f);
    g_alpha = 0.2f;
    CHECK(frame(e) == 0.2f);
    d_send(g_entity, g_sym[ZoneCompleted]);
    CHECK(frame(e) == 1.0f);
    g_alpha = 0.5f;

    std::printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails;
}
