#include "key_exchange.h"
#include "../utils/net_utils.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <stdint.h>
#include <string.h>
#endif

//--============
// -- PUBLIC
//--============

/// @brief Generate keypair, send public key, receive peer public key, derive shared secret
/// @param sock: Connected socket
/// @param key_out: 32-byte derived shared secret
/// @param is_server: 1 if server, 0 if client
/// @return 0 on success, -1 on error
#ifdef _WIN32
int key_exchange(socket_t sock, uint8_t key_out[KEY_SIZE], int is_server) {
	BCRYPT_ALG_HANDLE alg = NULL;
	BCRYPT_KEY_HANDLE keypair = NULL;
	BCRYPT_KEY_HANDLE peer_key = NULL;
	BCRYPT_SECRET_HANDLE secret = NULL;

	int result = -1;

	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDH_P256_ALGORITHM, NULL, 0)))
		goto cleanup;

	if (!BCRYPT_SUCCESS(BCryptGenerateKeyPair(alg, &keypair, ECDH_KEY_BITS, 0)))
		goto cleanup;

	if (!BCRYPT_SUCCESS(BCryptFinalizeKeyPair(keypair, 0)))
		goto cleanup;

	ULONG pubkey_len = 0;
	if (!BCRYPT_SUCCESS(BCryptExportKey(keypair, NULL, BCRYPT_ECCPUBLIC_BLOB, NULL, 0, &pubkey_len, 0)))
		goto cleanup;

	uint8_t pubkey_blob[sizeof(BCRYPT_ECCKEY_BLOB) + PUBKEY_SIZE];
	if (!BCRYPT_SUCCESS(BCryptExportKey(keypair, NULL, BCRYPT_ECCPUBLIC_BLOB,
					pubkey_blob, sizeof(pubkey_blob), &pubkey_len, 0)))
		goto cleanup;

	uint8_t peer_blob[sizeof(BCRYPT_ECCKEY_BLOB) + PUBKEY_SIZE];

	if (is_server) {
		if (write_exact(sock, pubkey_blob, (int)pubkey_len) != 0) goto cleanup;
		if (read_exact(sock, peer_blob, (int)pubkey_len) != 0) goto cleanup;
	} else {
		if (read_exact(sock, peer_blob, (int)pubkey_len) != 0) goto cleanup;
		if (write_exact(sock, pubkey_blob, (int)pubkey_len) != 0) goto cleanup;
	}

	if (!BCRYPT_SUCCESS(BCryptImportKeyPair(alg, NULL, BCRYPT_ECCPUBLIC_BLOB,
					&peer_key, peer_blob, pubkey_len, 0)))
		goto cleanup;

	if (!BCRYPT_SUCCESS(BCryptSecretAgreement(keypair, peer_key, &secret, 0)))
		goto cleanup;

	BCryptBufferDesc params;
	BCryptBuffer param_buf;
	WCHAR hash_alg[] = BCRYPT_SHA256_ALGORITHM;

	param_buf.cbBuffer = sizeof(hash_alg);
	param_buf.BufferType = KDF_HASH_ALGORITHM;
	param_buf.pvBuffer = hash_alg;

	params.cBuffers = 1;
	params.pBuffers = &param_buf;
	params.ulVersion = BCRYPTBUFFER_VERSION;

	ULONG derived_len = 0;
	if (!BCRYPT_SUCCESS(BCryptDeriveKey(secret, BCRYPT_KDF_HASH, &params,
					key_out, KEY_SIZE, &derived_len, 0)))
		goto cleanup;

	result = 0;

cleanup:
	if (secret)   BCryptDestroySecret(secret);
	if (peer_key) BCryptDestroyKey(peer_key);
	if (keypair)  BCryptDestroyKey(keypair);
	if (alg)      BCryptCloseAlgorithmProvider(alg, 0);

	return result;
}
#else
#define ECDH_PUBLIC_BLOB_SIZE (2 * sizeof(uint32_t) + 2 * KEY_SIZE)

static void write_u32_le(uint8_t *dst, uint32_t value) {
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)(value >> 8);
	dst[2] = (uint8_t)(value >> 16);
	dst[3] = (uint8_t)(value >> 24);
}

