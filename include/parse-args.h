#ifndef PARSE_ARGS_H
#define PARSE_ARGS_H

typedef struct {
	int PORT;
	char INPUT[512];
} Args;

int parse_args(Args* args, int argc, char* argv[]);

#endif
