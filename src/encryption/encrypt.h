#pragma once

#include <stdint.h>
#include <stddef.h>

//--============
// -- CONSTS
//--============

#define ENCRYPT_OVERHEAD 28	// AES_IV_SIZE + AES_TAG_SIZE

#define AES_KEY_SIZE 32
#define AES_IV_SIZE 12
#define AES_TAG_SIZE 16

//--============
// -- API
//--============

/// @brief Encrypt data using AES-256-GCM
/// @param key: 32-byte encryption key
/// @param src: Plaintext input
/// @param srcSize: Size of plaintext
/// @param dst: Destination buffer (must be srcSize + ENCRYPT_OVERHEAD bytes)
/// @param dstCapacity: Size of destination buffer
/// @return size_t: Size of encrypted output (srcSize + ENCRYPT_OVERHEAD), 0 on error
size_t encrypt_data(const uint8_t *key, const void *src, size_t srcSize, void *dst, size_t dstCapacity);

/// @brief Decrypt data using AES-256-GCM
/// @param key: 32-byte encryption key
/// @param src: Encrypted input (with IV and TAG prefix)
/// @param srcSize: Size of encrypted input
/// @param dst: Destination buffer (must be srcSize - ENCRYPT_OVERHEAD bytes)
/// @param dstCapacity: Size of destination buffer
/// @return size_t: Size of decrypted output, 0 on error or auth failure
size_t decrypt_data(const uint8_t *key, const void *src, size_t srcSize, void *dst, size_t dstCapacity);
