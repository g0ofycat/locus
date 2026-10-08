#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#include "encrypt.h"

//--============
// -- PRIVATE
//--============

/// @brief Get or initialize the AES-GCM algorithm handle (lazy, cached)
/// @return BCRYPT_ALG_HANDLE on success, NULL on failure
#ifdef _WIN32
static BCRYPT_ALG_HANDLE get_alg(void) {
	static BCRYPT_ALG_HANDLE alg = NULL;
	if (alg) return alg;

	BCRYPT_ALG_HANDLE h = NULL;
	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&h, BCRYPT_AES_ALGORITHM, NULL, 0)))
		return NULL;
	if (!BCRYPT_SUCCESS(BCryptSetProperty(h, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0))) {
		BCryptCloseAlgorithmProvider(h, 0);
		return NULL;
	}

	alg = h;

	return alg;
}

//--============
// -- PUBLIC
//--============

/// @brief Encrypt data using AES-256-GCM via Windows BCrypt
/// @param key: 32-byte encryption key
/// @param src: Plaintext input
/// @param srcSize: Size of plaintext
/// @param dst: Destination buffer (must be srcSize + ENCRYPT_OVERHEAD bytes)
/// @param dstCapacity: Size of destination buffer
/// @return size_t: Size of encrypted output (srcSize + ENCRYPT_OVERHEAD), 0 on error
size_t encrypt_data(const uint8_t *key, const void *src, size_t srcSize, void *dst, size_t dstCapacity) {
	if (!key || !src || !dst) return 0;
	if (dstCapacity < srcSize + ENCRYPT_OVERHEAD) return 0;

	uint8_t *iv = (uint8_t *)dst;
	uint8_t *tag = iv + AES_IV_SIZE;
	uint8_t *ct = tag + AES_TAG_SIZE;

	if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, iv, AES_IV_SIZE, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
		return 0;

	BCRYPT_KEY_HANDLE hkey = NULL;
	if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(get_alg(), &hkey, NULL, 0, (PUCHAR)key, AES_KEY_SIZE, 0)))
		return 0;

	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
	BCRYPT_INIT_AUTH_MODE_INFO(info);
	info.pbNonce = iv;
	info.cbNonce = AES_IV_SIZE;
	info.pbTag = tag;
	info.cbTag = AES_TAG_SIZE;

	ULONG out_len = 0;
	NTSTATUS st = BCryptEncrypt(hkey, (PUCHAR)src, (ULONG)srcSize, &info, NULL, 0, ct, (ULONG)srcSize, &out_len, 0);
	BCryptDestroyKey(hkey);

	return BCRYPT_SUCCESS(st) ? out_len + ENCRYPT_OVERHEAD : 0;
}

/// @brief Decrypt data using AES-256-GCM via Windows BCrypt
/// @param key: 32-byte encryption key
/// @param src: Encrypted input (with IV and TAG prefix)
/// @param srcSize: Size of encrypted input
/// @param dst: Destination buffer (must be at least srcSize - ENCRYPT_OVERHEAD bytes)
/// @param dstCapacity: Size of destination buffer
/// @return size_t: Size of decrypted output, 0 on error or auth failure
size_t decrypt_data(const uint8_t *key, const void *src, size_t srcSize, void *dst, size_t dstCapacity) {
	if (!key || !src || !dst) return 0;
	if (srcSize <= ENCRYPT_OVERHEAD) return 0;

	const uint8_t *iv  = (const uint8_t *)src;
	const uint8_t *tag = iv + AES_IV_SIZE;
	const uint8_t *ct  = tag + AES_TAG_SIZE;
	size_t ct_len = srcSize - ENCRYPT_OVERHEAD;

	if (dstCapacity < ct_len) return 0;

	BCRYPT_KEY_HANDLE hkey = NULL;
	if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(get_alg(), &hkey, NULL, 0, (PUCHAR)key, AES_KEY_SIZE, 0)))
		return 0;

	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
	BCRYPT_INIT_AUTH_MODE_INFO(info);
	info.pbNonce = (PUCHAR)iv;
	info.cbNonce = AES_IV_SIZE;
	info.pbTag = (PUCHAR)tag;
	info.cbTag = AES_TAG_SIZE;

	ULONG out_len = 0;
	NTSTATUS st = BCryptDecrypt(hkey, (PUCHAR)ct, (ULONG)ct_len, &info, NULL, 0, dst, (ULONG)dstCapacity, &out_len, 0);
	BCryptDestroyKey(hkey);

	return BCRYPT_SUCCESS(st) ? out_len : 0;
}
#else
size_t encrypt_data(const uint8_t *key, const void *src, size_t srcSize, void *dst, size_t dstCapacity) {
	if (!key || !src || !dst || srcSize > INT_MAX) return 0;
	if (dstCapacity < srcSize + ENCRYPT_OVERHEAD) return 0;

	uint8_t *iv = dst;
	uint8_t *tag = iv + AES_IV_SIZE;
	uint8_t *ciphertext = tag + AES_TAG_SIZE;
	if (RAND_bytes(iv, AES_IV_SIZE) != 1) return 0;

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (!ctx) return 0;

	int output_len = 0;
	int ciphertext_len = 0;
	size_t result = 0;
	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
		goto cleanup;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_IV_SIZE, NULL) != 1)
		goto cleanup;
	if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1)
		goto cleanup;
	if (EVP_EncryptUpdate(ctx, ciphertext, &output_len, src, (int)srcSize) != 1)
		goto cleanup;
	ciphertext_len = output_len;
	if (EVP_EncryptFinal_ex(ctx, ciphertext + ciphertext_len, &output_len) != 1)
		goto cleanup;
	ciphertext_len += output_len;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_TAG_SIZE, tag) != 1)
		goto cleanup;
	result = (size_t)ciphertext_len + ENCRYPT_OVERHEAD;

cleanup:
	EVP_CIPHER_CTX_free(ctx);
	return result;
}

size_t decrypt_data(const uint8_t *key, const void *src, size_t srcSize, void *dst, size_t dstCapacity) {
	if (!key || !src || !dst || srcSize <= ENCRYPT_OVERHEAD) return 0;

	const uint8_t *iv = src;
	const uint8_t *tag = iv + AES_IV_SIZE;
	const uint8_t *ciphertext = tag + AES_TAG_SIZE;
	size_t ciphertext_len = srcSize - ENCRYPT_OVERHEAD;
	if (ciphertext_len > INT_MAX || dstCapacity < ciphertext_len) return 0;

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (!ctx) return 0;

	int output_len = 0;
	int plaintext_len = 0;
	size_t result = 0;
	if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
		goto cleanup;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, AES_IV_SIZE, NULL) != 1)
		goto cleanup;
	if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1)
		goto cleanup;
	if (EVP_DecryptUpdate(ctx, dst, &output_len, ciphertext, (int)ciphertext_len) != 1)
		goto cleanup;
	plaintext_len = output_len;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_TAG_SIZE, (void *)tag) != 1)
		goto cleanup;
	if (EVP_DecryptFinal_ex(ctx, (uint8_t *)dst + plaintext_len, &output_len) != 1)
		goto cleanup;
	plaintext_len += output_len;
	result = (size_t)plaintext_len;

cleanup:
	EVP_CIPHER_CTX_free(ctx);
	return result;
}
#endif
