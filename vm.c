#include <stdio.h>

#include "vm.h"
#include "common.h"
#include "debug.h"
#include "compiler.h"

VM vm;

void initVM() {
	initChunk(&vm.chunk);
	vm.ip = NULL;
	vm.topStack = vm.stack;
}

void freeVM() {
	freeChunk(&vm.chunk);
}

//static void runtimeError(const char* error, ...) {

//}

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

static InterpretResult run() {
	#define READ_BYTE()             (*vm.ip++)
	#define READ_CONSTANT()         (vm.chunk.constants.values[READ_BYTE()])
        #define BINARY_OP(operator)                                           \
                do {                                                          \
                        Value b = pop();                                      \
                        Value a = pop();                                      \
                                                                              \
                        if (!IS_NUMBER(a) || !IS_NUMBER(b)) {                 \
                                return INTERPRET_RUNTIME_ERROR;               \
                        }                                                     \
                                                                              \
                        double result = AS_NUMBER(a) operator AS_NUMBER(b);   \
                                                                              \
                        push(NUMBER_VAL(result));                             \
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
			case OP_ADD:
				BINARY_OP(+);
				break;
			case OP_SUBTRACT:
				BINARY_OP(-);
				break;
			case OP_MULTIPLY:
				BINARY_OP(*);
				break;
			case OP_DIVIDE:
				BINARY_OP(/);
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
				push(BOOL_VAL(!AS_BOOL(pop())));
				break;
			case OP_CONSTANT:
				push(READ_CONSTANT());
				break;
			case OP_PRINT:
				printValue(pop());
				break;
			case OP_RETURN:
				return INTERPRET_SUCCESS;

		}
		
	}
	
	#undef READ_BYTE
	#undef READ_CONSTANT
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
