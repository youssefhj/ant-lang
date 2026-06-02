#include <stdio.h>

#include "debug.h"


static int constantInstruction(const char* opcode, Chunk* chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1];
	printf("%-16s %4d '", opcode, constant);
	printValue(chunk->constants.values[constant]);
	printf("'\n");
	return offset + 2;
}

static int byteInstruction(const char* opcode, Chunk* chunk, int offset) {
	uint8_t slot = chunk->code[offset + 1];
	printf("%-16s %4d\n", opcode, slot);
	return offset + 2;
}

static int simpleInstruction(const char* opcode, int offset) {
	printf("%-16s\n", opcode);
	return offset + 1;
}

int disassembleInstruction(Chunk* chunk, int offset) {
	printf("%04d  ", offset);
	
	int line = chunk->lines[offset];
	if (offset > 0 && line == chunk->lines[offset - 1]) {
		printf("   | ");
	} else {
		printf("%4d ", line);
	}

	uint8_t instruction = chunk->code[offset];
	switch (instruction) {
		case OP_ADD:
			return simpleInstruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return simpleInstruction("OP_SUBTRACT", offset);
		case OP_MULTIPLY:
			return simpleInstruction("OP_MULTIPLY", offset);
		case OP_DIVIDE:
			return simpleInstruction("OP_DIVIDE", offset);
		case OP_TRUE:
			return simpleInstruction("OP_TRUE", offset);
		case OP_FALSE:
			return simpleInstruction("OP_FALSE", offset);
		case OP_NIL:
			return simpleInstruction("OP_NIL", offset);
		case OP_EQUAL:
			return simpleInstruction("OP_EQUAL", offset);
		case OP_NOT:
			return simpleInstruction("OP_NOT", offset);
		case OP_LESS:
			return simpleInstruction("OP_LESS", offset);
		case OP_GREATER: 
			return simpleInstruction("OP_GREATER", offset);
		case OP_NEGATE:
			return simpleInstruction("OP_NEGATE", offset);
		case OP_POP:
			return simpleInstruction("OP_POP", offset);
		case OP_DEFINE_GLOBAL:
			return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
		case OP_GET_GLOBAL:
			return byteInstruction("OP_GET_GLOBAL", chunk, offset);
		case OP_SET_GLOBAL:
			return byteInstruction("OP_SET_GLOBAL", chunk, offset);
		case OP_GET_LOCAL:
			return byteInstruction("OP_GET_LOCAL", chunk, offset);
		case OP_SET_LOCAL:
			return byteInstruction("OP_SET_LOCAL", chunk, offset);
		case OP_CONSTANT:
			return constantInstruction("OP_CONSTANT", chunk, offset);
		case OP_PRINT:
			return simpleInstruction("OP_PRINT", offset);
		case OP_RETURN:
			return simpleInstruction("OP_RETURN", offset);	
	}

	printf("Unknown instruction\n");
	return offset + 1;

}

void disassemble(Chunk* chunk, const char* title) {
	printf("=== %s ===\n", title);
	for (int offset = 0; offset < chunk->count;) {
		offset = disassembleInstruction(chunk, offset);
	}


}
