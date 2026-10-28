// Offline test of CCBR against the real game exe, run as "azimuth.exe" by tests/run_harness.sh in a folder
// with d3dcompiler_47.dll (the platform), fh2-mods\CCBR.dll and CCBR.ini (Debug=1),
// FH2\Mount\forza_durango_release_final.exe and FH2\Mount\media\GameTunableSettings.zip.
//   azimuth.exe <forza_durango_release_final.exe> [online]
// It maps the game exe as Azimuth does (nothing of the game runs), waits for CCBR to hook it, then calls the
// hooked car-kind getter with a fake ForzaCar and sends speed camera events through the hooked send
// function. The game's preset table doesn't exist here, so the ghost's request must fail safely. With
// "online", the folder looks like a game started from FH2MP's launcher, and CCBR must stay off.
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
static int fails = 0;
#define CHECK(x) do { bool ok_ = (x); std::printf("%s %s\n", ok_ ? "ok  " : "FAIL", #x); if (!ok_) ++fails; } while (0)

static std::string slurp(const fs::path& f) {
    std::ifstream in{f, std::ios::binary};
    return {std::istreambuf_iterator<char>(in), {}};
}
static bool has(const std::string& s, const char* x) { return s.find(x) != std::string::npos; }

// The game's operator new and delete (game+0x3229C0, +0x31C7B0) use its own TLS, which only the real game
// sets up: here they go to the process heap.
static void* fake_new(std::size_t n) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n ? n : 1); }
static void fake_delete(void* p) {
    if (p) HeapFree(GetProcessHeap(), 0, p);
}
static void jump_to(char* at, void* to) {
    at[0] = 0x48;
    at[1] = static_cast<char>(0xB8); // mov rax, imm64
    std::memcpy(at + 2, &to, 8);
    at[10] = static_cast<char>(0xFF);
    at[11] = static_cast<char>(0xE0); // jmp rax
}
static int g_deletes = 0;
static void counting_delete(void* p) {
    ++g_deletes;
    fake_delete(p);
}

static char* map_game(const wchar_t* path) {
    std::ifstream in{path, std::ios::binary};
    std::vector<char> file((std::istreambuf_iterator<char>(in)), {});
    if (file.size() < 4096) return nullptr;
    auto* dos  = reinterpret_cast<IMAGE_DOS_HEADER*>(file.data());
    auto* nt   = reinterpret_cast<IMAGE_NT_HEADERS64*>(file.data() + dos->e_lfanew);
    auto* base = static_cast<char*>(VirtualAlloc(reinterpret_cast<void*>(nt->OptionalHeader.ImageBase),
                                                 nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT,
                                                 PAGE_READWRITE));
    if (!base) return nullptr;
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        std::memcpy(base + sec[i].VirtualAddress, file.data() + sec[i].PointerToRawData,
                    std::min(sec[i].SizeOfRawData, sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData));
    // The entity event queue's push (game+0x551800) does nothing here: there are no entities.
    base[0x551800] = static_cast<char>(0xC3);
    jump_to(base + 0x3229C0, reinterpret_cast<void*>(&fake_new));
    jump_to(base + 0x31C7B0, reinterpret_cast<void*>(&counting_delete));
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const DWORD c = sec[i].Characteristics;
        DWORD prot    = c & IMAGE_SCN_MEM_EXECUTE ? PAGE_EXECUTE_READ : c & IMAGE_SCN_MEM_WRITE ? PAGE_READWRITE
                                                                                                 : PAGE_READONLY;
        DWORD old = 0;
        VirtualProtect(base + sec[i].VirtualAddress, std::max(sec[i].Misc.VirtualSize, sec[i].SizeOfRawData), prot,
                       &old);
    }
    // Unwind data, as Azimuth registers it, so exceptions inside game code reach the mods' handlers.
    const auto& pdata = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    RtlAddFunctionTable(reinterpret_cast<PRUNTIME_FUNCTION>(base + pdata.VirtualAddress),
                        pdata.Size / sizeof(RUNTIME_FUNCTION), reinterpret_cast<DWORD64>(base));
    std::memcpy(base, file.data(), nt->OptionalHeader.SizeOfHeaders);
    DWORD old = 0;
    VirtualProtect(base, nt->OptionalHeader.SizeOfHeaders, PAGE_READONLY, &old);
    return base;
}

