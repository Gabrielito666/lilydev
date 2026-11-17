#include "reload-results.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

static uint32_t rol(uint32_t x, int c)
{
	return (x << c) | (x >> (32 - c));
}

static void sha1_hex(void* data, size_t len, char hex[41])
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
		sprintf(hex + i * 8, "%08x", h[i]);
}

static Result* read_result(char* dir, char* name, LilypondFiletype type)
{
	char path[600];
	snprintf(path, sizeof(path), "%s/%s", dir, name);

	int fd = open(path, O_RDONLY);
	if (fd < 0) return NULL;

	size_t size = lseek(fd, 0, SEEK_END);
	void* buffer = malloc(size);
	lseek(fd, 0, SEEK_SET);
	if (read(fd, buffer, size) != (ssize_t)size)
	{
		close(fd);
		free(buffer);
		return NULL;
	}
	close(fd);

	Result* r = malloc(sizeof(Result));
	r->type = type;
	r->name = strdup(name);
	r->size = size;
	r->buffer = buffer;
	sha1_hex(buffer, size, r->hash);
	return r;
}

static void clean_dir(char* dir)
{
	char path[600];
	DIR* d = opendir(dir);
	struct dirent* e;
	while (d && (e = readdir(d)))
	{
		if (e->d_name[0] == '.') continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		unlink(path);
	}
	if (d) closedir(d);
	rmdir(dir);
}

int reload_results(char* input, Results* results, char* error)
{
	char tmpdir[] = "/tmp/lilypond_XXXXXX";
	if (!mkdtemp(tmpdir))
	{
		snprintf(error, ERROR_SIZE, "mkdtemp fallo");
		return -1;
	}

	char outbase[600];
	snprintf(outbase, sizeof(outbase), "%s/out", tmpdir);

	int fds[2];
	pipe(fds);
	pid_t pid = fork();
	if (pid == 0)
	{
		close(fds[0]);
		dup2(fds[1], 1);
		dup2(fds[1], 2);
		close(fds[1]);
		char* cmd[] = { "lilypond", "-s", "-dno-point-and-click", "-o", outbase, input, NULL };
		execvp(cmd[0], cmd);
		_exit(127);
	}
	close(fds[1]);

	size_t n = 0, r;
	while (n < ERROR_SIZE - 1 && (r = read(fds[0], error + n, ERROR_SIZE - 1 - n)) > 0)
		n += r;
	error[n] = '\0';
	close(fds[0]);

	int status;
	waitpid(pid, &status, 0);

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	{
		if (!n) snprintf(error, ERROR_SIZE, "fallo lilypond");
		clean_dir(tmpdir);
		return -1;
	}

	results->length = 0;
	results->list = NULL;

	DIR* d = opendir(tmpdir);
	struct dirent* e;
	while (d && (e = readdir(d)))
	{
		char* ext = strrchr(e->d_name, '.');
		if (!ext) continue;

		LilypondFiletype type;
		if (strcmp(ext, ".pdf") == 0) type = PDF;
		else if (strcmp(ext, ".midi") == 0) type = MIDI;
		else continue;

		Result* r = read_result(tmpdir, e->d_name, type);
		if (!r) continue;
		results->list = realloc(results->list, (results->length + 1) * sizeof(Result*));
		results->list[results->length++] = r;
	}
	if (d) closedir(d);

	clean_dir(tmpdir);
	return 0;
}
