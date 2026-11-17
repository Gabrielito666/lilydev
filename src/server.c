#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_RESOURCES 32
#define MAX_WS_CLIENTS 64
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static void handle_request(Server* server, int client);
static void http_respond(int client, char* status, char* content_type, void* buffer, size_t size);

typedef struct {
	char* path;
	char* content_type;
	void* buffer;
	size_t size;
} Resource;

struct Server {
	int listen_fd;
	Resource resources[MAX_RESOURCES];
	int resource_count;
	int ws_clients[MAX_WS_CLIENTS];
	int ws_count;
};

static uint32_t rol(uint32_t x, int c)
{
	return (x << c) | (x >> (32 - c));
}

static void sha1_raw(void* data, size_t len, unsigned char out[20])
{
	uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };

	size_t total = len + 9;
	if (total % 64) total += 64 - total % 64;

	unsigned char* msg = calloc(1, total);
	memcpy(msg, data, len);
	msg[len] = 0x80;
	uint64_t bits = (uint64_t)len * 8;
	for (int i = 0; i < 8; i++)
		msg[total - 1 - i] = bits >> (8 * i);

	for (size_t off = 0; off < total; off += 64)
	{
		uint32_t w[80];
		for (int i = 0; i < 16; i++)
			w[i] = (msg[off + i * 4u] << 24) | (msg[off + i * 4u + 1] << 16)
				 | (msg[off + i * 4u + 2] << 8) | msg[off + i * 4u + 3];
		for (int i = 16; i < 80; i++)
			w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
		for (int i = 0; i < 80; i++)
		{
			uint32_t f, k;
			if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
			else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
			else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
			else { f = b ^ c ^ d; k = 0xCA62C1D6; }
			uint32_t t = rol(a, 5) + f + e + k + w[i];
			e = d; d = c; c = rol(b, 30); b = a; a = t;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
	}
	free(msg);

	for (int i = 0; i < 5; i++)
	{
		out[i * 4] = h[i] >> 24;
		out[i * 4 + 1] = h[i] >> 16;
		out[i * 4 + 2] = h[i] >> 8;
		out[i * 4 + 3] = h[i];
	}
}

static void base64_encode(unsigned char* in, int len, char* out)
{
	char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int o = 0;
	for (int i = 0; i < len; i += 3)
	{
		int have = len - i >= 3 ? 2 : len - i - 1;
		uint32_t n = in[i] << 16;
		if (have >= 1) n |= in[i + 1] << 8;
		if (have >= 2) n |= in[i + 2];
		out[o++] = tbl[(n >> 18) & 63];
		out[o++] = tbl[(n >> 12) & 63];
		out[o++] = have >= 1 ? tbl[(n >> 6) & 63] : '=';
		out[o++] = have >= 2 ? tbl[n & 63] : '=';
	}
	out[o] = '\0';
}

static Resource* find_resource(Server* server, char* path)
{
	for (int i = 0; i < server->resource_count; i++)
		if (strcmp(server->resources[i].path, path) == 0)
			return &server->resources[i];
	return NULL;
}

