#ifndef ANT_CHUNK_H
#define ANT_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_TRUE,
	OP_FALSE,
	OP_NIL,
	OP_EQUAL,
	OP_NOT,
	OP_LESS,
	OP_GREATER,
	OP_NEGATE,
	OP_POP,
	OP_DEFINE_GLOBAL,
	OP_GET_GLOBAL,
	OP_SET_GLOBAL,
	OP_GET_LOCAL,
	OP_SET_LOCAL,
	OP_JUMP_IF_FALSE,
	OP_JUMP,
	OP_CONSTANT,
	OP_PRINT,
	OP_RETURN	
} OpCode;

typedef struct {
	int capacity;
	int count;
	uint8_t* code;
	ValueArray constants;
	int* lines;
} Chunk;

void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t value, int line);
int writeConstant(Chunk* chunk, Value value);
void freeChunk(Chunk* chunk);

#endif // ANT_CHUNK_H
