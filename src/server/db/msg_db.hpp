#ifndef MSG_DB_HPP
#define MSG_DB_HPP

#include <stdint.h>

#ifdef __cplusplus
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cassert>
extern "C" {
#else
#include <string.h>
#include <assert.h>
#endif

#include "../../client/client.h"

	//--============
	// -- STRUCTS
	//--============

	typedef struct db_entry {
		char payload[INPUT_BUF_SIZE];
		uint64_t id;
		uint16_t payload_len;
		uint8_t type;
	} db_entry;

	typedef void (*msg_callback)(const db_entry* entry, void* user_data);

	//--============
	// -- msg_db
	//--============

	/// @brief Insert a payload into the database
	/// @param port: Port to push messages to
	/// @param type: Type of payload
	/// @param payload: Payload to store
	/// @param len: Payload length
	/// @return uint64_t: Message ID
	uint64_t insert_message_ex_c(uint16_t port, uint8_t type, const void *payload, uint16_t len);

	/// @brief Insert a message into the database
	/// @param port: Port to push messages to
	/// @param message: Message to store
	/// @param message_len: Message length
	/// @return uint64_t: Message ID
	uint64_t insert_message_c(int port, const char* message, uint16_t message_len);

	/// @brief Iterate through each message
	/// @param port: Port to get messages of
	/// @param cb: Callback to iterate
	/// @param user_data: Optional capture
	void for_each_message_c(int port, msg_callback cb, void* user_data);

#ifdef __cplusplus
}
#endif

#endif
