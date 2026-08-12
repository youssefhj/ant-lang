#include "chunk.h"
#include "../value/value.h"
#include "../memory/memory.h"
#include "../vm.h"

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
		int oldCapacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCapacity);
		chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
		chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
	}

	chunk->code[chunk->count] = value;
	chunk->lines[chunk->count] = line;
	chunk->count++;
}

int writeConstant(Chunk* chunk, Value value) {
	push(value);
	writeValueArray(&chunk->constants, value);
	pop();
	return chunk->constants.count - 1;
}

void freeChunk(Chunk* chunk) {
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	FREE_ARRAY(int, chunk->lines, chunk->capacity);
	freeValueArray(&chunk->constants);
	initChunk(chunk);
}
