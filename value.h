#ifndef ANT_VALUE_H
#define ANT_VALUE_H


typedef double Value;

typedef struct {
	int capacity;
	int count;
	Value* values;
} ValueArray;


void initValueArray(ValueArray* array);
int writeValueArray(ValueArray* array, Value value);
void printValue(Value value);
void freeValueArray(ValueArray* array);

#endif // ANT_VALUE_H
