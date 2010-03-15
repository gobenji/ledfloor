#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ledfloor.h>

char* buffer= NULL;

void pferror(const int errsv, const char* format, ...);

uint16_t reverse12(uint16_t a);

int main(int argc, char* argv[])
{
	int retval;
	int frameFd, ctlListenFd, ledFd;
	struct sockaddr_in addr;
	in_port_t portNum= 3456;
	int option;
	const char* devPath= "/dev/ledfloor0";
	bool verbose= false;
	int ctlFd= 0;

	ledFd= open(devPath, O_WRONLY);
	if (ledFd == -1)
	{
		pferror(errno, "Can't open ledfloor device");
		abort();
	}

	frameFd= socket(AF_INET, SOCK_DGRAM, 0);
	if (frameFd == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	addr.sin_family= AF_INET;
	addr.sin_port= htons(portNum);
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
	retval= bind(frameFd, (struct sockaddr*) &addr, sizeof(addr));
	if (retval == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	ctlListenFd= socket(AF_INET, SOCK_STREAM, 0);
	if (ctlListenFd == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}
	option= 1;
	retval= setsockopt(ctlListenFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	if (retval == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	retval= bind(ctlListenFd, (struct sockaddr*) &addr, sizeof(addr));
	if (retval == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	retval= listen(ctlListenFd, 1);
	if (retval == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	if (verbose)
	{
		printf("Listenning on %s:%u...\n", inet_ntoa(addr.sin_addr), portNum);
	}

	buffer= malloc(LFCOLS * 3 * LFROWS);
	while(true)
	{
		size_t done= 0;
		fd_set rdfds;
		int max= 0;

		FD_ZERO(&rdfds);
		FD_SET(frameFd, &rdfds);
		FD_SET(ctlListenFd, &rdfds);
		max= frameFd > ctlListenFd ? frameFd : ctlListenFd;
		if (ctlFd != 0)
		{
			FD_SET(ctlFd, &rdfds);
			max= ctlFd > max ? ctlFd : max;
		}

		retval= select(max + 1, &rdfds, 0, 0, 0);
		if (retval == -1)
		{
			pferror(errno, "line %d", __LINE__);
			abort();
		}

		if (FD_ISSET(frameFd, &rdfds))
		{
			// read a frame
			struct sockaddr_in srcAddr;
			socklen_t addrLen;

			addrLen= sizeof(srcAddr);
			retval= recvfrom(frameFd, buffer + done, LFCOLS * 3 * LFROWS - done, 0, (struct sockaddr*) &srcAddr, &addrLen);
			if (retval == -1)
			{
				pferror(errno, "Error reading from network");
				abort();
			}

			done+= retval;

			if (verbose)
			{
				printf("Received %d bytes from %s\n", retval, inet_ntoa(srcAddr.sin_addr));
			}

			// write a frame
			if (done == LFCOLS * 3 * LFROWS)
			{
				done= 0;
				while (done < LFCOLS * 3 * LFROWS)
				{
					retval= write(ledFd, buffer + done, LFCOLS * 3 * LFROWS - done);
					if (retval == -1)
					{
						pferror(errno, "Error writing to ledfloor");
						abort();
					}

					done+= retval;
				}
				if (verbose)
				{
					printf("Wrote a frame\n");
				}
				done= 0;
			}
		}
		else if (FD_ISSET(ctlListenFd, &rdfds))
		{
			struct sockaddr_in srcAddr;
			socklen_t addrLen;

			if (ctlFd != 0)
			{
				retval= close(ctlFd);
				if (retval == -1)
				{
					pferror(errno, "line %d", __LINE__);
					abort();
				}
			}

			addrLen= sizeof(srcAddr);
			ctlFd= accept(ctlListenFd, (struct sockaddr*) &srcAddr, &addrLen);
			if (retval == -1)
			{
				pferror(errno, "line %d", __LINE__);
				abort();
			}

			if (verbose)
			{
				printf("New control connection from %s\n", inet_ntoa(srcAddr.sin_addr));
			}
		}
		else if (FD_ISSET(ctlFd, &rdfds))
		{
			struct command_t command;

			retval= read(ctlFd, &command, sizeof(command));
			if (retval == -1)
			{
				pferror(errno, "line %d", __LINE__);
				abort();
			}
			else if (retval == 0)
			{
				retval= close(ctlFd);
				if (retval == -1)
				{
					pferror(errno, "line %d", __LINE__);
					abort();
				}
				ctlFd= 0;
				if (verbose)
				{
					printf("Closed control connection\n");
				}
			}
			else if (retval < sizeof(command))
			{
				fprintf(stderr, "Warning: command missing %lu bytes\n",
					sizeof(command) - retval);
			}
			else
			{
				static uint16_t gamma_c[256];
				int i;

				command.latch_ndelay= ntohl(command.latch_ndelay);
				retval= ioctl(ledFd, LF_IOCSLATCHNDELAY, &command.latch_ndelay);
				if (retval == -1)
				{
					pferror(errno, "line %d", __LINE__);
					abort();
				}
				command.clk_ndelay= ntohl(command.clk_ndelay);
				retval= ioctl(ledFd, LF_IOCSCLKNDELAY, &command.clk_ndelay);
				if (retval == -1)
				{
					pferror(errno, "line %d", __LINE__);
					abort();
				}

				for (i= 0; i < 256; i++)
				{
					gamma_c[i]= reverse12(floor(pow((double) i / 255., command.gamma) * 4095));
				}
				retval= ioctl(ledFd, LF_IOCSGAMMATABLE, gamma_c);
				if (retval == -1)
				{
					pferror(errno, "line %d", __LINE__);
					abort();
				}
			}
		}
		else
		{
			fprintf(stderr, "Warning: no fd is set after select\n");
		}
	}
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


uint16_t reverse12(uint16_t a)
{
	uint16_t b= 0;
	int i;

	for (i= 0; i < 12; i++)
	{
		b<<= 1;
		b|= a & 1;
		a>>= 1;
	}

	return b;
}
