#include <caca.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COLS 48
#define ROWS 24

uint8_t buffer[COLS * 3 * ROWS];
caca_display_t* cdisplay= NULL;
caca_dither_t* cdither= NULL;


void cleanup()
{
	if (cdither != NULL)
	{
		caca_free_dither(cdither);
	}

	if (cdisplay != NULL)
	{
		caca_free_display(cdisplay);
	}
}


void terminate(int signum)
{
	cleanup();
	exit(EXIT_SUCCESS);
}


int main(int argc, char* argv[])
{
	size_t retval;
	int fd;
	fd_set readfds;
	caca_canvas_t* ccanvas;
	caca_event_t cevent;
	struct timeval tv;
	struct sigaction act= {
		.sa_handler= &terminate,
	};
	int exitCode= EXIT_SUCCESS;

	retval= sigaction(SIGTERM, &act, NULL);
	if (retval == -1)
	{
		perror("Could not register TERM signal handler: ");
		exit(EXIT_FAILURE);
	}
	retval= sigaction(SIGINT, &act, NULL);
	if (retval == -1)
	{
		perror("Could not register INT signal handler: ");
		exit(EXIT_FAILURE);
	}

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
		fd= open(argv[1], O_RDONLY);
		if (fd == -1)
		{
			perror("Could not open input file: ");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		// stdin
		fd= 0;
	}
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	tv.tv_sec= 0;
	tv.tv_usec= 1000;

	// libcaca messes with the terminal such that it ignores BREAK, to read
	// ctrl-c we have to wait on a file descriptor and on libcaca events.
	// select() can't do that, so we have to use polling, sigh..
	while ((retval= select(fd + 1, &readfds, NULL, NULL, &tv)) != -1)
	{
		unsigned int offset= 0;

		caca_get_event(cdisplay, CACA_EVENT_KEY_PRESS, &cevent, 0);
		if (caca_get_event_key_ch(&cevent) == CACA_KEY_CTRL_C)
		{
			goto out;
		}

		if (FD_ISSET(fd, &readfds))
		{
			retval= read(fd, buffer + offset, sizeof(buffer) - offset);
			if (retval == 0)
			{
				if (offset != 0)
				{
					fprintf(stderr, "Bitmap too short, %lu bytes missing\n", ROWS *
						COLS * 3 - retval);
					exitCode= EXIT_FAILURE;
				}
				goto out;
			}
			if (retval == -1)
			{
				perror("Error reading from source: ");
				exitCode= EXIT_FAILURE;
				goto out;
			}
			offset+= retval;

			if (offset == sizeof(buffer))
			{
				caca_dither_bitmap(ccanvas, 0, 0,
					caca_get_canvas_width(ccanvas),
					caca_get_canvas_height(ccanvas), cdither, buffer);
				caca_refresh_display(cdisplay);

				offset= 0;
			}
		}

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		tv.tv_sec= 0;
		tv.tv_usec= 1000;
	}
	if (retval == -1)
	{
		perror("Error waiting for input: ");
		exitCode= EXIT_FAILURE;
	}

out:
	cleanup();
	return exitCode;
}
