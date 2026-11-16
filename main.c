#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "parse-args.h"
#include "reload-results.h"
#include "compare-results.h"
#include "event-loop.h"
#include "server.h"
#include "watch.h"

#define DEFAULT_PORT 8080
#define DEFAULT_INPUT "./main.ly"

typedef struct {
	char* input;
	Results* results;
	Server* server;
} OnLyChangeParams;

static void* read_file(char* path, size_t* size)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return NULL;

	off_t off = lseek(fd, 0, SEEK_END);
	void* buffer = malloc(off);
	lseek(fd, 0, SEEK_SET);
	if (read(fd, buffer, off) != off)
	{
		close(fd);
		free(buffer);
		return NULL;
	}
	close(fd);

	*size = off;
	return buffer;
}

static void on_ly_change(void* _params)
{
	OnLyChangeParams* params = _params;

	Results prev_results = *params->results;

	char error[ERROR_SIZE];
	if (reload_results(params->input, params->results, error) < 0)
	{
		ws_send(params->server, "{\"event\":\"error\"}");
		return;
	}

	ChangeEvents change_events;
	if (compare_results(&prev_results, params->results, &change_events) < 0)
		return;

	for (int i = 0; i < change_events.length; i++)
	{
		ChangeEvent* ev = &change_events.list[i];
		char path[600];
		snprintf(path, sizeof(path), "/%s", ev->file->name);

		char* type;
		if (ev->type == REMOVED_FILE)
		{
			http_delete_resource(params->server, path);
			type = "removed";
		}
		else
		{
			http_update_resource(params->server, path, ev->file->buffer, ev->file->size,
				ev->file->type == PDF ? "application/pdf" : "audio/midi");
			ev->file->buffer = NULL;
			type = ev->type == NEW_FILE ? "new" : "updated";
		}

		char msg[256];
		snprintf(msg, sizeof(msg), "{\"event\":\"%s\",\"file\":\"%s\"}", type, ev->file->name);
		ws_send(params->server, msg);
	}

	for (int i = 0; i < prev_results.length; i++)
	{
		free(prev_results.list[i]->name);
		free(prev_results.list[i]);
	}
	free(prev_results.list);

	for (int i = 0; i < params->results->length; i++)
	{
		Result* r = params->results->list[i];
		if (r->buffer)
		{
			free(r->buffer);
			r->buffer = NULL;
		}
	}
}

int main(int argc, char* argv[])
{
	Args args = { .PORT = DEFAULT_PORT, .INPUT = DEFAULT_INPUT };
	if (parse_args(&args, argc, argv) < 0)
	{
		fprintf(stderr, "Use: %s [-p|--port <puerto>] [input]\n", argv[0]);
		exit(1);
	}

	Results results = { .length = 0 };

	Server* server = server_init();
	server_listen(server, args.PORT);

	char* index_path = getenv("LILYDEV_HTML");
	if (!index_path)
	{
		fprintf(stderr, "Falta la variable de entorno LILYDEV_HTML\n");
		exit(1);
	}
	size_t index_size;
	void* index = read_file(index_path, &index_size);
	if (index)
		http_add_resource(server, "/", index, index_size, "text/html");

	char error[ERROR_SIZE];
	if (reload_results(args.INPUT, &results, error) == 0)
	{
		for (int i = 0; i < results.length; i++)
		{
			Result* r = results.list[i];
			char path[600];
			snprintf(path, sizeof(path), "/%s", r->name);
			http_add_resource(server, path, r->buffer, r->size,
				r->type == PDF ? "application/pdf" : "audio/midi");
			r->buffer = NULL;
		}
	}
	else
		fprintf(stderr, "%s\n", error);

	OnLyChangeParams on_ly_change_params = {
		.input = args.INPUT,
		.results = &results,
		.server = server
	};

	Watcher* watcher = watch_init(".", on_ly_change, &on_ly_change_params);

	EventSource sources[] = {
		{ server, server_fds, server_on_readable },
		{ watcher, watch_fds, watch_ready },
	};
	loop_run(sources, 2);

	return 0;
}
