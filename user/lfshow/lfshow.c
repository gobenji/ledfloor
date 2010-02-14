#include <curses.h>

#define COLS 48
#define ROWS 24

uint8_t buffer[COLS * 3 * ROWS];

int main()
{
	size_t retval;

	while (retval= fread(buffer, sizeof(buffer), 1, stdin))
	{
		if (retval != sizeof(buffer))
		{
			break;
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

	return EXIT_SUCCESS;
}
