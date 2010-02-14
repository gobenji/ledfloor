#include <caca.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COLS 48
#define ROWS 24

uint8_t buffer[COLS * 3 * ROWS];


int main(int argc, char* argv[])
{
	size_t retval;
	FILE* f;
	caca_display_t* cdisplay;
	caca_canvas_t* ccanvas;
	caca_dither_t* cdither;

	cdisplay= caca_create_display(NULL);
	if(cdisplay == NULL)
	{
		perror("Could not create libcaca display: ");
		exit(EXIT_FAILURE);
	}
	caca_set_display_title(cdisplay, "LedFloor");
	ccanvas= caca_get_canvas(cdisplay);
	cdither= caca_create_dither(24, COLS, ROWS, 3 * COLS, 0xff, 0xff00, 0xff0000, 0);
	if (cdither == NULL)
	{
		perror("Could not create libcaca dither: ");
		exit(EXIT_FAILURE);
	}

	if (argc > 1)
	{
		f= fopen(argv[1], "r");
		if (f == NULL)
		{
			perror(NULL);
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		f= stdin;
	}

	while ((retval= fread(buffer, 1, sizeof(buffer), f)))
	{
		if (retval == 0 && feof(f))
		{
			break;
		}
		if (retval < sizeof(buffer))
		{
			if (ferror(f))
			{
				perror("Error reading from source: ");
			}
			else
			{
				fprintf(stderr, "Bitmap too short, %lu bytes missing\n", ROWS *
					COLS * 3 - retval);
			}

			caca_free_display(cdisplay);
			exit(EXIT_FAILURE);
		}

		caca_dither_bitmap(ccanvas, 0, 0, COLS, ROWS, cdither, buffer);
		caca_refresh_display(cdisplay);
	}

	caca_free_dither(cdither);
	caca_free_display(cdisplay);

	return EXIT_SUCCESS;
}
