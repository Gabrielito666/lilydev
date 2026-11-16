#include "event-loop.h"
#include <poll.h>
#include <errno.h>

#define MAX_FDS 256
#define MAX_PER_SOURCE 64

int loop_poll(EventSource* sources, int n, int timeout_ms)
{
	struct pollfd pfds[MAX_FDS];
	int owner[MAX_FDS];
	int count = 0;

	for (int i = 0; i < n && count < MAX_FDS; i++)
	{
		int fds[MAX_PER_SOURCE];
		int got = sources[i].fds(sources[i].self, fds, MAX_PER_SOURCE);
		for (int j = 0; j < got && count < MAX_FDS; j++)
		{
			pfds[count].fd = fds[j];
			pfds[count].events = POLLIN;
			pfds[count].revents = 0;
			owner[count] = i;
			count++;
		}
	}

	int nready = poll(pfds, count, timeout_ms);
	if (nready <= 0)
		return nready;

	for (int i = 0; i < count; i++)
	{
		if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR))
			sources[owner[i]].ready(sources[owner[i]].self, pfds[i].fd);
	}

	return nready;
}

void loop_run(EventSource* sources, int n)
{
	while (loop_poll(sources, n, -1) >= 0 || errno == EINTR)
		;
}
