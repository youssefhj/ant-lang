#include <stdlib.h>

#include "vm.h"
#include "memory.h"

extern VM vm;

void* reallocate(void* ptr, size_t newSize) {
	if (newSize <= 0) {
		free(ptr);
		return NULL;
	}

	void* newPtr = realloc(ptr, newSize);
	if (newPtr == NULL) exit(1);

	return newPtr;
}

static void freeObject(Obj* object) {
	switch (object->type) {
		case OBJ_STRING: {
			ObjString* objString = (ObjString*)object;
			FREE_ARRAY(char, objString->chars);
			FREE(ObjString, objString);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction* objFunction = (ObjFunction*)object;
			freeChunk(&objFunction->chunk);
			FREE(ObjFunction, objFunction);
			break;
		}
	}

}

void freeObjects() {
	Obj* object = vm.objects;
	while (object != NULL) {
		Obj* next = object->next;
		freeObject(object);
		
		object = next;
	}
}
