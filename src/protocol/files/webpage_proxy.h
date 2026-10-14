#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mongoose.h"

//--============
// -- CONSTS
//--============

#define MAX_SHARED_FILES             1024
#define WEBPAGE_PROXY_PORT           7272
#define WEBPAGE_PROXY_TTL            600
#define WEBPAGE_PROXY_POLL_INTERVAL  50

//--============
// -- PUBLIC
//--============

/// @brief Initialize the webpage proxy
/// @return success bool
int webpage_proxy_init(void);

/// @brief Serve the webpage
/// @param *file_path
/// @param *id_out
/// @return success bool
int serve_webpage(const char *file_path, uint32_t *id_out);

/// @brief Poll the Proxy for WEBPAGE_PROXY_TTL
void webpage_proxy_poll();

/// @brief Mem Free Webpage Proxy
void webpage_proxy_free(void);
