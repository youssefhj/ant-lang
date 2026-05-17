#ifndef ANT_VM_H
#define ANT_VM_H

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "object.h"
#include "table.h"

#define STACK_MAX_SIZE 1024


typedef enum {
	INTERPRET_COMPILETIME_ERROR,
	INTERPRET_RUNTIME_ERROR,
	INTERPRET_SUCCESS
} InterpretResult;


typedef struct {
	Chunk chunk;
	uint8_t* ip;
	Value stack[STACK_MAX_SIZE];
	Value* topStack;
	Obj* objects;
	Table globals;
	Table strings;
} VM;


void initVM();
void freeVM();
InterpretResult interpret(const char* source);


#endif // ANT_VM_H
