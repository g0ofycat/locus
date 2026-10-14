#pragma once

#include <stdio.h>
#include <stdint.h>

#include "moongoose.h"

//--============
// -- CONSTS
//--============

#define WEBPAGE_PROXY_PORT 7272
#define WEBPAGE_PROXY_TTL  600

//--============
// -- PUBLIC
//--============

/// @brief Open the webpage port
void serve_webpage(void);
