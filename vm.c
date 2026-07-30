#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "object.h"
#include "debug.h"
#include "memory.h"

VM vm;

static Value clockNative(int argCount, Value* args) {
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void defineNative(const char* name, NativeFn function) {
	ObjString* key = copyString(name, (int)strlen(name));
	ObjNative* native = newNative(function);
	
	tableSet(&vm.globals, key, OBJ_VAL(native));
}

static void resetStack() {
	vm.frameCount = 0;
	vm.topStack = vm.stack;
	vm.objects = NULL;
	vm.openUpvalues = NULL;
}

void initVM() {
	resetStack();
	initTable(&vm.globals);
	initTable(&vm.strings);

	defineNative("clock", clockNative);
}

void freeVM() {
	freeTable(&vm.globals);
	freeTable(&vm.strings);
	freeObjects();
}

static void runtimeError(const char* format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs(".\n", stderr);

	for (int i = vm.frameCount - 1; i >= 0; i--) {
		CallFrame* frame = &vm.frames[i];

		ObjFunction* function = frame->closure->function;

		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

		if (function->name == NULL) {
			fprintf(stderr, "script\n");
		} else {
			fprintf(stderr, "%s()\n", function->name->chars);
		}
	}

	resetStack();
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

static bool call(ObjClosure* closure, uint8_t argCount) {
	if (closure->function->arity != argCount) {
		runtimeError("Expect %d arguments got %d", closure->function->arity, argCount);
		return false;
	}

	if (vm.frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow");
		return false;
	}

	CallFrame* frame = &vm.frames[vm.frameCount++];
	frame->closure = closure;
	frame->ip = closure->function->chunk.code;
	frame->slots = vm.topStack - argCount - 1; 
					
	return true;	
}

static bool callValue(Value callee, uint8_t argCount) {
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
			case OBJ_CLOSURE: 
				return call(AS_CLOSURE(callee), argCount);
			case OBJ_NATIVE: {
				NativeFn native = AS_NATIVE(callee);
				Value result = native(argCount, vm.topStack - argCount);
				vm.topStack = vm.topStack - argCount - 1;
				push(result);
				return true;
			}
			case OBJ_CLASS: {
				ObjInstance* instance = newInstance(AS_CLASS(pop()));
				push(OBJ_VAL(instance));
				return true;
			}
		}
	}

	runtimeError("Can only call functions and classes");
	return false;
}

static ObjUpvalue* captureUpvalue(Value* local) {
	ObjUpvalue* prevUpvalue = NULL;
	ObjUpvalue* upvalue = vm.openUpvalues;
						
	while (upvalue != NULL && upvalue->location > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}
						
	if (upvalue != NULL && upvalue->location == local) {
		return upvalue;
	}

	ObjUpvalue* createdUpvalue = newUpvalue(local);
	createdUpvalue->next = upvalue;
						
	if (prevUpvalue == NULL) {
		vm.openUpvalues = createdUpvalue;
	} else {
		prevUpvalue->next = createdUpvalue;
	}
	
	return createdUpvalue;
}

static void closeUpvalues(Value* last) {
	while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
		ObjUpvalue* upvalue = vm.openUpvalues;
		upvalue->closed = *upvalue->location;
		upvalue->location = &upvalue->closed;
		
		vm.openUpvalues = upvalue->next;
	}

}

static void defineMethod(ObjString* name) {
	Value method = peek(0);
	ObjClass* klass = AS_CLASS(peek(1));
	tableSet(&klass->methods, name, method);
	pop();
}

static bool invokeFromClass(ObjClass* klass, ObjString* name, uint8_t argCount) {
	Value value;
	if (!tableGet(&klass->methods, name, &value)) {
		runtimeError("Undefined property '%s'", name->chars);
		return false;
	}

	vm.topStack[-argCount - 1] = value;
	return callValue(value, argCount);
}

static bool invoke(ObjString* name, uint8_t argCount) {
	Value receiver = peek(argCount); 
	if (!IS_INSTANCE(receiver)) {
		runtimeError("Only instances have methods");
		return false;
	}

	ObjInstance* instance = AS_INSTANCE(receiver);	
	Value value;
	if (tableGet(&instance->fields, name, &value)) {
		vm.topStack[-argCount - 1] = value;
		return callValue(value, argCount);
	}

	return invokeFromClass(instance->klass, name, argCount);
}

static InterpretResult run() {
	CallFrame* frame = &vm.frames[vm.frameCount - 1];

	#define READ_BYTE()             (*frame->ip++)
	#define READ_SHORT()            (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8 | frame->ip[-1])))
	#define READ_CONSTANT()         (frame->closure->function->chunk.constants.values[READ_BYTE()])
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
		disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
			case OP_CLOSURE: {
				ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
				ObjClosure* closure = newClosure(function);
				push(OBJ_VAL(closure));

				for (int i = 0; i < closure->upvalueCount; i++) {
					uint8_t isLocal = READ_BYTE();
					uint8_t index = READ_BYTE();
					
					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(frame->slots + index);	
					} else {
						closure->upvalues[i] = frame->closure->upvalues[index];
					}
				}

				break;
			} 
			case OP_GET_UPVALUE: 
				push(*frame->closure->upvalues[READ_BYTE()]->location);
				break;
			case OP_SET_UPVALUE: 
				*frame->closure->upvalues[READ_BYTE()]->location = peek(0);
				break;
			case OP_CLASS:
				push(OBJ_VAL(newClass(READ_STRING())));
				break;
			case OP_METHOD:
				defineMethod(READ_STRING());
				break;
			case OP_GET_PROPERTY: {
				if (!IS_INSTANCE(peek(0))) {
					runtimeError("Only instances have properties");
					return INTERPRET_RUNTIME_ERROR;	
				}
				
				ObjInstance* instance = AS_INSTANCE(peek(0));
				ObjString* name = READ_STRING();

				Value value;

				if (tableGet(&instance->fields, name, &value)) {
					pop();
					push(value);
					break;
				}

				if (tableGet(&instance->klass->methods, name, &value)) {
					pop();
					push(value);
					break;
				}
				
				runtimeError("Undefined property '%s'", name->chars);
				return INTERPRET_RUNTIME_ERROR;
			}
			case OP_SET_PROPERTY: {
				if (!IS_INSTANCE(peek(1))) {
					runtimeError("Only instances have fields");
					return INTERPRET_RUNTIME_ERROR;
				}
				ObjInstance* instance = AS_INSTANCE(peek(1));
				tableSet(&instance->fields, READ_STRING(), peek(0));

				Value value = pop();
				pop();
				push(value);
				break;
			}
			case OP_INVOKE: {
				ObjString* method = READ_STRING();
				int argCount = READ_BYTE();

				if (!invoke(method, argCount)) {
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
				Value returnValue = pop();
				vm.frameCount--;
		
				closeUpvalues(frame->slots);

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
	ObjFunction* function = compile(source);
	if (function == NULL) {
		return INTERPRET_COMPILETIME_ERROR;
	}

	ObjClosure* closure = newClosure(function);
	push(OBJ_VAL(closure));
	callValue(OBJ_VAL(closure), 0);

	InterpretResult result = run();
	
	return result;
}
