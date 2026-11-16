#ifndef COMPARE_RESULTS_H
#define COMPARE_RESULTS_H

#include "reload-results.h"

#ifndef MAX_EVENTS
#define MAX_EVENTS 16
#endif

typedef enum { NEW_FILE, UPDATED_FILE, REMOVED_FILE } ChangeType;

typedef struct {
	ChangeType type;
	Result* file;
} ChangeEvent;

typedef struct {
	int length;
	ChangeEvent list[MAX_EVENTS];
} ChangeEvents;

int compare_results(Results* prev, Results* current, ChangeEvents* change_events);

#endif
