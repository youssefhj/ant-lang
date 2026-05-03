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
	printf("%g", value);
}

void freeValueArray(ValueArray* array) {
	FREE_ARRAY(Value, array->values);
	initValueArray(array);
}
