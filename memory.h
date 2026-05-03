#ifndef ANT_MEMORY_H
#define ANT_MEMORY_H

#include "common.h"

#define GROW_CAPACITY(oldCapacity)	(oldCapacity < 8 ? 8 : (oldCapacity * 2))
#define GROW_ARRAY(type, ptr, newSize)	((type*)reallocate(ptr, sizeof(type)*newSize))
#define FREE_ARRAY(type, ptr)		((type*)reallocate(ptr, 0))

void* reallocate(void* ptr, size_t newSize);


#endif // ANT_MEMORY_H
