#include <string.h>

#include "common.h"
#include "scanner.h"


Scanner scanner;

void initScanner(const char* source) {
	scanner.current = source;
	scanner.start = source;
	scanner.line = 1;
}

static Token errorToken(const char* message) {
	Token token;
	
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = scanner.line;

	return token;
}

static Token createToken(TokenType type) {
	Token token;

	token.type = type;
	token.start = scanner.start;
	token.length = (int) (scanner.current - scanner.start);
	token.line = scanner.line;

	return token;
}

static bool isAtEnd() {
	return *scanner.current == '\0';
}

static char peek() {
	return *scanner.current;
}

static char peekNext() {
	if (isAtEnd()) return '\0';
	return scanner.current[1];
}

static char advance() {
	scanner.current++;
	return scanner.current[-1];
}

static bool match(char expected) {
	if (isAtEnd()) return false;

	if (*scanner.current != expected) return false;
	
	scanner.current++;
	return true;
}

static bool isAlpha(char c) {
	return (c >= 'a' && c <= 'z') 
		|| (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c) {
	return c >= '0' && c <= '9';
}

static Token string() {
	while (peek() != '"' && !isAtEnd()) {
		if (peek() == '\n') scanner.line++;
		advance();
	}

	if (isAtEnd()) return errorToken("Unterminated string");

	advance();
	return createToken(TOKEN_STRING);
}

static void skipComment() {
	while (!isAtEnd() && peek() != '\n') advance();
}

static void skipWhiteSpace() {
	for (;;) {
		char c = peek();
		switch (c) {
			case ' ':
			case '\t':
			case '\r':
				advance();
				break;
			case '\n':
				scanner.line++;
				advance();
				break;
			case '/':
				if (peekNext() != '/') return;
				skipComment();
				break;
			default:
				return;

		}

	}
}

static TokenType checkKeyword(int start, int length, const char* rest, TokenType type) {
	if (scanner.current - scanner.start == length + start
			&& memcmp(rest, scanner.start + start, length) == 0) {
		return type;
	}


	return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
	switch (scanner.start[0]) {
		case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
		case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
		case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
		case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
		case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
		case 'f': 
			  if (scanner.current - scanner.start > 1) {
				  switch (scanner.start[1]) {
					case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
					case 'u': return checkKeyword(2, 2, "nc", TOKEN_FUNC);
					case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
				  }
				
			  }
			  break;
		case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
		case 't': 
			  if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
					case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
				}
			  }
			  break;
		case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
		case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
		case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
		case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
		case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
	
	}

	return TOKEN_IDENTIFIER;
}

static Token identifier() {
	while (isAlpha(peek()) || isDigit(peek())) {
		advance();
	}

	return createToken(identifierType());
}

static Token number() {
	while (isDigit(peek())) advance();

	// Fractional part
	if (peek() == '.' && isDigit(peekNext())) {
		advance();
		while (isDigit(peek())) advance();
	}

	return createToken(TOKEN_NUMBER);
}

Token scanToken() {
	skipWhiteSpace();
	
	scanner.start = scanner.current;

	if (isAtEnd()) return createToken(TOKEN_EOF);

	char c = advance();
	
	if (isAlpha(c)) return identifier();
	else if (isDigit(c)) return number();

	switch (c) {
		case '+': return createToken(TOKEN_PLUS);
		case '-': return createToken(TOKEN_MINUS);
		case '*': return createToken(TOKEN_STAR);
		case '/': return createToken(TOKEN_SLASH);
		case '<': return createToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
		case '>': return createToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
		case '=': return createToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
		case '!': return createToken(match('=') ? TOKEN_NOT_EQUAL : TOKEN_NOT);
		case ',': return createToken(TOKEN_COMMA);
		case ';': return createToken(TOKEN_SEMICOLON);
		case '.': return createToken(TOKEN_DOT);
		case '{': return createToken(TOKEN_LEFT_BRACE);
		case '}': return createToken(TOKEN_RIGHT_BRACE);
		case '(': return createToken(TOKEN_LEFT_PAREN);
		case ')': return createToken(TOKEN_RIGHT_PAREN);
		case '"': return string();
	}

	return errorToken("Unknown caracter");
}
