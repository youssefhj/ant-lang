#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "debug.h"
#include "memory.h"

VM vm;

void initVM() {
	vm.frameCount = 0;
	vm.topStack = vm.stack;
	vm.objects = NULL;
	initTable(&vm.globals);
	initTable(&vm.strings);
}

void freeVM() {
	freeObjects();
}

static void runtimeError(const char* format, ...) {
	va_list args;
	CallFrame* frame = &vm.frames[vm.frameCount - 1];
	size_t instruction = frame->ip - frame->function->chunk.code - 1;

	va_start(args, format);
	fprintf(stderr, "[line %d] Error: ", frame->function->chunk.lines[instruction]);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs(".\n", stderr);
}

static void push(Value value) {
	vm.topStack++;
	vm.topStack[-1] = value;
}

static Value pop() {
	vm.topStack--;
	return *vm.topStack;
}

static Value peek(int depth) {
	return vm.topStack[-depth - 1];
}

static bool isFalsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
	ObjString* stringA = AS_STRING(peek(1));
	ObjString* stringB = AS_STRING(peek(0));
	int totalLength = stringA->length + stringB->length;
	
	char* chars = ALLOCATE(char, totalLength + 1);

	memcpy(chars, stringA->chars, stringA->length);
	memcpy(chars + stringA->length, stringB->chars, stringB->length);

	chars[totalLength] = '\0';
	ObjString* concatenatedString = takeString(chars, totalLength);

	pop();
	pop();
	push(OBJ_VAL(concatenatedString));
}

static bool call(ObjFunction* function, uint8_t argCount) {
	if (function->arity != argCount) {
		runtimeError("Expect %d arguments got %d", function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow");
		return false;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;
	frame->slots = vm.topStack - argCount - 1; 
					
	return true;	
}

static bool callValue(Value value, uint8_t argCount) {
	if (IS_OBJ(value)) {
		switch (OBJ_TYPE(value)) {
			case OBJ_FUNCTION: 
				return call(AS_FUNCTION(value), argCount);
		}
	}

	runtimeError("Can only call functions and classes");
	return false;
}

static InterpretResult run() {
	CallFrame* frame = &vm.frames[vm.frameCount - 1];

	#define READ_BYTE()             (*frame->ip++)
	#define READ_SHORT()            (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8 | frame->ip[-1])))
	#define READ_CONSTANT()         (frame->function->chunk.constants.values[READ_BYTE()])
	#define READ_STRING()           (AS_STRING(READ_CONSTANT()))
        #define BINARY_OP(valueType, operator)                                \
                do {                                                          \
                        if (!IS_NUMBER(peek(1)) || !IS_NUMBER(peek(0))) {     \
                                runtimeError("Operands must be numbers");     \
                                return INTERPRET_RUNTIME_ERROR;               \
                        }                                                     \
                                                                              \
                        double b = AS_NUMBER(pop());                          \
                        double a = AS_NUMBER(pop());                          \
                                                                              \
                        push(valueType(a operator b));                        \
                } while (false);
	
	uint8_t instruction;
	for (;;) {
		#ifdef DEBUG_STACK_TRACE
		printf("           ");
		for (Value* slot = vm.stack; slot < vm.topStack; slot++) {
			printf("[ ");
			printValue(*slot);
			printf(" ]");
		}
		printf("\n");
		disassembleInstruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
		#endif
		switch (instruction = READ_BYTE()) {
			case OP_ADD: {
				if (IS_STRING(peek(1)) && IS_STRING(peek(0))) {
					concatenate();
				} else if (IS_NUMBER(peek(1)) && IS_NUMBER(peek(0))) {
					BINARY_OP(NUMBER_VAL, +);
				} else {
					runtimeError("Operands must be numbers or strings");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SUBTRACT:
				BINARY_OP(NUMBER_VAL, -);
				break;
			case OP_MULTIPLY:
				BINARY_OP(NUMBER_VAL, *);
				break;
			case OP_DIVIDE:
				BINARY_OP(NUMBER_VAL, /);
				break;
			case OP_TRUE:
				push(BOOL_VAL(true));
				break;
			case OP_FALSE:
				push(BOOL_VAL(false));
				break;
			case OP_NIL:
				push(NIL_VAL);
				break;
			case OP_EQUAL:
				push(BOOL_VAL(valuesEqual(pop(), pop())));
				break;
			case OP_NOT:
				push(BOOL_VAL(isFalsey(pop())));
				break;
			case OP_LESS:
				BINARY_OP(BOOL_VAL, <);
				break;
			case OP_GREATER:
				BINARY_OP(BOOL_VAL, >);
				break;
			case OP_NEGATE:
				if (!IS_NUMBER(peek(0))) {
					runtimeError("Operand must be number");
					return INTERPRET_RUNTIME_ERROR;
				}
				push(NUMBER_VAL(-AS_NUMBER(pop())));
				break;
			case OP_POP:
				pop();
				break;
			case OP_DEFINE_GLOBAL: {
				tableSet(&vm.globals, READ_STRING(), peek(0));
				pop();
				break;
			}
			case OP_GET_GLOBAL: {
				Value value;
				ObjString* variable = READ_STRING();
				if (!tableGet(&vm.globals, variable, &value)) {
					runtimeError("Undefined variable '%s'", variable->chars);
					return INTERPRET_RUNTIME_ERROR;
				}

				push(value);
				break;
			}
			case OP_SET_GLOBAL: {
				ObjString* variable = READ_STRING();
				if (tableSet(&vm.globals, variable, peek(0))) {
					tableDelete(&vm.globals, variable);
					runtimeError("Undeclared variable '%s'", variable->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_GET_LOCAL:
				push(frame->slots[READ_BYTE()]);
				break;
			case OP_SET_LOCAL:
				frame->slots[READ_BYTE()] = peek(0);
				break;
			case OP_JUMP_IF_FALSE: {
				uint16_t offset = READ_SHORT();
				if (isFalsey(peek(0))) {
					frame->ip += offset;
				}
				break;
			}
			case OP_JUMP: {
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}
			case OP_LOOP: {
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;
				break;
			}
			case OP_CALL: {
				uint8_t argCount = READ_BYTE();
				
				if (!callValue(peek(argCount), argCount)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				
				frame = &vm.frames[vm.frameCount - 1];
				break;
			} 
			case OP_CONSTANT:
				push(READ_CONSTANT());
				break;
			case OP_PRINT:
				printValue(pop());
				printf("\n");
				break;
			case OP_RETURN:
				Value returnValue = peek(0);
				vm.frameCount--;

				if (vm.frameCount == 0) {
					pop();
					return INTERPRET_SUCCESS;
				}

				vm.topStack = frame->slots;
				push(returnValue);

				frame = &vm.frames[vm.frameCount - 1];

				break;
		}
		
	}
	
	#undef READ_BYTE
	#undef READ_SHORT
	#undef READ_CONSTANT
	#undef READ_STRING
	#undef BINARY_OP

}

InterpretResult interpret(const char* source) {
	initVM();

	ObjFunction* function = compile(source);
	if (function == NULL) {
		return INTERPRET_COMPILETIME_ERROR;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;
	frame->slots = vm.stack;

	push(OBJ_VAL(function));
	InterpretResult result = run();
	
	freeVM();
	return result;
}
