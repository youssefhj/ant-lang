#include <stdio.h>
#include <string.h>

#include "object.h"
#include "vm.h"
#include "memory.h"

#define ALLOCATE_OBJ(type, objType)          ((type*)allocateObject(sizeof(type), objType))

extern VM vm;

static Obj* allocateObject(size_t size, ObjType type) {
	Obj* object = (Obj*)reallocate(NULL, size);
	object->type = type;
	
	object->next = vm.objects;
	vm.objects = object;

	return object;
}

static ObjString* allocateString(char* chars, int length) {
	ObjString* objString = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	
	objString->chars = chars;
	objString->length = length;
	
	return objString;
}

ObjString* takeString(char* chars, int length) {
	return allocateString(chars, length);
}

ObjString* copyString(const char* start, int length) {
	char* heapChars = ALLOCATE(char, length + 1);
	heapChars[length] = '\0';

	memcpy(heapChars, start, length);

	return allocateString(heapChars, length);
}

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
		case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
	}
}
