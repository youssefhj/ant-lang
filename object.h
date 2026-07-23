#ifndef ANT_OBJECT_H
#define ANT_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value)           (AS_OBJ(value)->type)

#define AS_STRING(value)          ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)         (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)        ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value)         ((ObjClosure*)AS_OBJ(value))

#define IS_STRING(value)          (isObjType(value, OBJ_STRING))
#define IS_FUNCTION(value)        (isObjType(value, OBJ_FUNCTION))
#define IS_CLOSURE(value)         (isObjType(value, OBJ_CLOSURE))

typedef enum {
	OBJ_STRING,
	OBJ_FUNCTION,
	OBJ_CLOSURE,
	OBJ_UPVALUE,
	OBJ_NATIVE
} ObjType;

struct Obj {
	ObjType type;
	Obj* next;
};

typedef struct {
	Obj obj;
	char* chars;
	int length;
	uint32_t hash;
} ObjString;

typedef struct {
	Obj obj;
	ObjString* name;
	int arity;
	Chunk chunk;
	int upvalueCount;
} ObjFunction;

typedef struct ObjUpvalue {
	Obj obj;
	Value* location;
	Value closed;
	struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
	Obj obj;
	ObjFunction* function;
	ObjUpvalue** upvalues;
	int upvalueCount;
} ObjClosure;

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* start, int length);
ObjFunction* newFunction();
ObjClosure* newClosure(ObjFunction* function);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif // ANT_OBJECT_H
