#ifndef ANT_MEMORY_H
#define ANT_MEMORY_H

#include "common.h"

#define ALLOCATE(type, size)                           ((type*)reallocate(NULL, 0, sizeof(type)*(size)))
#define GROW_CAPACITY(oldCapacity)                     ((oldCapacity) < 8 ? 8 : ((oldCapacity) * 2))
#define GROW_ARRAY(type, ptr, oldSize, newSize)        ((type*)reallocate(ptr, sizeof(type)*(oldSize), sizeof(type)*(newSize)))
#define FREE_ARRAY(type, ptr, oldSize)                 ((type*)reallocate(ptr, sizeof(type)*(oldSize), 0))
#define FREE(type, ptr)                                ((type*)reallocate(ptr, sizeof(type), 0))

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* ptr, size_t oldSize, size_t newSize);
void markObject(Obj* object);
void markValue(Value value);
void collectGarbage();
void freeObjects();

#endif // ANT_MEMORY_H
