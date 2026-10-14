#include "webpage_proxy.h"

//--============
// -- PRIVATE
//--============

static int file_id = 0;

/// @brief Serve webpage helper
/// @param *c
/// @param ev
/// @param *ev_data
static void handle_request(struct mg_connection *c, int ev, void *ev_data) {
	if (ev != MG_EV_HTTP_MSG)
		return;

	struct mg_http_message *hm = ev_data;
	struct mg_http_serve_opts opts = { .root_dir = "./webpage" };

	mg_http_serve_dir(c, hm, &opts);
}

//--============
// -- PUBLIC
//--============

/// @brief Open the webpage port
void serve_webpage(void) {
	struct mg_mgr mgr;
	mg_mgr_init(&mgr);

	char proxy[64];
	snprintf(proxy, sizeof proxy, "http://0.0.0.0:%d/file/%d", WEBPAGE_PROXY_PORT, file_id);

	mg_http_listen(&mgr, proxy, handle_request, NULL);

	file_id++;

	uint64_t deadline = mg_millis() + (uint64_t) WEBPAGE_PROXY_TTL * 1000;

	while (mg_millis() < deadline) {
		uint64_t remaining = deadline - mg_millis();
		int timeout_ms = remaining > 1000 ? 1000 : (int) remaining;
		mg_mgr_poll(&mgr, timeout_ms);
	}
}
