#include "webpage_proxy.h"

//--============
// -- PRIVATE
//--============

struct shared_file {
	uint32_t id;
	char *path;
	uint64_t expires_at;
	bool active;
};

static struct mg_mgr mgr;
static struct shared_file shared_files[MAX_SHARED_FILES];
static uint32_t next_id;
static bool initialized;

static void expire_files(void) {
	uint64_t now = mg_millis();

	for (size_t i = 0; i < MAX_SHARED_FILES; i++) {
		struct shared_file *file = &shared_files[i];

		if (file->active && now >= file->expires_at) {
			free(file->path);
			file->path = NULL;
			file->active = false;
		}
	}
}

static bool id_is_active(uint32_t id) {
	for (size_t i = 0; i < MAX_SHARED_FILES; i++) {
		if (shared_files[i].active && shared_files[i].id == id) {
			return true;
		}
	}
	return false;
}

static uint32_t allocate_id(void) {
	for (;;) {
		uint32_t id = next_id++;

		if (!id_is_active(id)) {
			return id;
		}
	}
}

static void handle_request(struct mg_connection *c, int ev, void *ev_data) {
	if (ev != MG_EV_HTTP_MSG) return;

	struct mg_http_message *hm = ev_data;

	if (mg_match(hm->uri, mg_str("/files"), NULL)) {
		expire_files();

		char id_text[32];
		int n = mg_http_get_var(&hm->query, "id", id_text, sizeof id_text);

		if (n <= 0 || (size_t) n >= sizeof id_text) {
			mg_http_reply(c, 400, "Content-Type: text/plain\r\n",
					"Missing or invalid id\n");
			return;
		}

		for (size_t i = 0; i < MAX_SHARED_FILES; i++) {
			struct shared_file *file = &shared_files[i];
			char expected_id[32];

			if (!file->active) continue;

			snprintf(expected_id, sizeof expected_id, "%u", file->id);

			if (strcmp(id_text, expected_id) == 0) {
				struct mg_http_serve_opts opts = {
					.extra_headers = "Content-Disposition: attachment\r\n"
				};

				mg_http_serve_file(c, hm, file->path, &opts);
				return;
			}
		}

		mg_http_reply(c, 404, "Content-Type: text/plain\r\n",
				"File not found or link expired\n");
		return;
	}

	struct mg_http_serve_opts opts = {
		.root_dir = "./webpage"
	};
	mg_http_serve_dir(c, hm, &opts);
}

//--============
// -- PUBLIC
//--============

int webpage_proxy_init(void) {
	if (initialized) return 1;

	mg_mgr_init(&mgr);

	char listen_url[64];
	int n = snprintf(listen_url, sizeof listen_url,
			"http://0.0.0.0:%d", WEBPAGE_PROXY_PORT);

	if (n < 0 || (size_t) n >= sizeof listen_url || mg_http_listen(&mgr, listen_url, handle_request, NULL) == NULL) {
		mg_mgr_free(&mgr);
		return 0;
	}

	initialized = true;
	return 1;
}

int serve_webpage(const char *file_path, uint32_t *id_out) {
	if (!initialized || file_path == NULL || id_out == NULL) return 0;

	expire_files();

	size_t slot = MAX_SHARED_FILES;
	for (size_t i = 0; i < MAX_SHARED_FILES; i++) {
		if (!shared_files[i].active) {
			slot = i;
			break;
		}
	}

	if (slot == MAX_SHARED_FILES) return 0;

	size_t path_len = strlen(file_path) + 1;
	char *path_copy = malloc(path_len);
	if (path_copy == NULL) return 0;

	memcpy(path_copy, file_path, path_len);

	shared_files[slot].id = allocate_id();
	shared_files[slot].path = path_copy;
	shared_files[slot].expires_at = mg_millis() + (uint64_t) WEBPAGE_PROXY_TTL * 1000;
	shared_files[slot].active = true;

	*id_out = shared_files[slot].id;
	return 1;
}

void webpage_proxy_poll() {
	if (!initialized) return;

	mg_mgr_poll(&mgr, WEBPAGE_PROXY_POLL_INTERVAL);
	expire_files();
}

void webpage_proxy_free(void) {
	if (!initialized) return;

	for (size_t i = 0; i < MAX_SHARED_FILES; i++) {
		free(shared_files[i].path);
		shared_files[i].path = NULL;
		shared_files[i].active = false;
	}

	mg_mgr_free(&mgr);
	initialized = false;
}

