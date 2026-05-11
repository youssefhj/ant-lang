#include <stdio.h>

#include "value.h"
#include "memory.h"

void initValueArray(ValueArray* array) {
	array->capacity = 0;
	array->count = 0;
	array->values = NULL;
}

int writeValueArray(ValueArray* array, Value value) {
	if (array->count + 1 > array->capacity) {
		// Allocating some extra memory
		array->capacity = GROW_CAPACITY(array->capacity);
		array->values = GROW_ARRAY(Value, array->values, array->capacity);
	}

	array->values[array->count++] = value;
	return array->count - 1;
}

void printValue(Value value) {
	switch (value.type) {
		case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
		case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
		case VAL_NIL: printf("nil"); break;
	}
}

bool valuesEqual(Value a, Value b) {
	if (a.type != b.type) return false;

	switch (a.type) {
		case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
		case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
		case VAL_NIL: return true;
		default:
		        return false;
	}
}

void freeValueArray(ValueArray* array) {
	FREE_ARRAY(Value, array->values);
	initValueArray(array);
}
