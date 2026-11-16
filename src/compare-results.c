#include "compare-results.h"
#include <string.h>

static Result* find_by_name(Results* results, char* name)
{
	for (int i = 0; i < results->length; i++)
		if (strcmp(results->list[i]->name, name) == 0)
			return results->list[i];
	return NULL;
}

int compare_results(Results* prev, Results* current, ChangeEvents* change_events)
{
	change_events->length = 0;

	for (int i = 0; i < current->length; i++)
	{
		Result* r = current->list[i];
		Result* old = find_by_name(prev, r->name);

		ChangeType type;
		if (!old) type = NEW_FILE;
		else if (strcmp(old->hash, r->hash) != 0) type = UPDATED_FILE;
		else continue;

		if (change_events->length == MAX_EVENTS)
			return -1;
		change_events->list[change_events->length] = (ChangeEvent){ .type = type, .file = r };
		change_events->length++;
	}

	for (int i = 0; i < prev->length; i++)
	{
		if (find_by_name(current, prev->list[i]->name))
			continue;
		if (change_events->length == MAX_EVENTS)
			return -1;
		change_events->list[change_events->length] = (ChangeEvent){ .type = REMOVED_FILE, .file = prev->list[i] };
		change_events->length++;
	}

	return change_events->length;
}
