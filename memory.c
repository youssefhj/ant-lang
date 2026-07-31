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
			ObjString* string = (ObjString*)object;
			FREE_ARRAY(char, string->chars);
			FREE(ObjString, object);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction* function = (ObjFunction*)object;
			freeChunk(&function->chunk);
			FREE(ObjFunction, object);
			break;
		}
		case OBJ_CLOSURE: {
			ObjClosure* closure = (ObjClosure*)object;
			FREE_ARRAY(ObjUpvalue*, closure->upvalues);
			FREE(ObjClosure, object);
			break;
		}
		case OBJ_UPVALUE: {
			FREE(ObjUpvalue, object);	
			break;
		}
		case OBJ_NATIVE: {
			FREE(ObjNative, object);	
			break;

		}
		case OBJ_CLASS: {
			ObjClass* klass = (ObjClass*)object;
			freeTable(&klass->methods);
			FREE(ObjClass, object);	
			break;
		}
		case OBJ_INSTANCE: {
			ObjInstance* instance = (ObjInstance*)object;
			freeTable(&instance->fields);
			FREE(ObjInstance, object);	
			break;
		}
		case OBJ_BOUND_METHOD: {
			FREE(ObjBoundMethod, object);
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
