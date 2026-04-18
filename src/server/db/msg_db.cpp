#include "msg_db.hpp"

//--================
// -- PRIVATE DATA
//--================

static std::unordered_map<int, std::vector<db_entry>> port_messages; // [port]: {entry}
static uint64_t msg_idx = 0;

//--============
// -- msg_db
//--============

/// @brief Insert a payload into the database
/// @param port: Port to push messages to
/// @param type: Type of payload
/// @param payload: Payload to store
/// @param len: Payload length
/// @return uint64_t: Message ID
extern "C" uint64_t insert_message_ex_c(uint16_t port, uint8_t type, const void *payload, uint16_t len) {
	assert(len <= INPUT_BUF_SIZE);

	db_entry entry{};

	entry.id = msg_idx++;
	entry.type = type;
	entry.payload_len = len;

	std::memcpy(entry.payload, payload, len);

	port_messages[port].push_back(entry);

	return entry.id;
}

/// @brief Insert a message into the database
/// @param port: Port to push messages to
/// @param message: Message to store
/// @param message_len: Message length
/// @return uint64_t: Message ID
extern "C" uint64_t insert_message_c(int port, const char* message, uint16_t message_len) {
	return insert_message_ex_c(port, MSG_CHAT, message, message_len);
}

/// @brief Iterate through each message
/// @param port: Port to get messages of
/// @param cb: Callback to iterate
/// @param user_data: Optional capture
extern "C" void for_each_message_c(int port, msg_callback cb, void* user_data) {
	if (!cb) return;

	auto it = port_messages.find(port);
	if (it != port_messages.end()) {
		for (const auto& entry : it->second) {
			cb(&entry, user_data);
		}
	}
}
