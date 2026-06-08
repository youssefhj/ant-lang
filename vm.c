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
	initChunk(&vm.chunk);
	vm.ip = NULL;
	vm.topStack = vm.stack;
	vm.objects = NULL;
	initTable(&vm.globals);
	initTable(&vm.strings);
}

void freeVM() {
	freeChunk(&vm.chunk);
	freeObjects();
}

static void runtimeError(const char* format, ...) {
	va_list args;
	size_t instruction = vm.ip - vm.chunk.code - 1;
	
	va_start(args, format);
	fprintf(stderr, "[line %d] Error: ", vm.chunk.lines[instruction]);
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

static InterpretResult run() {
	#define READ_BYTE()             (*vm.ip++)
	#define READ_SHORT()            (vm.ip += 2, (uint16_t) ((vm.ip[-2] << 8 | vm.ip[-1])))
	#define READ_CONSTANT()         (vm.chunk.constants.values[READ_BYTE()])
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
		disassembleInstruction(&vm.chunk, (int)(vm.ip - vm.chunk.code));
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
				push(vm.stack[READ_BYTE()]);
				break;
			case OP_SET_LOCAL:
				vm.stack[READ_BYTE()] = peek(0);
				break;
			case OP_JUMP_IF_FALSE: {
				uint16_t offset = READ_SHORT();
				if (isFalsey(peek(0))) {
					vm.ip += offset;
				}
				break;
			}
			case OP_JUMP: {
				uint16_t offset = READ_SHORT();
				vm.ip += offset;
				break;
			}
			case OP_LOOP: {
				uint16_t offset = READ_SHORT();
				vm.ip -= offset;
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
				return INTERPRET_SUCCESS;

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

	if (!compile(source, &vm.chunk)) {
		return INTERPRET_COMPILETIME_ERROR;
	}

	vm.ip = vm.chunk.code;
	InterpretResult result = run();
	
	freeVM();
	return result;
}
