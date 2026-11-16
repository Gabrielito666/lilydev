#ifndef WATCH_H
#define WATCH_H

typedef struct Watcher Watcher;

Watcher* watch_init(char* dir, void (*on_ly_change)(void* params), void* params);

int watch_fds(void* self, int* out, int max);
void watch_ready(void* self, int fd);

#endif
