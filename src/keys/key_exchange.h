#pragma once

#include <stdint.h>
#include <winsock2.h>

//--============
// -- CONSTS
//--============

#define KEY_SIZE        32
#define PUBKEY_SIZE     65          // uncompressed P-256 point
#define ECDH_MAGIC      0x314B4345  // ECK1

//--============
// -- API
//--============

/// @brief Generate keypair, send public key, receive peer public key, derive shared secret
/// @param sock: Connected socket
/// @param key_out: 32-byte derived shared secret
/// @param is_server: 1 if server, 0 if client
/// @return 0 on success, -1 on error
int key_exchange(SOCKET sock, uint8_t key_out[KEY_SIZE], int is_server);
