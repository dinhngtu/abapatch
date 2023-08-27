#pragma once

#include "stdafx.h"

struct GameState {
    PROCESS_INFORMATION procinfo;
    uint32_t entrypoint;
};
