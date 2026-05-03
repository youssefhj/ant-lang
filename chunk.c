#include "chunk.h"
#include "value.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
	chunk->capacity = 0;
	chunk->count = 0;
	chunk->code = NULL;
	initValueArray(&chunk->constants);
	chunk->lines = NULL;
}

void writeChunk(Chunk* chunk, uint8_t value, int line) {
	if (chunk->count + 1 > chunk->capacity) {
		// Allocating some extra memory
		chunk->capacity = GROW_CAPACITY(chunk->capacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, chunk->capacity);
		chunk->lines = GROW_ARRAY(int, chunk->lines, chunk->capacity);
	}

	chunk->code[chunk->count] = value;
	chunk->lines[chunk->count] = line;
	chunk->count++;
}

int writeConstant(Chunk* chunk, Value value) {
	return writeValueArray(&chunk->constants, value);
}

void freeChunk(Chunk* chunk) {
	FREE_ARRAY(uint8_t, chunk->code);
	FREE_ARRAY(int, chunk->lines);
	freeValueArray(&chunk->constants);
	initChunk(chunk);
}
