#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

typedef struct Server Server;

Server* server_init(void);
void server_listen(Server* server, int port);

int server_fds(void* self, int* out, int max);
void server_on_readable(void* self, int fd);

void ws_send(Server* server, char* msg);

void http_add_resource(Server* server, char* path, void* buffer, size_t size, char* content_type);
void http_update_resource(Server* server, char* path, void* buffer, size_t size, char* content_type);
void http_delete_resource(Server* server, char* path);

#endif
