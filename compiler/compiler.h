#ifndef ANT_COMPILER_H
#define ANT_COMPILER_H

#include "../common.h"
#include "../vm/chunk/chunk.h"
#include "../vm/value/object/object.h"

ObjFunction* compile(const char* source);
void markCompilerRoots();

#endif // ANT_COMPILER_H
