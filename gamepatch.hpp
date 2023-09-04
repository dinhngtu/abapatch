#pragma once

#include "stdafx.h"
#include <vector>
#include <system_error>
#include "gamestate.hpp"

template <typename T>
static bool proc_cmpxchg(HANDLE hProcess, LPVOID lpBaseAddress, T expected, T desired) {
    T buf{};
    if (!ReadProcessMemory(hProcess, lpBaseAddress, &buf, sizeof(T), NULL)) {
        throw std::system_error(GetLastError(), std::system_category(), "ReadProcessMemory");
    }
    if (buf != expected) {
        return false;
    }
    buf = desired;
    if (!WriteProcessMemory(hProcess, lpBaseAddress, &buf, sizeof(T), NULL)) {
        throw std::system_error(GetLastError(), std::system_category(), "WriteProcessMemory");
    }
    return true;
}

template <typename T>
class MemoryPatch : public GamePatch {
public:
    explicit MemoryPatch(uint32_t epoff, T expected, T desired) :
        _epoff(epoff),
        _expected(expected),
        _desired(desired) {}

    void apply(GameState& lg) override {
        if (!proc_cmpxchg(lg.procinfo.hProcess, reinterpret_cast<LPVOID>(lg.entrypoint + _epoff), _expected, _desired)) {
            throw std::runtime_error("patch failed");
        }
    }

private:
    uint32_t _epoff;
    T _expected;
    T _desired;
};

class MultiPatch : public GamePatch {
public:
    explicit MultiPatch(std::vector<std::unique_ptr<GamePatch>> patches) : _patches(std::move(patches)) {}

    void apply(GameState& lg) override {
        for (auto& patch : _patches) {
            patch->apply(lg);
        }
    }

private:
    std::vector<std::unique_ptr<GamePatch>> _patches;
};

class PriorityPatch : public GamePatch {
public:
    explicit PriorityPatch(DWORD priorityClass) : _priorityClass(priorityClass) {}

    void apply(GameState& lg) override {
        if (!SetPriorityClass(lg.procinfo.hProcess, _priorityClass)) {
            throw std::system_error(GetLastError(), std::system_category(), "SetPriorityClass");
        }
    }

private:
    DWORD _priorityClass;
};
