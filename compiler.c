#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "common.h"
#include "scanner.h"
#include "chunk.h"
#include "object.h"
#include "debug.h"


typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT,  // =
	PREC_OR,          // or
	PREC_AND,         // and
	PREC_EQUALITY,    // == !=
	PREC_COMPARISON,  // > < >= <=
	PREC_TERM,        // + -
	PREC_FACTOR,      // * /
	PREC_UNARY,       // ! -
	PREC_CALL,        // . ()
	PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool);

typedef struct {
	ParseFn prefixFn;
	ParseFn infixFn;
	Precedence precedence;
} PrecedenceRule;

typedef struct {
	Token previous;
	Token current;
	bool hadError;
	bool panicMode;
} Parser;

typedef struct {
	Token name;
	int scopeDepth;
} Local;

typedef struct {
	Local locals[UINT8_COUNT];
	int localsCount;
	int currentScope;
} Compiler;

static void binary(bool canAssign);
static void unary(bool canAssign);
static void grouping(bool canAssign);
static void number(bool canAssign);
static void literal(bool canAssign);
static void string(bool canAssign);
static void variable(bool canAssign);
static void and_(bool canAssign);
static void or_(bool canAssign);

PrecedenceRule rules[] = {
	[TOKEN_PLUS]            = {NULL,        binary,        PREC_TERM},
	[TOKEN_MINUS]           = {unary,       binary,        PREC_TERM},
	[TOKEN_STAR]            = {NULL,        binary,        PREC_FACTOR},
	[TOKEN_SLASH]           = {NULL,        binary,        PREC_FACTOR},
	[TOKEN_EQUAL]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_EQUAL_EQUAL]     = {NULL,        binary,        PREC_EQUALITY},
	[TOKEN_NOT]             = {unary,       NULL,          PREC_NONE},
	[TOKEN_NOT_EQUAL]       = {NULL,        binary,        PREC_EQUALITY},
	[TOKEN_LESS]            = {NULL,        binary,        PREC_COMPARISON},
	[TOKEN_LESS_EQUAL]      = {NULL,        binary,        PREC_COMPARISON},
	[TOKEN_GREATER]         = {NULL,        binary,        PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL]   = {NULL,        binary,        PREC_COMPARISON},
	[TOKEN_LEFT_PAREN]      = {grouping,    NULL,          PREC_NONE},
	[TOKEN_RIGHT_PAREN]     = {NULL,        NULL,          PREC_NONE},
	[TOKEN_LEFT_BRACE]      = {NULL,        NULL,          PREC_NONE},
	[TOKEN_RIGHT_BRACE]     = {NULL,        NULL,          PREC_NONE},
	[TOKEN_SEMICOLON]       = {NULL,        NULL,          PREC_NONE},
	[TOKEN_COMMA]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_DOT]             = {NULL,        NULL,          PREC_NONE},
	[TOKEN_IDENTIFIER]      = {variable,    NULL,          PREC_NONE},
	[TOKEN_STRING]          = {string,      NULL,          PREC_NONE},
	[TOKEN_NUMBER]          = {number,      NULL,          PREC_NONE},
	[TOKEN_VAR]             = {NULL,        NULL,          PREC_NONE},
	[TOKEN_PRINT]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_IF]              = {NULL,        NULL,          PREC_NONE},
	[TOKEN_ELSE]            = {NULL,        NULL,          PREC_NONE},
	[TOKEN_WHILE]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_FOR]             = {NULL,        NULL,          PREC_NONE},
	[TOKEN_FUNC]            = {NULL,        NULL,          PREC_NONE},
	[TOKEN_CLASS]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_THIS]            = {NULL,        NULL,          PREC_NONE},
	[TOKEN_SUPER]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_NIL]             = {literal,     NULL,          PREC_NONE},
	[TOKEN_RETURN]          = {NULL,        NULL,          PREC_NONE},
	[TOKEN_TRUE]            = {literal,     NULL,          PREC_NONE},
	[TOKEN_FALSE]           = {literal,     NULL,          PREC_NONE},
	[TOKEN_AND]             = {NULL,        and_,          PREC_AND},
	[TOKEN_OR]              = {NULL,        or_,          PREC_OR},
	[TOKEN_ERROR]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_EOF]             = {NULL,        NULL,          PREC_NONE}
};

