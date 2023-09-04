#pragma once

#include "stdafx.h"
#include <stdexcept>
#include <memory>

struct GameState;

struct GamePatch {
    virtual void apply(GameState&) = 0;
};

struct Game {
    virtual std::unique_ptr<GamePatch> get_patch_scale(float scale) {
        throw std::logic_error("unsupported patch");
    }
    virtual std::unique_ptr<GamePatch> get_patch_lwscale(float linewidth) {
        throw std::logic_error("unsupported patch");
    }
    virtual std::unique_ptr<GamePatch> get_patch_nosmooth(bool nosmooth) {
        throw std::logic_error("unsupported patch");
    }
    virtual std::unique_ptr<GamePatch> get_patch_nobgm(bool nobgm) {
        throw std::logic_error("unsupported patch");
    }
    virtual std::unique_ptr<GamePatch> get_patch_priority(DWORD priorityClass);
    virtual std::unique_ptr<GamePatch> get_patch_inject();
};
