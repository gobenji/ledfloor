#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ledfloor.h>

char* buffer= NULL;

void pferror(const int errsv, const char* format, ...);

int main(int argc, char* argv[])
{
	int netFd;
	FILE* ledF;
	int retval;
	struct sockaddr_in addr;
	in_port_t portNum= 3456;
	const char* devPath= "/dev/ledfloor0";
	struct stat statBuf;

	retval= stat(devPath, &statBuf);
	if (retval == -1)
	{
		pferror(errno, "Can't stat ledfloor device");
		abort();
	}
	ledF= fopen(devPath, "a");
	if (ledF == NULL)
	{
		pferror(errno, "Can't open ledfloor device");
		abort();
	}

	netFd= socket(AF_INET, SOCK_DGRAM, 0);
	if (netFd == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	addr.sin_family= AF_INET;
	addr.sin_port= htons(portNum);
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
	retval= bind(netFd, (struct sockaddr*) &addr, sizeof(addr));
	if (retval == -1)
	{
		pferror(errno, "line %d", __LINE__);
		abort();
	}

	{
		char* addrString;

		addrString= inet_ntoa(addr.sin_addr);
		//printf("Listenning for data on %s:%u...\n", addrString, portNum);
	}

	buffer= malloc(LFCOLS * 3 * LFROWS);
	while(true)
	{
		size_t done;

		// read a frame
		done= 0;
		while (done < LFCOLS * 3 * LFROWS)
		{
			struct sockaddr_in srcAddr;
			socklen_t addrLen;

			addrLen= sizeof(srcAddr);
			retval= recvfrom(netFd, buffer + done, LFCOLS * 3 * LFROWS - done, 0, (struct sockaddr*) &srcAddr, &addrLen);
			if (retval == -1)
			{
				pferror(errno, "Error reading from network");
				abort();
			}

			done-= retval;

			//printf("Received %d bytes from %s\n", retval, inet_ntoa(srcAddr.sin_addr));
		}

		// write a frame
		retval= fwrite(buffer, LFCOLS * 3 * LFROWS, 1, ledF);
		if (retval < 1)
		{
			if (feof(stdin))
			{
				fprintf(stderr, "Couldn't write complete frame\n");
				abort();
			}
			else if (ferror(stdin))
			{
				pferror(errno, "Error writing to ledfloor");
				abort();
			}
			else
			{
				abort();
			}
		}
		fflush(ledF);
		//printf("Wrote a frame\n");
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
