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

static uint32_t hashString(const char* key, int length) {
	uint32_t hash = 2166136261u;

	for (int i = 0; i < length; i++) {
		hash ^= key[i];
		hash *= 16777619;
	}

	return hash;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
	ObjString* objString = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	
	objString->chars = chars;
	objString->length = length;
	objString->hash = hash;
	
	tableSet(&vm.strings, objString, NIL_VAL);
	return objString;
}

ObjString* takeString(char* chars, int length) {
	uint32_t hash = hashString(chars, length);
	
	ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars);	
		return interned;
	}

	return allocateString(chars, length, hash);
}

ObjString* copyString(const char* start, int length) {
	uint32_t hash = hashString(start, length);
	
	ObjString* interned = tableFindString(&vm.strings, start, length, hash);
	if (interned != NULL) return interned;

	char* heapChars = ALLOCATE(char, length + 1);
	heapChars[length] = '\0';

	memcpy(heapChars, start, length);

	return allocateString(heapChars, length, hash);
}

ObjFunction* newFunction() {
	ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);

	function->name = NULL;
	function->arity = 0;
	initChunk(&function->chunk);

	return function;
}

static void printFunction(ObjFunction* function) {
	if (function->name == NULL) {
		printf("<script>");
		return;
	}

	printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
		case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
		case OBJ_FUNCTION: printFunction(AS_FUNCTION(value)); break;
	}
}
