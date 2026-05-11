#ifndef ANT_VALUE_H
#define ANT_VALUE_H

#include "common.h"

#define NUMBER_VAL(value)          ((Value){VAL_NUMBER, {.number = value}})
#define BOOL_VAL(value)            ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL                    ((Value){VAL_NIL, {.number = 0}})

#define AS_NUMBER(value)           ((value).as.number)
#define AS_BOOL(value)             ((value).as.boolean)
#define AS_NIL(value)              ((value).as.number)

#define IS_NUMBER(value)           ((value).type == VAL_NUMBER)
#define IS_BOOL(value)             ((value).type == VAL_BOOL)
#define IS_NIL(value)              ((value).type == VAL_NIL)

typedef enum {
	VAL_NUMBER,
	VAL_BOOL,
	VAL_NIL
} ValueType;

typedef struct {
	ValueType type;

	union {
		double number;
		bool boolean;
	} as;
} Value;

typedef struct {
	int capacity;
	int count;
	Value* values;
} ValueArray;


void initValueArray(ValueArray* array);
int writeValueArray(ValueArray* array, Value value);
void printValue(Value value);
bool valuesEqual(Value a, Value b);
void freeValueArray(ValueArray* array);

#endif // ANT_VALUE_H
