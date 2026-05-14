#ifndef ANT_OBJECT_H
#define ANT_OBJECT_H

#include "value.h"

#define OBJ_TYPE(value)           (AS_OBJ(value)->type)

#define AS_STRING(value)          ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)         (((ObjString*)AS_OBJ(value))->chars)

#define IS_STRING(value)          (isObjType(value, OBJ_STRING))

typedef enum {
	OBJ_STRING
} ObjType;

struct Obj {
	ObjType type;
	Obj* next;
};

typedef struct {
	Obj obj;
	char* chars;
	int length;
} ObjString;

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* start, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif // ANT_OBJECT_H
