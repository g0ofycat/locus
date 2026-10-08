#include "key_exchange.h"
#include "../utils/net_utils.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>
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
	EC_KEY *keypair = NULL;
	EC_POINT *peer_point = NULL;
	BIGNUM *x = NULL;
	BIGNUM *y = NULL;
	uint8_t pubkey_blob[ECDH_PUBLIC_BLOB_SIZE] = {0};
	uint8_t peer_blob[ECDH_PUBLIC_BLOB_SIZE];
	uint8_t shared[KEY_SIZE];
	int result = -1;

	keypair = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if (!keypair || EC_KEY_generate_key(keypair) != 1)
		goto cleanup;

	const EC_GROUP *group = EC_KEY_get0_group(keypair);
	const EC_POINT *public_key = EC_KEY_get0_public_key(keypair);
	x = BN_new();
	y = BN_new();
	if (!x || !y || EC_POINT_get_affine_coordinates(group, public_key, x, y, NULL) != 1)
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

	BN_free(x);
	x = BN_bin2bn(peer_blob + 2 * sizeof(uint32_t), KEY_SIZE, NULL);
	BN_free(y);
	y = BN_bin2bn(peer_blob + 2 * sizeof(uint32_t) + KEY_SIZE, KEY_SIZE, NULL);
	peer_point = EC_POINT_new(group);
	if (!x || !y || !peer_point ||
		EC_POINT_set_affine_coordinates(group, peer_point, x, y, NULL) != 1 ||
		EC_POINT_is_on_curve(group, peer_point, NULL) != 1)
		goto cleanup;

	if (ECDH_compute_key(shared, sizeof(shared), peer_point, keypair, NULL) != KEY_SIZE)
		goto cleanup;
	if (!SHA256(shared, sizeof(shared), key_out))
		goto cleanup;
	result = 0;

cleanup:
	EC_POINT_free(peer_point);
	BN_free(x);
	BN_free(y);
	EC_KEY_free(keypair);
	OPENSSL_cleanse(shared, sizeof(shared));
	return result;
}
#endif