static void ws_handshake(Server* server, int client, char* request)
{
	char* p = strstr(request, "Sec-WebSocket-Key:");
	if (!p)
	{
		close(client);
		return;
	}

	char key[64];
	if (sscanf(p, "Sec-WebSocket-Key:%63s", key) != 1)
	{
		close(client);
		return;
	}

	char concat[128];
	snprintf(concat, sizeof(concat), "%s%s", key, WS_GUID);
	unsigned char digest[20];
	char accept[32];
	sha1_raw(concat, strlen(concat), digest);
	base64_encode(digest, 20, accept);

	char resp[256];
	int n = snprintf(resp, sizeof(resp),
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Accept: %s\r\n\r\n", accept);
	send(client, resp, n, 0);

	if (server->ws_count < MAX_WS_CLIENTS)
		server->ws_clients[server->ws_count++] = client;
	else
		close(client);
}

static void ws_detach(Server* server, int fd)
{
	close(fd);
	for (int i = 0; i < server->ws_count; i++)
	{
		if (server->ws_clients[i] != fd)
			continue;
		server->ws_clients[i] = server->ws_clients[server->ws_count - 1];
		server->ws_count--;
		return;
	}
}

static void handle_request(Server* server, int client)
{
	char request[4096];
	int len = recv(client, request, sizeof(request) - 1, 0);
	if (len <= 0)
	{
		close(client);
		return;
	}
	request[len] = '\0';

	if (strstr(request, "Upgrade: websocket"))
	{
		ws_handshake(server, client, request);
		return;
	}

	char path[1024];
	if (sscanf(request, "GET %1023s", path) != 1)
	{
		http_respond(client, "400 Bad Request", "text/plain", NULL, 0);
		return;
	}

	char* q = strchr(path, '?');
	if (q) *q = '\0';

	Resource* r = find_resource(server, path);
	if (r)
		http_respond(client, "200 OK", r->content_type, r->buffer, r->size);
	else
		http_respond(client, "404 Not Found", "text/plain", "404", 3);
}

int server_fds(void* self, int* out, int max)
{
	Server* server = self;
	int n = 0;
	if (n < max) out[n++] = server->listen_fd;
	for (int i = 0; i < server->ws_count && n < max; i++)
		out[n++] = server->ws_clients[i];
	return n;
}

void server_on_readable(void* self, int fd)
{
	Server* server = self;

	if (fd == server->listen_fd)
	{
		int client;
		while ((client = accept(fd, NULL, NULL)) >= 0)
			handle_request(server, client);
		return;
	}

	unsigned char frame[16];
	int n = recv(fd, frame, sizeof(frame), MSG_DONTWAIT);
	if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return;
	if (n <= 0)
	{
		ws_detach(server, fd);
		return;
	}
	if ((frame[0] & 0x0F) == 0x8)
		ws_detach(server, fd);
}

static void http_respond(int client, char* status, char* content_type, void* buffer, size_t size)
{
	char hdr[512];
	int n = snprintf(hdr, sizeof(hdr),
		"HTTP/1.1 %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n\r\n", status, content_type, size);
	send(client, hdr, n, MSG_MORE);
	if (buffer && size)
		send(client, buffer, size, MSG_NOSIGNAL);
	close(client);
}

Server* server_init(void)
{
	return calloc(1, sizeof(Server));
}

void http_add_resource(Server* server, char* path, void* buffer, size_t size, char* content_type)
{
	if (find_resource(server, path) || server->resource_count == MAX_RESOURCES)
		return;

	Resource* r = &server->resources[server->resource_count++];
	r->path = strdup(path);
	r->content_type = strdup(content_type);
	r->buffer = buffer;
	r->size = size;
}

void http_update_resource(Server* server, char* path, void* buffer, size_t size, char* content_type)
{
	Resource* r = find_resource(server, path);
	if (!r)
	{
		http_add_resource(server, path, buffer, size, content_type);
		return;
	}

	free(r->buffer);
	r->buffer = buffer;
	r->size = size;
	if (strcmp(r->content_type, content_type) != 0)
	{
		free(r->content_type);
		r->content_type = strdup(content_type);
	}
}

void http_delete_resource(Server* server, char* path)
{
	for (int i = 0; i < server->resource_count; i++)
	{
		if (strcmp(server->resources[i].path, path) != 0)
			continue;
		free(server->resources[i].path);
		free(server->resources[i].content_type);
		free(server->resources[i].buffer);
		server->resources[i] = server->resources[server->resource_count - 1];
		server->resource_count--;
		return;
	}
}

void ws_send(Server* server, char* msg)
{
	size_t len = strlen(msg);
	unsigned char* frame = malloc(len + 10);
	size_t flen;

	frame[0] = 0x81;
	if (len < 126)
	{
		frame[1] = len;
		memcpy(frame + 2, msg, len);
		flen = len + 2;
	}
	else if (len <= 0xFFFF)
	{
		frame[1] = 126;
		frame[2] = len >> 8;
		frame[3] = len;
		memcpy(frame + 4, msg, len);
		flen = len + 4;
	}
	else
	{
		frame[1] = 127;
		for (int i = 0; i < 8; i++)
			frame[2 + i] = (unsigned char)(len >> (56 - 8 * i));
		memcpy(frame + 10, msg, len);
		flen = len + 10;
	}

	for (int i = 0; i < server->ws_count; )
	{
		if (send(server->ws_clients[i], frame, flen, MSG_NOSIGNAL) < 0)
		{
			close(server->ws_clients[i]);
			server->ws_clients[i] = server->ws_clients[server->ws_count - 1];
			server->ws_count--;
		}
		else
			i++;
	}
	free(frame);
}

void server_listen(Server* server, int port)
{
	server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server->listen_fd < 0)
	{
		perror("socket");
		exit(1);
	}

	int opt = 1;
	setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(server->listen_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
	{
		perror("bind");
		exit(1);
	}

	if (listen(server->listen_fd, 16) < 0)
	{
		perror("listen");
		exit(1);
	}

	fcntl(server->listen_fd, F_SETFL, O_NONBLOCK);
}
