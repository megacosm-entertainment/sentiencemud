#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#include "niblang.h"

extern int nibmethoddebug;
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

void dummy_init();
void dummy_cleanup();

struct parse_params_s {
	char *name;
	bool dump;
	bool run;
	bool step;
	bool clock;
	int run_count;
};

bool parse_args(int argc, char **argv, struct parse_params_s *params)
{
	int n = 1;	// Skip argv[0]
	memset(params,0,sizeof(*params));
	params->run = true;
	params->run_count = 1;
	
	while(n < argc)
	{
		if (!str_cmp(argv[n], "-d"))
			params->dump = true;
		else if (!str_cmp(argv[n], "-s"))
			params->step = true;
		else if (!str_cmp(argv[n], "-t"))
			params->clock = true;
		else if (!str_cmp(argv[n], "-f"))
		{
			if ((n+1) >= argc)
			{
				fprintf(stderr, "Missing input file name.\n");
				return false;
			}

			params->name = argv[++n];
		}
		else if (!str_cmp(argv[n], "-n"))
		{
			if ((n+1) >= argc)
			{
				fprintf(stderr, "Missing run count.\n");
				return false;
			}

			int count = atoi(argv[++n]);
			if (count < 1)
			{
				fprintf(stderr, "Run count must be positive.\n");
				return false;
			}

			params->run_count = count;
		}
		else
		{
			fprintf(stderr, "Invalid option '%s'.\n", argv[n]);
			return false;
		}
		
		n++;
	}

	if (params->step && params->run_count > 1)
	{
		fprintf(stderr, "Run count ignored in STEP mode.\n");
		params->run_count = 1;
	}

	return true;
}

int main(int argc, char **argv)
{
	srand(time(NULL));
	struct parse_params_s params;
	if (argc < 2 || !parse_args(argc,argv,&params))
	{
		fprintf(stderr, "Usage: niblang <options>\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "-f <file>   - Compiles <file>.\n");
		fprintf(stderr, "-d          - Dumps compiled script information.\n");
		fprintf(stderr, "-s          - Executes the script in Step mode.\n");
		fprintf(stderr, "-t          - Times execution when in Run mode.\n");
		fprintf(stderr, "-n <count>  - Executes the code <count>.  Ignored in STEP mode.\n");
		exit(-1);
	}

	nibmethoddebug = 0;

	dummy_init();

	if (!nib_methods_init())
	{
		dummy_cleanup();
		nib_flag_tables_cleanup();
		nib_ledger_cleanup();
		fprintf(stderr, "Failed to load method definitions.\n");
		exit(1);
	}

	if(variable_init())
	{
		char *source = fread_file(params.name);
		
		if (source)
		{
			printf("Source:\n");
			printf("==================================\n");
			printf("%s\n", source);
			printf("----------------------------------\n");
			printf("\n");

			nibdebug = 0;
			// Compile source
			NIB_SCRIPT *script = nib_compile_script(source, NSC_MOBILE);
			if (script)
			{
				struct termios oldt, newt;
				tcgetattr(STDIN_FILENO, &oldt);
				newt = oldt;
				newt.c_lflag &= ~(ICANON | ECHO);
				tcsetattr(STDIN_FILENO, TCSANOW, &newt);

				printf("\033[?25l");

				// Post processing
				if (params.dump)
				{
					nib_dump_program();

					nib_decompile_code(script);

					nib_dump_script_tables(script);

					nib_dump_scopetree();

					// List all the variables
					// nib_dump_global_variables();
					// nib_dump_local_variables();

					// nib_dump_string_storage();

					printf("Press Q to quit.  Anything else to continue.");

					char ch = getchar();
					if (ch == 'q' || ch == 'Q')
						params.run = false;

					printf("\n");
				}

				if (params.run)
				{
					if (params.step)
					{
						struct winsize w;
						ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
					
						NIB_SCRIPT_RUNTIME *nsr = nib_step_execute_init(script);
						if (nsr && !nib_is_execution_done(nsr))
						{
							nib_step_execute_show(nsr, w.ws_row, w.ws_col);

							while(true)
							{
								char ch = getchar();
								if (ch == '\n')
								{
									if (nib_is_execution_done(nsr) || nib_get_last_return(nsr) != SCPERR_SUCCESS)
										break;

									// Step
									nib_step_execute(nsr);

									if (nib_get_last_return(nsr) == SCPERR_SUCCESS)
										nib_step_execute_show(nsr, w.ws_row, w.ws_col);
									else
										break;
								}
								else if (ch == 'q')
								{
									// Quit
									break;
								}
							}
						}

						nib_step_execute_show(nsr, w.ws_row, w.ws_col);

						char *code = NULL;
						if(nib_get_last_return(nsr) < 0)
						{
							switch(nib_get_last_return(nsr))
							{
							case SCPERR_FAILURE:	code = "FAILURE"; break;
							case SCPERR_MATH:		code = "MATH"; break;
							case SCPERR_INVALID:	code = "INVALID"; break;
							case SCPERR_MEMORY:		code = "MEMORY"; break;
							case SCPERR_STACK:		code = "STACK"; break;
							case SCPERR_FIELD:		code = "FIELD"; break;
							case SCPERR_METHOD:		code = "METHOD"; break;
							case SCPERR_FUNCTION:	code = "FUNCTION"; break;
							}

						}

						if (code)
							printf("\nScript Return: %s (%d)\n", code, nib_get_last_return(nsr));
						else
							printf("\nScript Return: %d\n", nib_get_last_return(nsr));

						nib_step_execute_cleanup(nsr);
					}
					else
					{
						clock_t total;
						int ret;
						if (params.run_count > 1)
						{
							total = 0;
							for(int i = params.run_count; i-- > 0;)
							{
								clock_t begin = clock();
								ret = nib_interpret_script(script);
								total += clock() - begin;

								nib_dump_script_global_variables(script);

								if (ret) break;
							}
						}
						else
						{
							total = clock();
							ret = nib_interpret_script(script);
							total = clock() - total;

							nib_dump_script_global_variables(script);
						}
						char *code = NULL;
						if(ret < 0)
						{
							switch(ret)
							{
							case SCPERR_FAILURE:	code = "FAILURE"; break;
							case SCPERR_MATH:		code = "MATH"; break;
							case SCPERR_INVALID:	code = "INVALID"; break;
							case SCPERR_MEMORY:		code = "MEMORY"; break;
							case SCPERR_STACK:		code = "STACK"; break;
							case SCPERR_FIELD:		code = "FIELD"; break;
							case SCPERR_METHOD:		code = "METHOD"; break;
							case SCPERR_FUNCTION:	code = "FUNCTION"; break;
							}

						}

						if (code)
							printf("Script Return: %s (%d)\n", code, ret);
						else
							printf("Script Return: %d\n", ret);
						if (params.clock)
							printf("Execution time: %.3lfms\n", 1000.0 * (double)total / CLOCKS_PER_SEC);
					}

				}

				printf("\n\033[?25h");
				tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

				free_nib_script(script);
			}

			nib_cleanup_compile();

			free(source);
		}
	}

	variable_cleanup();

	dummy_cleanup();

	nib_methods_cleanup();

	nib_flag_tables_cleanup();

	nib_ledger_display();
	printf("outstanding allocations: %lu\n", nib_allocations);
	nib_ledger_cleanup();

	printf("\U0001F60A\n\U00004E16\n");
	return 0;
}

