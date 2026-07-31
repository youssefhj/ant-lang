#ifndef ANT_OBJECT_H
#define ANT_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

#define OBJ_TYPE(value)           (AS_OBJ(value)->type)

#define AS_STRING(value)          ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)         (((ObjString*)AS_OBJ(value))->chars)
#define AS_FUNCTION(value)        ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value)         ((ObjClosure*)AS_OBJ(value))
#define AS_NATIVE(value)          (((ObjNative*)AS_OBJ(value))->function)
#define AS_CLASS(value)           ((ObjClass*)AS_OBJ(value))
#define AS_INSTANCE(value)        ((ObjInstance*)AS_OBJ(value))
#define AS_BOUND_METHOD(value)    ((ObjBoundMethod*)AS_OBJ(value))

#define IS_STRING(value)          (isObjType(value, OBJ_STRING))
#define IS_FUNCTION(value)        (isObjType(value, OBJ_FUNCTION))
#define IS_CLOSURE(value)         (isObjType(value, OBJ_CLOSURE))
#define IS_NATIVE(value)          (isObjType(value, OBJ_NATIVE))
#define IS_CLASS(value)           (isObjType(value, OBJ_CLASS))
#define IS_INSTANCE(value)        (isObjType(value, OBJ_INSTANCE))
#define IS_BOUND_METHOD(value)    (isObjType(value, OBJ_BOUND_METHOD))

typedef enum {
	OBJ_STRING,
	OBJ_FUNCTION,
	OBJ_CLOSURE,
	OBJ_UPVALUE,
	OBJ_NATIVE,
	OBJ_CLASS,
	OBJ_INSTANCE,
	OBJ_BOUND_METHOD
} ObjType;

struct Obj {
	ObjType type;
	Obj* next;
};

struct ObjString {
	Obj obj;
	char* chars;
	int length;
	uint32_t hash;
};

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

typedef Value (*NativeFn)(int, Value*);

typedef struct {
	Obj obj;
	NativeFn function;
} ObjNative;

typedef struct {
	Obj obj;
	ObjString* name;
	Table methods;
} ObjClass;

typedef struct {
	Obj obj;
	ObjClass* klass;
	Table fields;
} ObjInstance;

typedef struct {
	Obj obj;
	Value receiver;
	ObjClosure* method;
} ObjBoundMethod;

ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* start, int length);
ObjFunction* newFunction();
ObjClosure* newClosure(ObjFunction* function);
ObjUpvalue* newUpvalue(Value* slot);
ObjNative* newNative(NativeFn function);
ObjClass* newClass(ObjString* name);
ObjInstance* newInstance(ObjClass* klass);
ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif // ANT_OBJECT_H
