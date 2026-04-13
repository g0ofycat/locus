#include "key_exchange.h"
#include "../utils/net_utils.h"

#include <windows.h>
#include <bcrypt.h>

//--============
// -- API
//--============

/// @brief Generate keypair, send public key, receive peer public key, derive shared secret
/// @param sock: Connected socket
/// @param key_out: 32-byte derived shared secret
/// @param is_server: 1 if server, 0 if client
/// @return 0 on success, -1 on error
int key_exchange(SOCKET sock, uint8_t key_out[KEY_SIZE], int is_server) {
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
