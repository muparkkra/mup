char * license_text = 
" Copyright (c) 1995-2020  by Arkkra Enterprises.\n\
 All rights reserved.\n\
\n\
 Redistribution and use in source and binary forms,\n\
 with or without modification, are permitted provided that\n\
 the following conditions are met:\n\
\n\
 1. Redistributions of source code must retain\n\
 the above copyright notice, this list of conditions\n\
 and the following DISCLAIMER.\n\
\n\
 2. Redistributions in binary form must reproduce the above\n\
 copyright notice, this list of conditions and\n\
 the following DISCLAIMER in the documentation and/or\n\
 other materials provided with the distribution.\n\
\n\
 3. Any additions, deletions, or changes to the original files\n\
 must be clearly indicated in accompanying documentation,\n\
 including the reasons for the changes,\n\
 and the names of those who made the modifications.\n\
\n\
	DISCLAIMER\n\
\n\
 THIS SOFTWARE IS PROVIDED \"AS IS\" AND ANY EXPRESS\n\
 OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,\n\
 THE IMPLIED WARRANTIES OF MERCHANTABILITY\n\
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.\n\
 IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,\n\
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,\n\
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO\n\
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n\
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n\
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,\n\
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
 (INCLUDING NEGLIGENCE OR OTHERWISE)\n\
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,\n\
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
"
;

/* This is a utility program that takes a PostScript file and creates
 * a bitmap that can then be included in a C file.
 * It expects one argument, which is name to use for the bitmap.
 * It reads stdin and writes to stdout.
 * Output is C code giving width, height, and contents of bitmap.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#if defined(SIGCHLD) && ! defined(SIGCLD)
#define SIGCLD SIGCHLD
#endif

#ifdef linux
#define sigset(x, y) signal(x, y)
#endif

/* page dimensions. BORDER is how much space to leave around bitmap */
#define PGWIDTHBYTES	77
#define PGHEIGHT	792
#define BORDER		8

/* Ghostscript temp file */
char gstempfile[L_tmpnam];

void cleanup(int status);



int
main(argc, argv)

int argc;
char **argv;

{
	int ret;		/* ret val of Ghostscript */
	int leftmost, rightmost, top, bottom;
	char *name;		/* of bitmap */
	int t;			/* temporary file for Ghostscript */
	char buff[BUFSIZ];
	int i, j;
	int child;		/* Ghostscript PID */


	if (argc != 2) {
		fprintf(stderr, "usage: %s name < file.ps > file.c\n", argv[0]);
		exit(1);
	}

	name = argv[1];
	if (tmpnam(gstempfile) == 0) {
		fprintf(stderr, "unable to create temporary file\n");
		exit(1);
	}

	/* arrange to remove temp file */
	for (i = 1; i < NSIG; i++) {
		if (i != SIGKILL && i != SIGCLD && i != SIGWINCH) {
			sigset(i, cleanup);
		}
	}
	sigset(SIGWINCH, SIG_IGN);

	/* run Ghostscript */
	switch(child = fork()) {
	case 0:
		(void) snprintf(buff, sizeof(buff), "-sOutputFile=%s", gstempfile);
		close(1);
		if (dup(2) < 0) {
			fprintf(stderr, "unable to connect to gs\n");
			exit(1);
		}
		(void) execlp("gs", "gs", "-sDEVICE=bit", "-dNOPAUSE",
				"-sPAPERSIZE=letter", "-dQUIET", buff, "-", (char *) 0);
		/*FALLTHRU*/
	case -1:
		(void) fprintf(stderr, "failed to exec gs\n");
		cleanup(1);
	default:
		/* wait for Ghostscript to complete */
		do {
		} while (wait(&ret) != child);
		
		if (ret != 0) {
			(void) fprintf(stderr, "gs failed\n");
			cleanup(1);
		}

		/* read output from Ghostscript */
		if ((t = open(gstempfile, O_RDONLY)) < 0) {
			(void) fprintf(stderr, "can't open temp file\n");
			cleanup(1);
		}

		/* find bounding box info. For left/right, work inwards till
		 * find a non-zero byte. For top/bottom find rows
		 * containing non-zero bytes */
		leftmost = PGWIDTHBYTES - 1;
		rightmost = 0;
		top = -1;
		for (i = 0; i < PGHEIGHT; i++) {
			/* read a row of bits */
			if (read(t, buff, PGWIDTHBYTES) != PGWIDTHBYTES) {
				(void) fprintf(stderr, "read error\n");
				cleanup(-1);
			}

			/* see if any farther left than we found before */
			for (j = 0; j <= leftmost; j++) {
				if (buff[j]) {
					leftmost = j;
					break;
				}
			}

			/* see if any farther right */
			for (j = PGWIDTHBYTES - 1; j >= rightmost; j--) {
				if (buff[j]) {
					rightmost = j;
					break;
				}
			}

			/* check if anything in row, for finding top/bottom */
			for (j = 0; j < PGWIDTHBYTES; j++) {
				if (buff[j]) {
					if (top == -1) {
						top = i;
					}
					bottom = i;
					break;
				}
			}
		}
		if (top < 0) {
			(void) fprintf(stderr, "no output generated\n");
			cleanup(-1);
		}

		printf("/*\n%s\n*/\n\n", license_text);

		/* output width, height and contents of bitmap */
		printf("int %s_width = %d;\n", name, rightmost - leftmost + 5);
		printf("int %s_height = %d;\n", name, bottom - top + 1 + 4 * BORDER);
		printf("unsigned char %s_bitmap[] = {\n", name);

		/* generate black border on top */
		for (i = 0; i < BORDER; i++) {
			for (j = leftmost - 2; j <= rightmost + 2; j++) {
				printf("0x00, ");
			}
			printf("\n");
		}
		/* then white space on top */
		for (i = 0; i < BORDER; i++) {
			printf("0x00, ");
			for (j = leftmost - 1; j <= rightmost + 1; j++) {
				printf("0xff, ");
			}
			printf("0x00, ");
			printf("\n");
		}
		
		/* then the information in reverse video */
		for (i = top; i <= bottom; i++) {
			lseek(t, i * PGWIDTHBYTES + leftmost, SEEK_SET);
			printf("0x00, 0xff, ");
			for (j = leftmost; j <= rightmost; j++) {
				if (read(t, buff, 1) != 1) {
					(void) fprintf(stderr, "read error\n");
					cleanup(1);
				}
				printf("0x%02x, ", (buff[0] ^ 0xff) & 0xff);
			}
			printf("0xff, 0x00,\n");
		}

		/* then bottom white space */
		for (i = 0; i < BORDER; i++) {
			printf("0x00, ");
			for (j = leftmost - 1; j <= rightmost + 1; j++) {
				printf("0xff,");
			}
			printf("0x00, ");
			printf("\n");
		}
		/* and bottom black border */
		for (i = 0; i < BORDER; i++) {
			for (j = leftmost - 2; j <= rightmost + 2; j++) {
				printf("0x00");
				if (i != BORDER - 1 || j != rightmost + 2) {
					printf(", ");
				}
			}
			printf("\n");
		}
		printf("};\n\n");
	}
	cleanup(0);
}


void
cleanup(status)
int status;
{
	unlink(gstempfile);
	exit(status);
}
