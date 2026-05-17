#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "common.h"
#include "scanner.h"
#include "chunk.h"
#include "object.h"
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
} Parser;

static void binary(bool canAssign);
static void unary(bool canAssign);
static void grouping(bool canAssign);
static void number(bool canAssign);
static void literal(bool canAssign);
static void string(bool canAssign);
static void variable(bool canAssign);

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

static uint8_t makeIdentifierConstant(Token token) {
	return makeConstant(OBJ_VAL(copyString(token.start, token.length)));
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
	
	bool canAssign = precedence <= PREC_ASSIGNEMENT;
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

static void expression() {
	parsePrecedence(PREC_ASSIGNEMENT);
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

static void variable(bool canAssign) {
	uint8_t constant = makeIdentifierConstant(parser.previous);

	uint8_t op;
	if (canAssign && match(TOKEN_EQUAL)) {
		expression();
		op = OP_SET_GLOBAL;
	} else {
		op = OP_GET_GLOBAL;
	}

	emitBytes(op, constant);
}

static uint8_t parseVariable(Token token) {
	consume(TOKEN_IDENTIFIER, "Expect variable name");
	return makeIdentifierConstant(parser.previous);
}

static void defineVariable(uint8_t global) {
	emitBytes(OP_DEFINE_GLOBAL, global);
}

static void varDeclaration() {
	uint8_t global = parseVariable(parser.previous);
	if (match(TOKEN_EQUAL)) {
		expression();
	} else {
		emitByte(OP_NIL);		
	}
	
	defineVariable(global);
	consume(TOKEN_SEMICOLON, "Expect ';' at end of expression");
}

static void printStmt() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' at end of expression");
	emitByte(OP_PRINT);
}

static void exprStmt() {
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' at end of expression");
}

static void statement() {
	if (match(TOKEN_PRINT)) {
		printStmt();	
	} else {
		exprStmt();
	}
}

static void declaration() {
	if (match(TOKEN_VAR)) {
		varDeclaration();
	} else {
		statement();
	}
}

/*
 * program        -> declaration* EOF
 * declaration    -> varDeclaration | statement
 * varDeclaration -> 'var' IDENTIFIER '=' expression ';'
 * statement      -> printStmt | exprStmt
 * printStmt      -> 'print' expression ';'
 * exprStmt       -> expression ';'
 * expression     -> assignement
 * assignement    -> IDENTIFIER '=' assignement | logic_or
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
