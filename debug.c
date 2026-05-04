#include <stdio.h>

#include "debug.h"


static int constantInstruction(const char* opcode, Chunk* chunk, int offset) {
	uint8_t constant = chunk->code[offset + 1];
	printf("%-16s %4d '", opcode, constant);
	printValue(chunk->constants.values[constant]);
	printf("'\n");
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
