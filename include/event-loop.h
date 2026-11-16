#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

typedef struct {
	void* self;
	int (*fds)(void* self, int* out, int max);
	void (*ready)(void* self, int fd);
} EventSource;

int loop_poll(EventSource* sources, int n, int timeout_ms);
void loop_run(EventSource* sources, int n);

#endif
