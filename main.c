#include <stdio.h>
#include <stdlib.h>

void runFile(const char* filename);
void repl();

int main(int argc, const char** argv) {
	if (argc == 2) {
		// script mode
		runFile(argv[1]);

	} else if (argc == 1) {
		// interactive mode 
		repl();// Read Evaluate Print Loop
	} else {
		fprintf(stderr, "Error: Too much arguments\n");
		fprintf(stderr, "Usage: ant [file]\n");
		exit(1);
	}
	
	return 0;
}

void runFile(const char* filename) {
	FILE* file = fopen(filename, "r");
	
	if (file == NULL) {
		perror("Error");
		exit(1);
	}
	
	fseek(file, 0L, SEEK_END);
	size_t count = ftell(file);
	fseek(file, 0L, SEEK_SET);

	char source[count];
	size_t readCount = fread(source, sizeof(*source), sizeof(source)/sizeof(source[0]), file); 
	
	if (readCount != count) {
		fprintf(stderr, "Error: Unable to read the file `%s`\n", filename);
		exit(1);
	}

	source[count - 1] = '\0';

	fclose(file);
	
	//interpret(source);
}

void repl() {
	char line[200];
	
	for (;;) {
		printf("> ");
		
		if (fgets(line, sizeof(line)/sizeof(line[0]), stdin) == NULL) {
			fprintf(stderr, "Unable to read line\n");
			exit(1);
		}
		
		//interpret(line);
	}
}
