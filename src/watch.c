#include "watch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#define MAX_WATCHES 1024

struct Watcher {
	int fd;
	void (*on_ly_change)(void* params);
	void* params;
};

static char* watched[MAX_WATCHES];

static int has_ext(char* name, char* ext)
{
	return strlen(name) > strlen(ext) && strcmp(name + strlen(name) - strlen(ext), ext) == 0;
}

static void add_watch(int fd, char* dir)
{
	int wd = inotify_add_watch(fd, dir, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);
	if (wd < 0 || wd >= MAX_WATCHES) return;
	watched[wd] = strdup(dir);

	DIR* d = opendir(dir);
	struct dirent* e;
	char path[1024];
	while (d && (e = readdir(d)))
	{
		if (e->d_name[0] == '.') continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
			add_watch(fd, path);
	}
	if (d) closedir(d);
}

Watcher* watch_init(char* dir, void (*on_ly_change)(void* params), void* params)
{
	Watcher* w = malloc(sizeof(Watcher));
	w->fd = inotify_init();
	w->on_ly_change = on_ly_change;
	w->params = params;
	fcntl(w->fd, F_SETFL, O_NONBLOCK);
	add_watch(w->fd, dir);
	return w;
}

int watch_fds(void* self, int* out, int max)
{
	Watcher* w = self;
	if (max < 1) return 0;
	out[0] = w->fd;
	return 1;
}

void watch_ready(void* self, int fd)
{
	Watcher* w = self;

	unsigned char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
	int len;
	while ((len = read(fd, buf, sizeof(buf))) > 0)
	{
		for (char* p = (char*)buf; p < (char*)buf + len; )
		{
			struct inotify_event* e = (struct inotify_event*)p;
			p += sizeof(struct inotify_event) + e->len;
			if (!e->len) continue;

			if (e->mask & IN_ISDIR && e->mask & IN_CREATE)
			{
				char path[1024];
				snprintf(path, sizeof(path), "%s/%s", watched[e->wd], e->name);
				add_watch(fd, path);
			}
			else if (has_ext(e->name, ".ly") &&
				(e->mask & (IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM)))
				w->on_ly_change(w->params);
		}
	}
}
