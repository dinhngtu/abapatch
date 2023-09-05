#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdlib>
#include <vector>
#include <string>
#include "detours.h"

struct Mix_Music;

typedef Mix_Music* (__cdecl* Mix_LoadMUSFunc)(const char* file);
typedef int(__cdecl* Mix_PlayMusicFunc)(Mix_Music* music, int loops);
typedef void(__cdecl* Mix_FreeMusicFunc)(Mix_Music* music);

static Mix_LoadMUSFunc loadmusic = NULL;
static Mix_PlayMusicFunc playmusic = NULL;
static std::vector<Mix_Music*> musics;

static void find_musics() {
    WIN32_FIND_DATAA f;
    auto hf = FindFirstFileA("allmusics\\*.ogg", &f);
    if (hf == INVALID_HANDLE_VALUE)
        return;
    do {
        //MessageBoxA(NULL, f.cFileName, "", MB_OK);
        auto path = std::string("allmusics\\") + f.cFileName;
        auto mus = loadmusic(path.c_str());
        if (mus)
            musics.push_back(mus);
    } while (FindNextFileA(hf, &f));
    FindClose(hf);
}

static void free_musics() {
    auto freemusic = static_cast<Mix_FreeMusicFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_FreeMusic"));
    if (!freemusic)
        return;
    for (auto mus : musics)
        freemusic(mus);
}

int my_Mix_PlayMusic(Mix_Music* music, int loops) {
    if (musics.size()) {
        return playmusic(musics[rand() % musics.size()], loops);
    }
    else {
        return playmusic(music, loops);
    }
}

Mix_Music* my_Mix_LoadMUS(const char* file) {
    static bool inited = false;
    if (!inited) {
        find_musics();
        inited = true;
    }
    return loadmusic(file);
}

static void install(bool value) {
    loadmusic = static_cast<Mix_LoadMUSFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_LoadMUS"));
    if (!loadmusic)
        return;
    playmusic = static_cast<Mix_PlayMusicFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_PlayMusic"));
    if (!playmusic)
        return;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (value) {
        DetourAttach(&loadmusic, my_Mix_LoadMUS);
        DetourAttach(&playmusic, my_Mix_PlayMusic);
    }
    else {
        DetourDetach(&playmusic, my_Mix_PlayMusic);
        DetourDetach(&loadmusic, my_Mix_LoadMUS);
    }
    DetourTransactionCommit();
    if (!value)
        free_musics();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        install(true);
        break;
    case DLL_PROCESS_DETACH:
        install(false);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

