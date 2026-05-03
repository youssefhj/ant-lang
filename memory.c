#include <stdlib.h>

#include "memory.h"


void* reallocate(void* ptr, size_t newSize) {
	if (newSize <= 0) {
		free(ptr);
		return NULL;
	}

	void* newPtr = realloc(ptr, newSize);
	if (newPtr == NULL) exit(1);

	return newPtr;
}
