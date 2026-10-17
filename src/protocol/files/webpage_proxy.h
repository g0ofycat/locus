#pragma once

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <pthread.h>
#include <openssl/rand.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "mongoose.h"

//--============
// -- CONSTS
//--============

#define MAX_SHARED_FILES             1024
#define WEBPAGE_PROXY_PORT           7272
#define WEBPAGE_PROXY_TTL            600
#define WEBPAGE_PROXY_POLL_INTERVAL  50
#define WEBPAGE_NONCE_SIZE           32

//--============
// -- PUBLIC
//--============

/// @brief Initialize the webpage proxy
/// @return success bool
int webpage_proxy_init(void);

/// @brief Get the active port the webpage proxy is listening on
/// @return port number
int webpage_proxy_get_port(void);

/// @brief Serve the webpage
/// @param *file_path
/// @param *id_out
/// @param *nonce_out
/// @param nonce_out_len
/// @return success bool
int serve_webpage(const char *file_path, uint32_t *id_out, char *nonce_out, size_t nonce_out_len);

/// @brief Poll the webpage proxy for incoming HTTP requests
void webpage_proxy_poll();

/// @brief Free all resources used by the webpage proxy
void webpage_proxy_free(void);