Parser parser;
Chunk* currentChunk = NULL;
Compiler* currentCompiler = NULL;

static void errorAt(Token* token, const char* message) {
	if (parser.panicMode) return;
	parser.panicMode = true;

	fprintf(stderr, "[line %d] Error", token->line);
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
		// nothing	
	} else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s.\n", message);
	parser.hadError = true;
}

static void errorAtCurrent(const char* message) {
	errorAt(&parser.current, message);
}

static void error(const char* message) {
	errorAt(&parser.previous, message);
}

static void advance() {
	parser.previous = parser.current;
	for (;;) {
		parser.current = scanToken();
		
		if (parser.current.type != TOKEN_ERROR) {
			break;
		}
		
		errorAtCurrent(parser.current.start);
	}
}

static bool check(TokenType type) {
	return parser.current.type == type;
}

static bool match(TokenType type) {
	if (!check(type)) return false;
	
	advance();
	return true;
}

static void consume(TokenType type, const char* message) {
	if (parser.current.type == type) {
		advance();
		return;
	}

	errorAtCurrent(message);
}

static Chunk* getCurrentChunk() {
	return currentChunk;
}

static void emitByte(uint8_t byte) {
	writeChunk(getCurrentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitConstant(uint8_t constant) {
	emitBytes(OP_CONSTANT, constant);
}

static uint8_t makeConstant(Value value) {
	int constant = writeConstant(getCurrentChunk(), value);
	if (constant > UINT8_MAX) {
		fprintf(stderr, "Error: Too many constant in a chunk.");
		return 0;
	}

	return (uint8_t)constant;
}

static uint8_t makeIdentifierConstant(Token token) {
	return makeConstant(OBJ_VAL(copyString(token.start, token.length)));
}

static bool identifierEquals(Token* id1, Token* id2) {
	if (id1->length != id2->length) return false;
	return memcmp(id1->start, id2->start, id1->length) == 0;
}

static PrecedenceRule* getPrecedenceRule(TokenType type) {
	return &rules[type];
}

static void parsePrecedence(Precedence precedence) {
	advance();

	ParseFn prefix = getPrecedenceRule(parser.previous.type)->prefixFn;
	if (prefix == NULL) {
		error("Expecting expression");
		return;
	}
	
	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefix(canAssign); 

	while (precedence <= getPrecedenceRule(parser.current.type)->precedence) {
		advance();
		ParseFn infix = getPrecedenceRule(parser.previous.type)->infixFn;

		infix(canAssign);
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid assignement target");
		return;
	}
}

static void initCompiler(Compiler* compiler) {
	compiler->localsCount = 0;
	compiler->currentScope = 0;

	currentCompiler = compiler;
}

static void expression() {
	parsePrecedence(PREC_ASSIGNMENT);
}

static void binary(bool canAssign) {
	TokenType operatorType = parser.previous.type;

	Precedence precedence = getPrecedenceRule(operatorType)->precedence;
	parsePrecedence((Precedence)(precedence + 1));

	switch (operatorType) {
		case TOKEN_PLUS: emitByte(OP_ADD); break;
		case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
		case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
		case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
		case TOKEN_NOT_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
		case TOKEN_LESS: emitByte(OP_LESS); break;
		case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
		case TOKEN_GREATER: emitByte(OP_GREATER); break;
		case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
		default:
		        // Unreachable
			return;
	}
}

static void unary(bool canAssign) {
	TokenType operatorType = parser.previous.type;

	parsePrecedence(PREC_UNARY);

	switch (operatorType) {
		case TOKEN_NOT: emitByte(OP_NOT); break;
		case TOKEN_MINUS: emitByte(OP_NEGATE); break;
		default:
		        // Unreachable
			return;
	}
}

static void grouping(bool canAssign) {
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')'");
}

static void number(bool canAssign) {
	double number = strtod(parser.previous.start, NULL);
	emitConstant(makeConstant(NUMBER_VAL(number)));
}

static void literal(bool canAssign) {
	switch (parser.previous.type) {
		case TOKEN_TRUE: emitByte(OP_TRUE); break;
		case TOKEN_FALSE: emitByte(OP_FALSE); break;
		case TOKEN_NIL:	emitByte(OP_NIL); break;
		default:
			// Unreachable
			return;
	}
}

static void string(bool canAssign) {
	emitConstant(makeConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2))));
}

