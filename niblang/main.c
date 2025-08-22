#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include "niblang.h"

extern int nibdebug;

char *fread_file(char *path)
{
	FILE *fp = fopen(path, "r");

	if (fp != NULL)
	{
		// Read the entire file
		fseek(fp, 0, SEEK_END);
		long file_size = ftell(fp);
		rewind(fp);

		char *buffer = malloc(file_size + 1);
		size_t bytes_read = fread(buffer, 1, file_size, fp);
		if (bytes_read != file_size)
		{
			free(buffer);
			buffer = NULL;
		}
		else
			buffer[file_size] = '\0';

		fclose(fp);

		return buffer;
	}

	return NULL;
}

int main(void)
{
	char *source = fread_file("./testcode.nib");
	
	if (source)
	{
		printf("Source:\n");
		printf("==================================\n");
		printf("%s\n", source);
		printf("----------------------------------\n");
		printf("\n");

		nibdebug = 0;
		// Compile source
		if (nib_compile_script(source))
		{
			// Post processing

			nib_dump_scopetree();

			// List all the variables
			nib_dump_global_variables();
			nib_dump_local_variables();

			nib_cleanup_compile();
		}

		free(source);
	}
	return 0;
}

