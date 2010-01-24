#define _GNU_SOURCE
#include <string.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COLS 48
#define ROWS 24

/* Use a linear buffer index to convert a ledfloor buffer into an RGB buffer
 * row= index / (cols * 3)
 * col= index / 3 % cols
 * comp= index % 3
 * buffer index= ((cols - 1 - col) * 3 + 2 - comp) * rows + row
 */
#define RCC_TO_CCR(_index) \
	(((COLS - 1 - (_index / 3 % COLS)) * 3 + 2 - (_index % 3)) * ROWS + \
	 (_index / (COLS * 3)))

/* Use a linear buffer index to convert an RGB buffer into a ledfloor buffer
 * row= index % rows
 * col= cols - 1 - index / (rows * 3)
 * comp= 2 - index / rows % 3
 * rgb index= row * cols * 3 + col * 3 + comp
 */
#define CCR_TO_RCC(_index) \
	((_index % ROWS) * COLS * 3 + \
	(COLS - 1 - _index / (ROWS * 3)) * 3 + \
	(2 - _index / ROWS % 3))

uint8_t buffer[COLS * 3 * ROWS];

int main(int argc, char* argv[])
{
	unsigned int index;
	uint8_t value;
	enum {ENCODE, DECODE} mode;

	if (argc > 0 && strcmp("encode", basename(argv[0])) == 0)
	{
		mode= ENCODE;
	}
	else if (argc > 0 && strcmp("decode", basename(argv[0])) == 0)
	{
		mode= DECODE;
	}
	else
	{
		fprintf(stderr, "Unknown mode '%s', please call me 'encode' or "
			"'decode'\n", basename(argv[0]));
		exit(EXIT_FAILURE);
	}

	for (index= 0; index < sizeof(buffer); index++)
	{
		if (!fread(&value, 1, 1, stdin))
		{
			if (feof(stdin))
			{
				fprintf(stderr, "Bitmap too short, %u bytes missing\n",
					ROWS * COLS * 3 - index);
				exit(EXIT_FAILURE);
			}
			else if (ferror(stdin))
			{
				perror("Error reading bitmap: ");
				exit(EXIT_FAILURE);
			}
			else
			{
				abort();
			}
		}
		if (mode == ENCODE)
		{
			buffer[RCC_TO_CCR(index)]= value;
		}
		else
		{
			buffer[CCR_TO_RCC(index)]= value;
		}
	}

	if (fwrite(buffer, sizeof(buffer), 1, stdout) < sizeof(buffer))
	{
		if (feof(stdout))
		{
			fprintf(stderr, "stdout closed?\n");
			exit(EXIT_FAILURE);
		}
		else if (ferror(stdout))
		{
			perror("Error writing bitmap: ");
			exit(EXIT_FAILURE);
		}
		else
		{
			abort();
		}
	}

	return EXIT_SUCCESS;
}