static std::string wait_for(const char* line, int seconds) {
    std::string log;
    for (int i = 0; i < seconds * 10; ++i) {
        log = slurp(L"fh2-mods\\platform.log");
        if (has(log, line)) break;
        Sleep(100);
    }
    return log;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::printf("usage: azimuth.exe <forza_durango_release_final.exe> [online]\n");
        return 2;
    }
    const bool online = argc > 2 && std::wstring(argv[2]) == L"online";
    CHECK(LoadLibraryW(L"d3dcompiler_47.dll") != nullptr);
    Sleep(500);
    char* game = map_game(argv[1]);
    CHECK(game == reinterpret_cast<char*>(0x140000000));
    if (!game) return 1;
    std::string log = wait_for(online ? "[CCBR] started for online play" : "[CCBR] ready", 20);

    if (online) {
        // Started from FH2MP's launcher: registered (it's on the MODS tab), but nothing else.
        Sleep(1000);
        log = slurp(L"fh2-mods\\platform.log");
        std::printf("---- platform.log\n%s----\n", log.c_str());
        CHECK(has(log, "[CCBR] 1.0.0 registered"));
        CHECK(has(log, "[CCBR] started for online play (FH2MP): off, since a ghost would only be one on your own "
                       "screen"));
        CHECK(!has(log, "[CCBR] hooked"));
        CHECK(!has(log, "[CCBR] added the"));
        CHECK(!fs::exists(L"FH2\\Mount\\mediaoverride\\GameTunableSettings\\collidablepresets.xml"));
        CHECK(static_cast<unsigned char>(game[0x28C590]) == 0x48); // the game's code as it was
        std::printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
        std::fflush(stdout);
        TerminateProcess(GetCurrentProcess(), fails);
        return fails;
    }

    // A fake player's car: ForzaCar's vtable, kind 0 (PLAYER_LOCAL).
    auto* car = static_cast<char*>(VirtualAlloc(nullptr, 0x6000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    *reinterpret_cast<void**>(car)        = game + 0x2C5CB80;
    *reinterpret_cast<int*>(car + 0x4D08) = 0;
    auto kind = reinterpret_cast<int (*)(void*)>(game + 0xF62280);
    CHECK(kind(car) == 0);
    auto* traffic = static_cast<char*>(VirtualAlloc(nullptr, 0x6000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    *reinterpret_cast<void**>(traffic)        = game + 0x2C5CB80;
    *reinterpret_cast<int*>(traffic + 0x4D08) = 2;
    CHECK(kind(traffic) == 2);

    // Speed camera events, through the hooked send functions (game+0x190BF90, game+0x190BFD0).
    auto send      = reinterpret_cast<void* (*)(void*, const void*)>(game + 0x190BF90);
    auto send_data = reinterpret_cast<void* (*)(void*, const void*, void*)>(game + 0x190BFD0);
    char entity[0x200]{};
    send(entity, game + 0x34BFCA0);                // ARUI_Trigger_Entered
    send(entity, game + 0x34BFB50);                // ARUI_Trigger_Exited
    send_data(entity, game + 0x34BFF20, nullptr);  // SpeedZoneActivated
    send(entity, game + 0x34BFB10);                // SpeedZoneCompleted
    send(entity, game + 0x2F60158);                // something else: not logged
    Sleep(300);
    log = slurp(L"fh2-mods\\platform.log");
    std::printf("---- platform.log\n%s----\n", log.c_str());

    CHECK(has(log, "[CCBR] 1.0.0 registered"));
    CHECK(has(log, "[CCBR] added the \"ccbr\" preset (collision off) to collidablepresets.xml"));
    const std::string presets = slurp(L"FH2\\Mount\\mediaoverride\\GameTunableSettings\\collidablepresets.xml");
    CHECK(has(presets, "<preset name=\"ccbr\" collision=\"off\" />"));
    CHECK(has(presets, "<preset name=\"tag_ghosting\" collision=\"off\" alpha=\"0.5\">")); // the game's own file
    CHECK(has(presets, "<preset name=\"intersection\" collision=\"off\"/>"));
    const std::string ini = slurp(L"fh2-mods\\CCBR.ini");
    CHECK(has(ini, "Ghost=1") && has(ini, "SeeThrough=1") && has(ini, "OthersAlpha=0.5") && has(ini, "Debug=1"));
    CHECK(has(log, "[CCBR] hooked function game+0xF62280 (ForzaCar kind (player's car))"));
    CHECK(has(log, "[CCBR] hooked function game+0xF60510"));
    CHECK(has(log, "[CCBR] hooked function game+0x190BF90 (send event)"));
    CHECK(has(log, "[CCBR] hooked function game+0x190BFD0 (send event with data)"));
    CHECK(has(log, "[CCBR] hooked function game+0x289320 (~ForzaCar (other cars going away))"));
    CHECK(has(log, "[CCBR] hooked function game+0x28C590 (car alpha (other cars see-through))"));
    CHECK(has(log, "[CCBR] hooked function game+0xF649F0 (car hide (other cars blinking out))"));
    CHECK(has(log, "[CCBR] enabled 7 hooks"));
    CHECK(static_cast<unsigned char>(game[0x28C590]) != 0x48); // the alpha function now jumps to CCBR
    CHECK(static_cast<unsigned char>(game[0xF649F0]) != 0x40); // and the hide function
    CHECK(has(log, "[CCBR] ready (6 of 6 speed camera events): ghost in speed zones on, other cars see-through "
                   "meanwhile"));
    CHECK(has(log, "[CCBR] player's car is now"));
    CHECK(has(log, "[CCBR] event ARUI_Trigger_Entered (speed camera entity"));
    CHECK(has(log, "[CCBR]   the game has no \"ccbr\" preset"));
    CHECK(has(log, "[CCBR] approaching a speed camera: no ghost, the game has no \"intersection\" preset either"));
    CHECK(has(log, "[CCBR] speed zone started: no ghost, the game has no \"intersection\" preset either"));
    CHECK(!has(log, "crashed"));
    CHECK(!has(log, "CRASH"));
    std::printf("game operator delete calls: %d\n", g_deletes);
    CHECK(g_deletes >= 4); // the four empty handles' control blocks were freed
    CHECK(has(log, "[CCBR] event ARUI_Trigger_Exited"));
    CHECK(has(log, "[CCBR] event SpeedZoneActivated"));
    CHECK(has(log, "[CCBR] event SpeedZoneCompleted"));
    CHECK(!has(log, "your car is a ghost"));
    CHECK(!has(log, "doesn't match"));
    CHECK(!has(log, "vtable slot 0x130 isn't")); // the apply function CCBR calls at the start and end
    CHECK(!has(log, "couldn't hook"));
    std::printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    std::fflush(stdout);
    TerminateProcess(GetCurrentProcess(), fails);
    return fails;
}
