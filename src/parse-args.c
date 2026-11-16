#include "parse-args.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parse_args(Args* args, int argc, char* argv[])
{
	for (int i = 1; i < argc; i++)
	{
		char *arg = argv[i];

		if (strcmp(arg, "--port") == 0 || strcmp(arg, "-p") == 0)
		{
			if (++i >= argc) return -1;
			args->PORT = atoi(argv[i]);
		}
		else if (strncmp(arg, "--port=", 7) == 0) args->PORT = atoi(arg + 7);
		else if (strncmp(arg, "-p=", 3) == 0) args->PORT = atoi(arg + 3);
		else snprintf(args->INPUT, sizeof(args->INPUT), "%s", arg);
	}

	if (args->PORT <= 0 || args->PORT > 65535) return -1;

	return 0;
}
