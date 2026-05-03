#ifndef ANT_DEBUG_H
#define ANT_DEBUG_H

#include "chunk.h"

int disassembleInstruction(Chunk* chunk, int slot);
void disassemble(Chunk* chunk, const char* title);

#endif // ANT_DEBUG_H
