#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

static void json_escape(char* out, char* in)
{
	char* o = out;
	for (char* p = in; *p; p++)
	{
		if (*p == '"' || *p == '\\') *o++ = '\\';
		else if (*p == '\n') { *o++ = '\\'; *o++ = 'n'; continue; }
		else if (*p == '\t') { *o++ = '\\'; *o++ = 't'; continue; }
		else if (*p == '\r' || (unsigned char)*p < 0x20)
			continue;
		*o++ = *p;
	}
	*o = '\0';
}

static void update_files_json(Server* server, Results* results)
{
	char json[2048];
	int o = snprintf(json, sizeof(json), "{\"pdfs\":[");

	for (int pass = 0; pass < 2; pass++)
	{
		if (pass == 1)
			o += snprintf(json + o, sizeof(json) - o, "],\"midis\":[");

		int first = 1;
		for (int i = 0; i < results->length; i++)
		{
			Result* r = results->list[i];
			if ((pass == 0) != (r->type == PDF))
				continue;
			o += snprintf(json + o, sizeof(json) - o, "%s{\"name\":\"%s\",\"hash\":\"%s\"}",
				first ? "" : ",", r->name, r->hash);
			first = 0;
		}
	}

	snprintf(json + o, sizeof(json) - o, "]}");
	http_update_resource(server, "/files/", strdup(json), strlen(json), "application/json");
}

static void on_ly_change(void* _params)
{
	OnLyChangeParams* params = _params;

	Results prev_results = *params->results;

	ws_send(params->server, "{\"event\":\"compiling\"}");

	char error[ERROR_SIZE];
	if (reload_results(params->input, params->results, error) < 0)
	{
		char esc[ERROR_SIZE * 2];
		char msg[ERROR_SIZE * 2 + 64];
		json_escape(esc, error);
		snprintf(msg, sizeof(msg), "{\"event\":\"error\",\"message\":\"%s\"}", esc);
		ws_send(params->server, msg);
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

		if (ev->type == REMOVED_FILE)
			http_delete_resource(params->server, path);
		else
		{
			http_update_resource(params->server, path, ev->file->buffer, ev->file->size,
				ev->file->type == PDF ? "application/pdf" : "audio/midi");
			ev->file->buffer = NULL;
		}
	}

	update_files_json(params->server, params->results);

	for (int i = 0; i < change_events.length; i++)
	{
		ChangeEvent* ev = &change_events.list[i];
		char* type = ev->type == NEW_FILE ? "new" : ev->type == UPDATED_FILE ? "updated" : "removed";
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
		fprintf(stderr, "Usage: %s [-p|--port <port>] [input]\n", argv[0]);
		exit(1);
	}

	Results results = { .length = 0 };

	Server* server = server_init();
	server_listen(server, args.PORT);

	char* index_path = getenv("LILYDEV_HTML");
	if (!index_path)
		index_path = "/usr/share/lilydev/index.html";
	size_t index_size;
	void* index = read_file(index_path, &index_size);
	if (!index)
	{
		fprintf(stderr, "No index.html found at %s\n", index_path);
		exit(1);
	}
	http_add_resource(server, "/", index, index_size, "text/html");

	/* AudioWorklet processor for the MIDI player, stored next to index.html */
	char synth_js_path[1024];
	char* slash = strrchr(index_path, '/');
	if (slash)
	{
		snprintf(synth_js_path, sizeof(synth_js_path), "%.*s/synth.js",
			(int)(slash - index_path), index_path);
	}
	else
		snprintf(synth_js_path, sizeof(synth_js_path), "synth.js");
	size_t synth_js_size;
	void* synth_js = read_file(synth_js_path, &synth_js_size);
	if (synth_js)
		http_add_resource(server, "/synth.js", synth_js, synth_js_size, "application/javascript");
	else
		fprintf(stderr, "No synth.js found at %s\n", synth_js_path);

	/* SoundFont for the MIDI player */
	char* soundfont_path = getenv("LILYDEV_SOUNDFONT");
	if (!soundfont_path)
		soundfont_path = "/usr/share/lilydev/sounds/GeneralUser-GS.sf2";
	size_t soundfont_size;
	void* soundfont = read_file(soundfont_path, &soundfont_size);
	if (soundfont)
		http_add_resource(server, "/soundfont.sf2", soundfont, soundfont_size, "application/octet-stream");
	else
		fprintf(stderr, "No soundfont found at %s, the MIDI player will fall back to the basic synth\n", soundfont_path);

	update_files_json(server, &results);

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

	update_files_json(server, &results);

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
