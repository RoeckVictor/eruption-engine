#pragma once
#include "engine/core/Log.h"

#define GAME_LOG(fmt, ...) LOG_MSG("Game", fmt, ##__VA_ARGS__)
#define GAME_ERR(fmt, ...) LOG_ERR("Game", fmt, ##__VA_ARGS__)
