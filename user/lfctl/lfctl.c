#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include <ledfloor.h>

struct termios savedAttributes;


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
void pferror(const int errsv, const char* format, ...)
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


// sert à restaurer le mode du terminal à ce qu'il était au paravant
void resetInputMode(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &savedAttributes);
}


int main(int argc, char* argv[])
{
	int retval;
	struct termios termAttributes;
	char touche;
	int ctlFd;
	const char* hostName;
	struct sockaddr_in dst;
	char* dstString;
	struct addrinfo hints, * results;
	in_port_t portNum= 3456;
	struct command_t command= {
		.gamma= 2.2,
		.contrast= 0.5,
		.brightness= 0.5,
		.blank= false,
	};
	uint32_t latch_ndelay= 0;
	uint32_t clk_ndelay= 200;

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

	ctlFd= socket(AF_INET, SOCK_STREAM, 0);
	if (ctlFd == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	retval= connect(ctlFd, (struct sockaddr*) &dst, sizeof(dst));
	if (retval == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	printf("Transmitting to %s:%u...\n", dstString, portNum);

	// on s'assure que stdin est un terminal
	if (!isatty(STDIN_FILENO))
	{
		fprintf(stderr, "Not a terminal.\n");
		abort();
	}

	// acquision du mode actuel du terminal
	tcgetattr(STDIN_FILENO, &termAttributes);
	savedAttributes= termAttributes;
	atexit(resetInputMode);

	// régler le nouveau mode du terminal
	// clear de ICANON et ECHO
	// (lecture de tous les caracteres sans leur affichage)
	termAttributes.c_lflag &= ~(ICANON|ECHO);
	termAttributes.c_cc[VMIN]= 1;
	termAttributes.c_cc[VTIME]= 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &termAttributes);	

	while (true)
	{
		const uint32_t ndelayAdjust= 10;
		const float gammaAdjust= 0.1;

		read(STDIN_FILENO, &touche, 1);
		
		switch (touche)
		{
			case 'q':
				if (latch_ndelay < ((int32_t) -1) - ndelayAdjust)
				{
					latch_ndelay+= ndelayAdjust;
				}
				command.latch_ndelay= htonl(latch_ndelay);
				break;

			case 'a':
				if (latch_ndelay >= ndelayAdjust)
				{
					latch_ndelay-= ndelayAdjust;
				}
				command.latch_ndelay= htonl(latch_ndelay);
				break;

			case 'w':
				if (clk_ndelay < ((int32_t) -1) - ndelayAdjust)
				{
					clk_ndelay+= ndelayAdjust;
				}
				command.clk_ndelay= htonl(clk_ndelay);
				break;

			case 's':
				if (clk_ndelay >= ndelayAdjust)
				{
					clk_ndelay-= ndelayAdjust;
				}
				command.clk_ndelay= htonl(clk_ndelay);
				break;

			case 'e':
				if (command.gamma + gammaAdjust < 4)
				{
					command.gamma+= gammaAdjust;
				}
				break;

			case 'd':
				if (command.gamma > gammaAdjust)
				{
					command.gamma-= gammaAdjust;
				}
				break;
		}

		printf("latch: %u clock: %u gamma: %f\n", latch_ndelay, clk_ndelay,
			command.gamma);

		retval= send(ctlFd, &command, sizeof(command), 0);
		if (retval == - 1)
		{
			pferror(errno, "line %d", __LINE__);
			abort();
		}
		if (retval < sizeof(command))
		{
			fprintf(stderr, "Couldn't write complete command\n");
			abort();
		}
	}
}
