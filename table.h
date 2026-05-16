#ifndef ANT_TABLE_H
#define ANT_TABLE_H

#include "common.h"
#include "object.h"
#include "value.h"

typedef struct {
	ObjString* key;
	Value value;
} Entry;

typedef struct {
	int capacity;
	int count;
	Entry* entries;
} Table;

void initTable(Table* table);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableDelete(Table* table, ObjString* key);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

#endif // ANT_TABLE_H
