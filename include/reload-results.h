#ifndef RELOAD_RESULTS_H
#define RELOAD_RESULTS_H

#include <stdio.h>

typedef enum { PDF, MIDI } LilypondFiletype;

typedef struct {
	LilypondFiletype type;
	char* name;
	char hash[41];
	size_t size;
	void* buffer;
} Result;

typedef struct {
	int length;
	Result** list;
} Results;

#define ERROR_SIZE 4096

int reload_results(char* input, Results* results, char* error);

#endif
