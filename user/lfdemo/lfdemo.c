#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <caca.h>

#include <ledfloor.h>


enum action { PREPARE, INIT, UPDATE, RENDER, FREE };

static void pferror(const int errsv, const char* format, ...);
static void plasma(enum action, uint8_t** buffer);

static int frame = 0;


int main(int argc, const char* argv[])
{
	int netFd;
	int retval;
	struct sockaddr_in dst;
	in_port_t portNum= 3456;
	struct addrinfo hints, * results;
	char* dstString;
	const char* hostName;
	uint8_t* buffer= NULL;
	int randomFd;
	unsigned int seed;

	caca_display_t* cdisplay= NULL;
	caca_dither_t* cdither= NULL;
	caca_canvas_t* ccanvas;

	cdisplay= caca_create_display(NULL);
	if(cdisplay == NULL)
	{
		perror("Could not create libcaca display: ");
		exit(EXIT_FAILURE);
	}
	caca_set_display_title(cdisplay, "LedFloor");
	ccanvas= caca_get_canvas(cdisplay);
	cdither= caca_create_dither(24, LFCOLS, LFROWS, 3 * LFCOLS, 0xff, 0xff00, 0xff0000, 0);
	if (cdither == NULL)
	{
		perror("Could not create libcaca dither: ");
		exit(EXIT_FAILURE);
	}

	if (argc < 2)
	{
		hostName= "localhost";
	}
	else if (argc == 2)
	{
		hostName= argv[1];
	}
	else
	{
		fprintf(stderr, "Too many arguments. Usage: %s <host>\n", argv[0]);
	}

	randomFd= open("/dev/urandom", O_RDONLY);
	if (randomFd == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	retval= read(randomFd, &seed, sizeof(unsigned int));
	if (retval == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}
	close(randomFd);

	srandom(seed);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family= AF_INET;
	retval= getaddrinfo(hostName, NULL, &hints, &results);
	if (retval != 0)
	{
		if (retval == EAI_SYSTEM)
		{
			pferror(errno, "line %d", __LINE__);
		}
		else
		{
			fprintf(stderr, "line %d: %s\n", __LINE__, gai_strerror(retval));
		}
		abort();
	}
	memcpy(&dst.sin_addr, &((struct sockaddr_in*) results->ai_addr)->sin_addr, sizeof(dst.sin_addr));
	freeaddrinfo(results);
	dst.sin_family= AF_INET;
	dst.sin_port= htons(portNum);
	dstString= inet_ntoa(dst.sin_addr);

	netFd= socket(AF_INET, SOCK_DGRAM, 0);
	if (netFd == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	retval= connect(netFd, (struct sockaddr*) &dst, sizeof(dst));
	if (retval == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	printf("Transmitting to %s:%u...\n", dstString, portNum);

	plasma(PREPARE, &buffer);
	plasma(INIT, &buffer);
	while(true)
	{
		plasma(UPDATE, &buffer);
		plasma(RENDER, &buffer);

		retval= send(netFd, buffer, LFROWS * LFCOLS * 3, 0);
		if (retval == - 1)
		{
			pferror(errno, "line %d", __LINE__);
			abort();
		}
		if (retval < LFROWS * LFCOLS * 3)
		{
			fprintf(stderr, "Couldn't write complete frame\n");
			abort();
		}

		caca_dither_bitmap(ccanvas, 0, 0,
			caca_get_canvas_width(ccanvas),
			caca_get_canvas_height(ccanvas), cdither, buffer);
		caca_refresh_display(cdisplay);

		frame++;
		usleep(33330);
	}
	plasma(FREE, &buffer);
}


/*
 * Print a custom error message followed by the message associated with an
 * errno.
 *
 * Mostly copied from printf(3)
 *
 * Args:
 *   errsv:        the errno for which to print the message
 *   format:       a printf-style format string
 *   ...:          the arguments associated with the format
 */
static void pferror(const int errsv, const char* format, ...)
{
	/* Guess we need no more than 100 bytes. */
	int n, size= 100;
	char *p, *np;
	va_list ap;

	if ((p= malloc(size)) == NULL)
	{
		return;
	}

	while (true)
	{
		/* Try to print in the allocated space. */
		va_start(ap, format);
		n= vsnprintf(p, size, format, ap);
		va_end(ap);
		/* If that worked, output the string. */
		if (n > -1 && n < size)
		{
			fputs(p, stderr);
			fprintf(stderr, ": %s\n", strerror(errsv));
			return;
		}

		/* Else try again with more space. */
		if (n > -1)    /* glibc 2.1 */
		{
			size= n + 1; /* precisely what is needed */
		}
		else           /* glibc 2.0 */
		{
			size*= 2;  /* twice the old size */
		}

		if ((np= realloc(p, size)) == NULL)
		{
			free(p);
			return;
		}
		else
		{
			p= np;
		}
	}
}


/* The plasma effect */
#define TABLEX (LFCOLS * 2)
#define TABLEY (LFROWS * 2)
static uint8_t table[TABLEX * TABLEY];

static void do_plasma(uint8_t *, double, double, double, double, double,
	double);
static void do_palette(uint8_t* color_screen, const uint8_t* index_screen,
	const uint8_t* red, const uint8_t* green, const uint8_t* blue);

static void plasma(enum action action, uint8_t** buffer)
{
    static uint8_t *index_screen;
    static uint8_t *color_screen;
    static uint8_t red[256], green[256], blue[256];
    static double r[3], R[6];

    int i, x, y;

    switch(action)
    {
    case PREPARE:
        /* Fill various tables */
        for(i = 0 ; i < 256; i++)
            red[i] = green[i] = blue[i] = 0;

        for(i = 0; i < 3; i++)
			r[i] = (((double) random() / RAND_MAX) * 998 + 1) / 60000 * M_PI;

        for(i = 0; i < 6; i++)
            R[i] = (((double) random() / RAND_MAX) * 998 + 1) / 10000;

        for(y = 0 ; y < TABLEY ; y++)
            for(x = 0 ; x < TABLEX ; x++)
        {
            double tmp = (((double)((x - (TABLEX / 2)) * (x - (TABLEX / 2))
                                  + (y - (TABLEX / 2)) * (y - (TABLEX / 2))))
                          * (M_PI / (TABLEX * TABLEX + TABLEY * TABLEY)));

            table[x + y * TABLEX] = (1.0 + sin(12.0 * sqrt(tmp))) * 256 / 6;
        }
        break;

    case INIT:
        index_screen = malloc(LFCOLS * LFROWS * sizeof(uint8_t));
        color_screen = malloc(LFCOLS * LFROWS * 3 * sizeof(uint8_t));
        break;

    case UPDATE:
        for(i = 0 ; i < 256; i++)
        {
            double z = ((double)i) / 256 * 6 * M_PI;

            red[i] = (1.0 + sin(z + r[1] * frame)) / 2 * 256;
            blue[i] = (1.0 + cos(z + r[0] * (frame + 100))) / 2 * 256;
            green[i] = (1.0 + cos(z + r[2] * (frame + 200))) / 2 * 256;
        }

        do_plasma(index_screen,
                  (1.0 + sin(((double)frame) * R[0])) / 2,
                  (1.0 + sin(((double)frame) * R[1])) / 2,
                  (1.0 + sin(((double)frame) * R[2])) / 2,
                  (1.0 + sin(((double)frame) * R[3])) / 2,
                  (1.0 + sin(((double)frame) * R[4])) / 2,
                  (1.0 + sin(((double)frame) * R[5])) / 2);
        break;

    case RENDER:
		do_palette(color_screen, index_screen, red, green, blue);
		*buffer= color_screen;
        break;

    case FREE:
        free(index_screen);
        free(color_screen);
        break;
    }
}

static void do_plasma(uint8_t *pixels, double x_1, double y_1,
                      double x_2, double y_2, double x_3, double y_3)
{
    unsigned int X1 = x_1 * (TABLEX / 2),
                 Y1 = y_1 * (TABLEY / 2),
                 X2 = x_2 * (TABLEX / 2),
                 Y2 = y_2 * (TABLEY / 2),
                 X3 = x_3 * (TABLEX / 2),
                 Y3 = y_3 * (TABLEY / 2);
    unsigned int y;
    uint8_t * t1 = table + X1 + Y1 * TABLEX,
            * t2 = table + X2 + Y2 * TABLEX,
            * t3 = table + X3 + Y3 * TABLEX;

    for(y = 0; y < LFROWS; y++)
    {
        unsigned int x;
        uint8_t * tmp = pixels + y * LFCOLS;
        unsigned int ty = y * TABLEX, tmax = ty + LFCOLS;
        for(x = 0; ty < tmax; ty++, tmp++)
            tmp[0] = t1[ty] + t2[ty] + t3[ty];
    }
}

static void do_palette(uint8_t* color_screen, const uint8_t* index_screen,
	const uint8_t* red, const uint8_t* green, const uint8_t* blue)
{
	unsigned int i;

	for (i= 0; i < LFROWS * LFCOLS; i++)
	{
		color_screen[3 * i]= red[index_screen[i]];
		color_screen[3 * i + 1]= green[index_screen[i]];
		color_screen[3 * i + 2]= blue[index_screen[i]];
	}
}
