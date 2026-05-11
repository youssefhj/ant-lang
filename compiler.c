#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "common.h"
#include "scanner.h"
#include "chunk.h"
#include "debug.h"


typedef enum {
	PREC_NONE,
	PREC_ASSIGNEMENT, // =
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

typedef void (*ParseFn)();

typedef struct {
	ParseFn prefixFn;
	ParseFn infixFn;
	Precedence precedence;
} PrecedenceRule;

typedef struct {
	Token previous;
	Token current;
	bool hadError;
} Parser;

static void binary();
static void number();
static void literal();

PrecedenceRule rules[] = {
	[TOKEN_PLUS]            = {NULL,        binary,        PREC_TERM},
	[TOKEN_MINUS]           = {NULL,        binary,        PREC_TERM},
	[TOKEN_STAR]            = {NULL,        binary,        PREC_FACTOR},
	[TOKEN_SLASH]           = {NULL,        binary,        PREC_FACTOR},
	[TOKEN_EQUAL]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_EQUAL_EQUAL]     = {NULL,        NULL,          PREC_NONE},
	[TOKEN_NOT]             = {NULL,        NULL,          PREC_NONE},
	[TOKEN_NOT_EQUAL]       = {NULL,        NULL,          PREC_NONE},
	[TOKEN_LESS]            = {NULL,        NULL,          PREC_NONE},
	[TOKEN_LESS_EQUAL]      = {NULL,        NULL,          PREC_NONE},
	[TOKEN_GREATER]         = {NULL,        NULL,          PREC_NONE},
	[TOKEN_GREATER_EQUAL]   = {NULL,        NULL,          PREC_NONE},
	[TOKEN_LEFT_PAREN]      = {NULL,        NULL,          PREC_NONE},
	[TOKEN_RIGHT_PAREN]     = {NULL,        NULL,          PREC_NONE},
	[TOKEN_LEFT_BRACE]      = {NULL,        NULL,          PREC_NONE},
	[TOKEN_RIGHT_BRACE]     = {NULL,        NULL,          PREC_NONE},
	[TOKEN_SEMICOLON]       = {NULL,        NULL,          PREC_NONE},
	[TOKEN_COMMA]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_DOT]             = {NULL,        NULL,          PREC_NONE},
	[TOKEN_IDENTIFIER]      = {NULL,        NULL,          PREC_NONE},
	[TOKEN_STRING]          = {NULL,        NULL,          PREC_NONE},
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
	[TOKEN_AND]             = {NULL,        NULL,          PREC_NONE},
	[TOKEN_OR]              = {NULL,        NULL,          PREC_NONE},
	[TOKEN_ERROR]           = {NULL,        NULL,          PREC_NONE},
	[TOKEN_EOF]             = {NULL,        NULL,          PREC_NONE}
};

Parser parser;
Chunk* currentChunk = NULL;

static void errorAt(Token* token, const char* message) {
	parser.hadError = true;

	fprintf(stderr, "[line %d] Error", token->line);
	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	} else if (token->type == TOKEN_ERROR) {
		// nothing	
	} else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s.\n", message);
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

	prefix();

	while (precedence <= getPrecedenceRule(parser.current.type)->precedence) {
		advance();
		ParseFn infix = getPrecedenceRule(parser.previous.type)->infixFn;

		infix();
	}
}

static void expression() {
	parsePrecedence(PREC_ASSIGNEMENT);
}

static void binary() {
	TokenType operatorType = parser.previous.type;

	Precedence precedence = getPrecedenceRule(operatorType)->precedence;
	parsePrecedence((Precedence)(precedence + 1));

	switch (operatorType) {
		case TOKEN_PLUS: emitByte(OP_ADD); break;
		case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
		case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
	}
}

static void number() {
	double number = strtod(parser.previous.start, NULL);
	emitConstant(makeConstant(NUMBER_VAL(number)));
}

static void literal() {
	switch (parser.previous.type) {
		case TOKEN_TRUE: emitByte(OP_TRUE); break;
		case TOKEN_FALSE: emitByte(OP_FALSE); break;
		case TOKEN_NIL:	emitByte(OP_NIL); break;
		default:
			// Unreachable
			return;
	}
}

static void expStmt() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' at end of expression");
}

static void declaration() {
	expStmt();
}

/*
 * program        -> declaration* EOF
 * declaration    -> expStmt
 * expStmt        -> expresion ';'
 * expression     -> logic_or
 * logic_or       -> logic_and ('or' logic_and)*
 * logic_and      -> equality ('and' equality)*
 * equality       -> comparison (('==' | '!=') comparison)*
 * comparison     -> term (('>' | '<' | '>=' | '<=') term)*
 * term           -> factor (('+' | '-') factor)*
 * factor         -> unary (('*' | '/') unary)*
 * unary          -> ('!' | '-') unary | primary
 * primary        -> 'true' | 'false' | 'nil' | IDENTIFIER | STRING | NUMBER | '(' expression ')'
 */
bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
	currentChunk = chunk;	
	advance();

	parser.hadError = false;

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	emitByte(OP_RETURN);

	#ifdef DEBUG_PRINT_CODE
	disassemble(currentChunk, "script");
	#endif

	return parser.hadError ? false : true;
}
