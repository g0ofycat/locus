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

static int active_port = WEBPAGE_PROXY_PORT;
static struct mg_mgr mgr;
static struct shared_file shared_files[MAX_SHARED_FILES];
static uint32_t next_id;
static bool initialized;

#ifdef _WIN32
static CRITICAL_SECTION shared_files_mutex;
#else
static pthread_mutex_t shared_files_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void shared_files_lock(void)
{
#ifdef _WIN32
	EnterCriticalSection(&shared_files_mutex);
#else
	pthread_mutex_lock(&shared_files_mutex);
#endif
}

static void shared_files_unlock(void)
{
#ifdef _WIN32
	LeaveCriticalSection(&shared_files_mutex);
#else
	pthread_mutex_unlock(&shared_files_mutex);
#endif
}

static void expire_files(void)
{
	uint64_t now = mg_millis();

	for (size_t i = 0; i < MAX_SHARED_FILES; i++)
	{
		struct shared_file *file = &shared_files[i];

		if (file->active && now >= file->expires_at)
		{
			free(file->path);
			file->path = NULL;
			file->active = false;
		}
	}
}

static bool id_is_active(uint32_t id) {
	for (size_t i = 0; i < MAX_SHARED_FILES; i++)
		if (shared_files[i].active && shared_files[i].id == id)
			return true;

	return false;
}

static uint32_t allocate_id(void) {
	for (;;) {
		uint32_t id = next_id++;

		if (!id_is_active(id))
			return id;
	}
}

static void sanitize_content_disposition_filename(char *dst, size_t dst_size, const char *src)
{
	size_t j = 0;
	if (dst_size == 0)
		return;
	for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
		unsigned char ch = (unsigned char)src[i];
		if (ch < 0x20 || ch == 0x7F || ch == '"' || ch == '\\')
			continue;
		dst[j++] = (char)ch;
	}
	if (j == 0) {
		const char *fallback = "sanitized_file";
		while (*fallback && j + 1 < dst_size)
			dst[j++] = *fallback++;
	}
	dst[j] = '\0';
}

static void handle_request(struct mg_connection *c, int ev, void *ev_data) {
	if (ev != MG_EV_HTTP_MSG) return;

	struct mg_http_message *hm = ev_data;

	if (mg_match(hm->uri, mg_str("/files"), NULL))
	{
		char id_text[32];
		int n = mg_http_get_var(&hm->query, "id", id_text, sizeof id_text);

		if (n <= 0 || (size_t)n >= sizeof id_text)
		{
			mg_http_reply(c, 400, "Content-Type: text/plain\r\n",
					"Missing or invalid id\n");
			return;
		}

		shared_files_lock();

		expire_files();

		for (size_t i = 0; i < MAX_SHARED_FILES; i++)
		{
			struct shared_file *file = &shared_files[i];
			char expected_id[32];

			if (!file->active)
				continue;

			snprintf(expected_id, sizeof expected_id, "%u", file->id);

			if (strcmp(id_text, expected_id) == 0)
			{
				const char *filename = strrchr(file->path, '/');
				if (!filename) {
					filename = strrchr(file->path, '\\');
				}
				filename = filename ? filename + 1 : file->path;

				char safe_name[256];
				sanitize_content_disposition_filename(safe_name, sizeof(safe_name), filename);

				char header_buf[512];
				snprintf(header_buf, sizeof(header_buf),
						"Content-Disposition: attachment; filename=\"%s\"\r\n",
						safe_name);

				struct mg_http_serve_opts opts = {
					.extra_headers = header_buf
				};

				mg_http_serve_file(c, hm, file->path, &opts);

				shared_files_unlock();
				return;
			}
		}

		shared_files_unlock();

		mg_http_reply(c, 404, "Content-Type: text/plain\r\n",
				"File not found or link expired\n");
		return;
	}

	char cwd[1024];
	static char full_webpage_path[1200] = {0};

	if (full_webpage_path[0] == '\0' && getcwd(cwd, sizeof(cwd)) != NULL) {
		snprintf(full_webpage_path, sizeof(full_webpage_path), "%s/src/protocol/files/webpage", cwd);
	}

	struct mg_http_serve_opts opts = {
		.root_dir = full_webpage_path[0] != '\0' ? full_webpage_path : "/src/protocol/files/webpage"
	};

	mg_http_serve_dir(c, hm, &opts);
}

//--============
// -- PUBLIC
//--============

int webpage_proxy_init(void) {
	if (initialized) return 1;

#ifdef _WIN32
	InitializeCriticalSection(&shared_files_mutex);
#endif

	mg_log_set(MG_LL_NONE);
	mg_mgr_init(&mgr);

	char listen_url[64];
	struct mg_connection *c = NULL;

	for (int i = 0; i < 1024; i++) {
		active_port = WEBPAGE_PROXY_PORT + i;
		snprintf(listen_url, sizeof(listen_url), "http://0.0.0.0:%d", active_port);

		c = mg_http_listen(&mgr, listen_url, handle_request, NULL);
		if (c != NULL)
			break;
	}

	if (c == NULL)
	{
		mg_mgr_free(&mgr);

#ifdef _WIN32
		DeleteCriticalSection(&shared_files_mutex);
#endif

		return 0;
	}

	initialized = true;
	return 1;
}

int webpage_proxy_get_port(void) {
	return active_port;
}

int serve_webpage(const char *file_path, uint32_t *id_out)
{
	if (!initialized || file_path == NULL || id_out == NULL)
		return 0;

	struct stat st;
	if (stat(file_path, &st) != 0 || !S_ISREG(st.st_mode))
		return 0;

	shared_files_lock();

	expire_files();

	size_t slot = MAX_SHARED_FILES;

	for (size_t i = 0; i < MAX_SHARED_FILES; i++)
	{
		if (!shared_files[i].active)
		{
			slot = i;
			break;
		}
	}

	if (slot == MAX_SHARED_FILES)
	{
		shared_files_unlock();
		return 0;
	}

	size_t path_len = strlen(file_path) + 1;
	char *path_copy = malloc(path_len);

	if (path_copy == NULL)
	{
		shared_files_unlock();
		return 0;
	}

	memcpy(path_copy, file_path, path_len);

	shared_files[slot].id = allocate_id();
	shared_files[slot].path = path_copy;
	shared_files[slot].expires_at = mg_millis() + (uint64_t)WEBPAGE_PROXY_TTL * 1000;
	shared_files[slot].active = true;

	*id_out = shared_files[slot].id;

	shared_files_unlock();

	return 1;
}

void webpage_proxy_poll(void)
{
	if (!initialized)
		return;

	mg_mgr_poll(&mgr, WEBPAGE_PROXY_POLL_INTERVAL);

	shared_files_lock();
	expire_files();
	shared_files_unlock();
}

void webpage_proxy_free(void)
{
	if (!initialized)
		return;

	shared_files_lock();

	for (size_t i = 0; i < MAX_SHARED_FILES; i++)
	{
		free(shared_files[i].path);
		shared_files[i].path = NULL;
		shared_files[i].active = false;
	}

	shared_files_unlock();

	mg_mgr_free(&mgr);
	initialized = false;

#ifdef _WIN32
	DeleteCriticalSection(&shared_files_mutex);
#endif
}