static uint32_t read_u32_le(const uint8_t *src) {
	return (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
		((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

int key_exchange(socket_t sock, uint8_t key_out[KEY_SIZE], int is_server) {
	EVP_PKEY_CTX *keygen_ctx = NULL;
	EVP_PKEY_CTX *peer_ctx = NULL;
	EVP_PKEY_CTX *derive_ctx = NULL;
	EVP_PKEY *keypair = NULL;
	EVP_PKEY *peer_key = NULL;
	BIGNUM *x = NULL;
	BIGNUM *y = NULL;
	uint8_t pubkey_blob[ECDH_PUBLIC_BLOB_SIZE] = {0};
	uint8_t peer_blob[ECDH_PUBLIC_BLOB_SIZE];
	uint8_t peer_public[1 + 2 * KEY_SIZE];
	uint8_t shared[KEY_SIZE] = {0};
	size_t shared_len = sizeof(shared);
	unsigned int digest_len = 0;
	char group_name[] = "prime256v1";
	int result = -1;

	keygen_ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	if (!keygen_ctx || EVP_PKEY_keygen_init(keygen_ctx) <= 0 ||
		EVP_PKEY_CTX_set_group_name(keygen_ctx, group_name) <= 0 ||
		EVP_PKEY_generate(keygen_ctx, &keypair) <= 0)
		goto cleanup;

	if (EVP_PKEY_get_bn_param(keypair, OSSL_PKEY_PARAM_EC_PUB_X, &x) != 1 ||
		EVP_PKEY_get_bn_param(keypair, OSSL_PKEY_PARAM_EC_PUB_Y, &y) != 1)
		goto cleanup;

	write_u32_le(pubkey_blob, ECDH_MAGIC);
	write_u32_le(pubkey_blob + sizeof(uint32_t), KEY_SIZE);
	if (BN_bn2binpad(x, pubkey_blob + 2 * sizeof(uint32_t), KEY_SIZE) != KEY_SIZE ||
		BN_bn2binpad(y, pubkey_blob + 2 * sizeof(uint32_t) + KEY_SIZE, KEY_SIZE) != KEY_SIZE)
		goto cleanup;

	if (is_server) {
		if (write_exact(sock, pubkey_blob, sizeof(pubkey_blob)) != 0) goto cleanup;
		if (read_exact(sock, peer_blob, sizeof(peer_blob)) != 0) goto cleanup;
	} else {
		if (read_exact(sock, peer_blob, sizeof(peer_blob)) != 0) goto cleanup;
		if (write_exact(sock, pubkey_blob, sizeof(pubkey_blob)) != 0) goto cleanup;
	}

	if (read_u32_le(peer_blob) != ECDH_MAGIC ||
		read_u32_le(peer_blob + sizeof(uint32_t)) != KEY_SIZE)
		goto cleanup;

	peer_public[0] = 0x04;
	memcpy(peer_public + 1, peer_blob + 2 * sizeof(uint32_t), 2 * KEY_SIZE);
	OSSL_PARAM peer_params[] = {
		OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, group_name, 0),
		OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, peer_public, sizeof(peer_public)),
		OSSL_PARAM_construct_end()
	};
	peer_ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	if (!peer_ctx || EVP_PKEY_fromdata_init(peer_ctx) <= 0 ||
		EVP_PKEY_fromdata(peer_ctx, &peer_key, EVP_PKEY_PUBLIC_KEY, peer_params) <= 0)
		goto cleanup;

	derive_ctx = EVP_PKEY_CTX_new(keypair, NULL);
	if (!derive_ctx || EVP_PKEY_derive_init(derive_ctx) <= 0 ||
		EVP_PKEY_derive_set_peer(derive_ctx, peer_key) <= 0 ||
		EVP_PKEY_derive(derive_ctx, shared, &shared_len) <= 0 || shared_len != KEY_SIZE)
		goto cleanup;
	if (EVP_Digest(shared, shared_len, key_out, &digest_len, EVP_sha256(), NULL) != 1 ||
		digest_len != KEY_SIZE)
		goto cleanup;
	result = 0;

cleanup:
	EVP_PKEY_CTX_free(derive_ctx);
	EVP_PKEY_CTX_free(peer_ctx);
	EVP_PKEY_CTX_free(keygen_ctx);
	EVP_PKEY_free(peer_key);
	EVP_PKEY_free(keypair);
	BN_free(x);
	BN_free(y);
	OPENSSL_cleanse(shared, sizeof(shared));
	return result;
}
#endif