static void statement();
static void declaration();

static int resolveLocal(Token* name) {
	if (currentCompiler->currentScope <= 0) return -1;

	for (int i = currentCompiler->localsCount - 1; i >= 0; i--) {
		Local* local = &currentCompiler->locals[i];

		if (identifierEquals(&local->name, name)) {
			if (local->scopeDepth == -1) {
				error("Can't read local variable");
			}

			return i;
		}
	}
	
	return -1;
}

static void variable(bool canAssign) {
	uint8_t opSet, opGet;
	int constant = resolveLocal(&parser.previous);

	if (constant == -1) {
		constant = makeIdentifierConstant(parser.previous);
		
		opSet = OP_SET_GLOBAL;
		opGet = OP_GET_GLOBAL;
	} else {
		opSet = OP_SET_LOCAL;
		opGet = OP_GET_LOCAL;
	}
	
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();	
		emitBytes(opSet, (uint8_t)constant);
	} else {
		emitBytes(opGet, (uint8_t)constant);
	}
}

static void addLocal(Token name) {
	if (currentCompiler->localsCount == UINT8_COUNT) {
		error("Too many local variables");
		return;
	}

	Local* local = &currentCompiler->locals[currentCompiler->localsCount++];
	local->name = name;
	local->scopeDepth = -1;
}

static void declareVariable() {
	if (currentCompiler->currentScope <= 0) return;
	
	Token* name = &parser.previous;
	for (int i = currentCompiler->localsCount - 1; i >= 0; i--) {
		Local* local = &currentCompiler->locals[i];

		if (local->scopeDepth != -1 && local->scopeDepth < currentCompiler->currentScope) {		
			break;
		} else if (identifierEquals(name, &local->name)) {
			error("Variable has already been declared in this scope");
			return;
		}
	}
	
	addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
	consume(TOKEN_IDENTIFIER, errorMessage);
	
	declareVariable();
	if (currentCompiler->currentScope > 0) return 0;

	return makeIdentifierConstant(parser.previous);
}

static void markInitialized() {
	if (currentCompiler->currentScope <= 0) return;
	currentCompiler->locals[currentCompiler->localsCount - 1].scopeDepth = currentCompiler->currentScope;
}

static void defineVariable(uint8_t global) {
	if (currentCompiler->currentScope > 0) {
		// Local variables are implicitly defined
		markInitialized();
		return;
	}

	emitBytes(OP_DEFINE_GLOBAL, global);
}

static void varDeclaration() {
	uint8_t global = parseVariable("Expect variable name");
	if (match(TOKEN_EQUAL)) {
		expression();
	} else {
		emitByte(OP_NIL);		
	}
	
	defineVariable(global);
	consume(TOKEN_SEMICOLON, "Expect ';'");
}

static void printStmt() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';'");
	emitByte(OP_PRINT);
}

static void beginScope() {
	currentCompiler->currentScope++;
}

static void block() {
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}'");
}

static void endScope() {
	currentCompiler->currentScope--;

	while (currentCompiler->localsCount > 0 && 
			currentCompiler->locals[currentCompiler->localsCount - 1].scopeDepth > currentCompiler->currentScope) {
		currentCompiler->localsCount--;
		emitByte(OP_POP);
	}
}

static int emitJump(uint8_t byte) {
	emitByte(byte);
	emitBytes((uint8_t)0xff, (uint8_t)0xff);

	return getCurrentChunk()->count - 2;
}

static void jumpBackPatching(int offset) {
	int jumpOffset = getCurrentChunk()->count - offset - 2;

	if (jumpOffset > UINT16_MAX) {
		error("Too many code to jump over");
		return;
	}

	getCurrentChunk()->code[offset] = (jumpOffset >> 8) & 0xff;
	getCurrentChunk()->code[offset + 1] = jumpOffset & 0xff;
}

