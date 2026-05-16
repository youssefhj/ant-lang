#include <string.h>

#include "table.h"
#include "memory.h"

#define MAX_LOAD_FACTOR 0.70

void initTable(Table* table) {
	table->capacity = 0;
	table->count = 0;
	table->entries = NULL;
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
	if (entries == NULL) return NULL;

	int index = key->hash % capacity;
	
	Entry* entry = NULL;
	Entry* tombstone = NULL;

	for (;;) {
		entry = &entries[index];

		if (entry->key == NULL) {
		       	if (IS_NIL(entry->value)) {
				return tombstone == NULL ? entry : tombstone;
			} else {
				if (tombstone == NULL) tombstone = entry;
			}
		} else if (key == entry->key && key->length == entry->key->length 
				&& memcmp(key, entry->key, key->length) == 0) {
			return entry;
		}

		index = (index + 1) % capacity;
	}

	return NULL;

}

static void adjustCapacity(Table* table, int capacity) {
	Entry* entries = ALLOCATE(Entry, capacity);

	for (int idx = 0; idx < capacity; idx++) {
		entries[idx].key = NULL;
		entries[idx].value = NIL_VAL;
	}

	table->count = 0;
	for (int idx = 0; idx < table->capacity; idx++) {
		Entry* entry = &table->entries[idx];
		if (entry->key == NULL)  continue;
		

		Entry* newEntry = findEntry(entries, capacity, entry->key);
		newEntry->key = entry->key;
		newEntry->value = entry->value;
		table->count++;
	}

	FREE_ARRAY(Entry, table->entries);
	table->capacity = capacity;
	table->entries = entries;
}

bool tableSet(Table* table, ObjString* key, Value value) {
	if (table == NULL) return false;

	if (table->count + 1 > table->capacity * MAX_LOAD_FACTOR) {
		adjustCapacity(table, GROW_CAPACITY(table->capacity));
	}
	
	Entry* entry = findEntry(table->entries, table->capacity, key);
	bool isNewKey = entry->key == NULL;

	if (isNewKey && IS_NIL(entry->value)) {
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	
	return isNewKey;	
}

bool tableGet(Table* table, ObjString* key, Value* value) {
	if (table == NULL || table->count == 0) return false;
	
	Entry* entry = findEntry(table->entries, table->capacity, key);
	
	if (entry->key == NULL) return false;
	
	*value = entry->value;
	return true;
}

bool tableDelete(Table* table, ObjString* key) {
	if (table == NULL || table->count == 0) return false;
	
	Entry* entry = findEntry(table->entries, table->capacity, key);
	if (entry->key == NULL) return false;

	// Rest in Peace (R.I.P)
	entry->key = NULL;
	entry->value = BOOL_VAL(true); // tombstone

	return true;
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
	if (table == NULL || table->count == 0) return NULL;
	
	int capacity = table->capacity;
	int index = hash % capacity;

	Entry* entry = NULL;

	for (;;) {
		entry = &table->entries[index];

		if (entry == NULL) {
			if (IS_NIL(entry->value)) return NULL;
		} else if (entry->key->hash == hash && entry->key->length == length 
				&& memcmp(entry->key->chars, chars, length) == 0) {
			return entry->key;
		}

		index = (index + 1) % capacity;

	}

	return NULL;
}
