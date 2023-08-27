#pragma once

#include "game.hpp"
#include "gamepatch.hpp"

struct Parsec47 : public Game {
    static constexpr uint32_t entrypoint = 0x00420e2c;

    std::unique_ptr<GamePatch> get_patch_scale(float scale) override {
        std::vector<std::unique_ptr<GamePatch>> patches;
        patches.emplace_back(std::make_unique<MemoryPatch<uint32_t>>(0x00475a44u - entrypoint, 640, static_cast<uint32_t>(640 * scale)));
        patches.emplace_back(std::make_unique<MemoryPatch<uint32_t>>(0x00475a48u - entrypoint, 480, static_cast<uint32_t>(480 * scale)));
        return std::make_unique<MultiPatch>(std::move(patches));
    }
    std::unique_ptr<GamePatch> get_patch_lwscale(float lwscale) override {
        return std::make_unique<MemoryPatch<float>>(0x0040bc75u - entrypoint, 1.0f, lwscale);
    }
    std::unique_ptr<GamePatch> get_patch_nosmooth(bool nosmooth) override {
        return std::make_unique<MemoryPatch<uint32_t>>(0x0040bc84u - entrypoint, 0x000212fa, 0x00021300);
    }
    std::unique_ptr<GamePatch> get_patch_nobgm(bool nobgm) override {
        return std::make_unique<MemoryPatch<uint8_t>>(0x0040d9acu - entrypoint, 0x50, 0xc3);
    }
};
