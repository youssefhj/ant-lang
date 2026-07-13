#include <stdio.h>
#include <stdlib.h>

#include "vm.h"


void runFile(const char* filename) {
	FILE* file = fopen(filename, "rb");
	
	if (file == NULL) {
		perror("Error");
		exit(74);
	}
	
	fseek(file, 0L, SEEK_END);
	size_t count = ftell(file);
	fseek(file, 0L, SEEK_SET);

	char source[count];
	
	size_t readCount = fread(source, sizeof(*source), sizeof(source)/sizeof(source[0]), file); 	
	if (readCount != count) {
		fprintf(stderr, "Error: Unable to read the file `%s`\n", filename);
		exit(74);
	}

	source[count - 1] = '\0';

	fclose(file);
	
	InterpretResult result = interpret(source);

	if (result == INTERPRET_COMPILETIME_ERROR) exit(65);
	if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

void repl() {
	char line[1024];
	
	for (;;) {
		printf("> ");
		
		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}
		
		interpret(line);

		printf("\n");
	}
}

int main(int argc, const char* argv[]) {
	initVM();

	if (argc == 2) {
		// script mode
		runFile(argv[1]);

	} else if (argc == 1) {
		// interactive mode 
		repl();// Read Evaluate Print Loop
	} else {
		fprintf(stderr, "Error: Too much arguments\n");
		fprintf(stderr, "Usage: ant [file]\n");
		exit(64);
	}
	
	freeVM();

	return 0;
}