static void and_(bool canAssign) {
	// short circuit
	int jumpOffset = emitJump(OP_JUMP_IF_FALSE);
	
	emitByte(OP_POP);

	parsePrecedence(PREC_AND + 1);
	
	jumpBackPatching(jumpOffset);
}

static void or_(bool canAssign) {
	// short circuit
	int jumpIfFalseOffset = emitJump(OP_JUMP_IF_FALSE);
	int jumpOffset = emitJump(OP_JUMP);

	jumpBackPatching(jumpIfFalseOffset);
	emitByte(OP_POP);	
	
	parsePrecedence(PREC_OR + 1);

	jumpBackPatching(jumpOffset);
}

static void ifStmt() {
	consume(TOKEN_LEFT_PAREN, "Expect '(' after if statement");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after if condition");

	int ifOffset = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);

	// if block
	statement();	

	int elseOffset = emitJump(OP_JUMP);
	
	jumpBackPatching(ifOffset);
	emitByte(OP_POP);

	// else block
	if (match(TOKEN_ELSE)) statement();

	jumpBackPatching(elseOffset);
}

static void emitLoop(int loopStart) {
	emitByte(OP_LOOP);

	int offset = getCurrentChunk()->count + 2 - loopStart;

	if (offset > UINT16_MAX) {
		error("Loop body is too large");
	}

	emitByte((offset >> 8) & 0xff);
	emitByte(offset & 0xff);
}

static void whileStmt() {
	int loopStart = getCurrentChunk()->count;

	consume(TOKEN_LEFT_PAREN, "Expect '(' after while statement");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after while condition");

	int jumpOffset = emitJump(OP_JUMP_IF_FALSE);

	emitByte(OP_POP);	
	statement();	

	emitLoop(loopStart);
	
	jumpBackPatching(jumpOffset);
	emitByte(OP_POP);
}

static void exprStmt() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';'");
	emitByte(OP_POP);
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		printStmt();
	} else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	} else if (match(TOKEN_IF)) {
		ifStmt();
	} else if (match(TOKEN_WHILE)) {
		whileStmt();	
	} else {
		exprStmt();
	}
}

static void synchronize() {
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		
		if (parser.previous.type == TOKEN_SEMICOLON) return;

		switch (parser.current.type) {
			case TOKEN_VAR:
			case TOKEN_PRINT:
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_FOR:
			case TOKEN_FUNC:
			case TOKEN_CLASS:
			case TOKEN_RETURN:
				return;
		}

		advance();
	}
}

static void declaration() {
	if (match(TOKEN_VAR)) {
		varDeclaration();
	} else {
		statement();
	}

	if (parser.panicMode) {
		synchronize();
	}
}

/*
 * program        -> declaration* EOF
 * declaration    -> varDeclaration | statement
 * varDeclaration -> "var" IDENTIFIER "=" expression ";"
 * statement      -> printStmt | exprStmt | block | ifStmt | whileStmt
 * printStmt      -> "print" expression ";"
 * exprStmt       -> expression ";"
 * block          -> "{" declaration* "}"
 * ifStmt         -> "if" "(" expression ")" statement ("else" statement)?
 * whileStmt      -> "while" "(" expression ")" statement
 * expression     -> assignment
 * assignment     -> IDENTIFIER "=" assignment | logic_or
 * logic_or       -> logic_and ("or" logic_and)*
 * logic_and      -> equality ("and" equality)*
 * equality       -> comparison (("==" | "!=") comparison)*
 * comparison     -> term ((">" | "<" | ">=" | "<=") term)*
 * term           -> factor (("+" | "-") factor)*
 * factor         -> unary (("*" | "/") unary)*
 * unary          -> ("!" | "-") unary | primary
 * primary        -> "true" | "false" | "nil" | IDENTIFIER | STRING | NUMBER | "(" expression ")"
 */
bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
	currentChunk = chunk;	
	advance();

	parser.hadError = false;
	parser.panicMode = false;

	Compiler compiler;
	initCompiler(&compiler);

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	emitByte(OP_RETURN);

	#ifdef DEBUG_PRINT_CODE
	disassemble(currentChunk, "script");
	#endif

	return parser.hadError ? false : true;
}
