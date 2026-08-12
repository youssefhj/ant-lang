#ifndef ANT_COMPILER_H
#define ANT_COMPILER_H

#include "common.h"
#include "chunk.h"
#include "object.h"

ObjFunction* compile(const char* source);
void markCompilerRoots();

#endif // ANT_COMPILER_H
