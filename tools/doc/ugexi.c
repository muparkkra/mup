/* Copyright 2007, 2012, 2019 by Arkkra Enterprises */
/* All rights reserved */

/* This utility program reads the Mup User's Guide and does what it takes
 * to add output examples. It reads stdin and just copies most of it. However,
 * if it comes across a .Ex macro that has an argument, it also copies
 * the contexts from there to the .Ee to a temp file. Any troff comment
 * lines (starting with .\") will also be copied to the temp file, but
 * without the comment marker. In this way, the input can add Mup input
 * that won't be printed as part of the input example, but will be
 * included in the output example (for example, to set some score parameters
 * that are irrelevant to the example, but are needed to make it print
 * the way we want). Once the temp file is built, mup is run on it, then
 * gs, then the bounding box information is determined from the gs output.
 * A Postscript file is then produced which is the original Mup example
 * output, minus the prolog and "quit" statement, plus a %%BoundingBox
 * comment. The .Ee macro is modified to include the filename of this
 * generated PostScript file, along with height and width in inches, and
 * width in points. The uguide is expected to have the .Ee macro defined
 * such that if those arguments are present, it will generate the
 * appropriate \!x X PI statement to be processed by grops to include
 * the Mup output as a picture.
 *
 * This also generates an index of the parameters, with an HTML tag and
 * link to be able to jump to any parameter, along with
 * a letter-of-the-alphabet index that lets the user jump to the index for
 * parameters that start with that letter.
 * This is put in a file named "param_index" which is then used by the
 * tr2html utility program that generates the HTML version of the User's Guide.
 */


#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef linux
#define sigset(x, y)  signal(x, y)
#endif
#if defined(SIGCHLD) && ! defined(SIGCLD)
#define SIGCLD SIGCHLD
#endif

#define PGHEIGHT	(792)	/* 11 inches in points */
#define PGWIDTH		(77)	/* 8.5 inches in inches divided by 8 bits per byte and rounded up to next integer */

char tfilename[32]; 	/* temp file for mup input */
char t1filename[32];	/* temp file for mup output */
char t2filename[32];	/* temp file for gs output */

struct PARAM {
	char	*mark;		/* HTML NAME tag */
	char	*name;		/* parameter name */
	struct PARAM *next;	/* linked list */
};

/* This stores a list of parameters for each letter of the alphabet */
struct PARAM *param_list[26];

void cleanup(int sig);
void proc_ex(char **fname_p, int *llx_p, int *lly_p, int *urx_p, int *ury_p);
void save_param_info(char *mark, char *name);
void make_param_index(void);

int
main(int argc, char **argv)
{
	char *pfilename;	/* name of PostScript file with mup output */
	FILE *tfile = 0;	/* temp file to hold .Ex - .Ee text */
	char buff[BUFSIZ];	/* for reading lines of input */
	char *p;		/* pointer into buff */
	int llx, lly, urx, ury;	/* bounding box info */
	int line;		/* input line number */
	int n;			/* loop index for signal setting */
	

	/* Generate the names for the temp files. */
	sprintf(tfilename, "/tmp/ugexiM%d", getpid());
	sprintf(t1filename, "/tmp/ugexiP%d", getpid());
	sprintf(t2filename, "/tmp/ugexiB%d", getpid());

	/* Arrange to mop up if something goes haywire. */
	for (n = 1; n < NSIG; n++) {
		sigset(n, cleanup);
	}
	sigset(SIGWINCH, SIG_IGN);
	sigset(SIGCLD, SIG_DFL);

	/* read and translate input */
	for (line = 1; fgets(buff, BUFSIZ, stdin); line++) {

		if (strncmp(buff, ".Ee", 3) == 0) {
			/* if copying an example, process it now */
			if (tfile) {
				fclose(tfile);
				tfile = (FILE *) 0;
				proc_ex(&pfilename, &llx, &lly, &urx, &ury);

				/* replace with .Ee with appropriate arguments */
				printf(".Ee %s %f %f %d %d\n", pfilename,
						(ury - lly) / 72.0,
						(urx - llx) / 72.0,
						ury - lly, urx - llx);
				continue;
			}
		}

		/* copy to output */
		printf("%s", buff);

		/* If we are currently in the middle of a Mup input example,
		 * output into temp file. */
		if (tfile) {

			p = buff;
			/* troff comments inside example are assumed to be
			 * extra mup input needed to make the example work */
			if (strncmp(buff, ".\\\"", 3) == 0) {
				p += 3;
			}
			for (  ; *p != '\0'; p++) {
				if (*p == '\\' && *(p+1) == 'e') {
					/* translate \e to backslash */
					putc('\\', tfile);
					p++;
				}
				else {
					putc(*p, tfile);
				}
			}
		}

		if (strncmp(buff, ".Ex", 3) == 0) {
			/* An .Ex macro with an argument is Mup input to
			 * be processed. We're rather stupid on deciding
			 * if there is an argument: if there is anything on
			 * the line besides the .Ex, assume there is an
			 * argument, even if there is really only white space */
			if (buff[3] != '\n') {
				fprintf(stderr, "%s: processing example on line %d\n", argv[0], line);
				if ((tfile = fopen(tfilename, "w")) == NULL) {
					fprintf(stderr, "can't open temp picture file %s\n", tfilename);
					cleanup(-1);
				}
			}
		}

		if (strncmp(buff, ".Na", 3) == 0) {
			/* This is a parameter. Save info for parameter index */
			char *mark;
			char *name;

			/* get the HTML NAME tag */
			if (fgets(buff, BUFSIZ, stdin) == 0) {
				fprintf(stderr, "line %d: text ends after .Na\n", line);
				cleanup(-1);
			}
			line++;
			if (strncmp(buff, ".Hm", 3) != 0) {
				fprintf(stderr, "line %d: missing .Hm after .Na\n", line);
				cleanup(-1);
			}
			printf("%s", buff);
			buff[strlen(buff)-1] = '\0';
			mark = strdup(buff+4);

			/* get the parameter name */
			if (fgets(buff, BUFSIZ, stdin) == 0) {
				fprintf(stderr, "line %d: missing parameter after .Na\n", line);
				cleanup(-1);
			}
			line++;
			printf("%s", buff);
			buff[strlen(buff)-1] = '\0';
			name = strdup(buff);
			save_param_info(mark, name);
		}

	}
	make_param_index();
	cleanup(0);
	/*NOTREACHED--for lint */
	return(0);
}


/* Once an example has been gathered, process it. Run mup on it. Then run
 * gs and figure out bounding box information. Then create final PostScript
 * file to be included by troff. Arguments are for returning the name of
 * the generated file and the bounding box information in points. */


void
proc_ex(char **fname_p, int *llx_p, int *lly_p, int *urx_p, int *ury_p)
{
	static int fnumber = 1;	/* serial number used in making file names */
	static char namebuff[16]; /* buffer for file name */
	char outfile[50];	/* buffer for -sOutputFile=xxx arg to gs */
	int retval;		/* from child process */
	struct stat sb;		/* to check on temp file size */
	int f;			/* temp file descriptor */
	int i, j;		/* loop indices */
	int leftmost, rightmost, top, bottom;	/* bounding box */
	char buff[BUFSIZ];
	FILE *src, *dest;	/* for copying full mup output to just what
				 * we need for the included picture */


	/* run mup on the temp file, and put in another temp file */
	switch (fork()) {
	case 0:
		close(1);
		if (open(t1filename, O_WRONLY | O_CREAT| O_TRUNC, 0664) < 0) {
			exit(1);
		}
		execlp("mup", "mup", tfilename, (char *) 0);
		/*FALLTHRU*/
	case -1:
		fprintf(stderr, "couldn't run mup\n");
		exit(1);
	default:
		if (wait( &retval ) < 0) {
			fprintf(stderr, "wait returned -1, errno %d\n", errno);
			cleanup(-1);
		}
		if (retval != 0) {
			fprintf(stderr, "mup failed, ret code = %d\n", retval);
			cleanup(-1);
		}
	}

	/* run gs on the mup output to determine bounding box info */
	switch(fork()) {
	case 0:
		close(1);
		open("/dev/null", O_WRONLY, 0666);
		sprintf(outfile, "-sOutputFile=%s", t2filename);
		close(0);
		open(t1filename, O_RDONLY, 0);
		execlp("gs", "gs", "-sDEVICE=bit", "-sPAPERSIZE=letter",
					"-dNOPAUSE", outfile, "-", (char *) 0);
		/*FALLTHRU*/
	case -1:
		fprintf(stderr, "failed to run gs\n");
		exit(1);
	default:
		if (wait( &retval ) < 0) {
			fprintf(stderr, "wait failed on gs\n");
			cleanup(-1);
		}
		if (retval != 0) {
			fprintf(stderr, "gs failed\n");
			cleanup(-1);
		}
		break;
	}

	/* make sure generated file looks okay. 72x72 pixel on 8.5x11 paper
	 * is 77 bytes by 792 rows.  We can't handle multi-page examples. */
	if (stat(t2filename, &sb) != 0) {
		fprintf(stderr, "can't stat %s\n", t2filename);
		cleanup(-1);
	}
	if (sb.st_size != PGWIDTH * PGHEIGHT) {
		fprintf(stderr, "gs output is wrong size\n");
		cleanup(-1);
	}

	if ((f = open(t2filename, O_RDONLY)) < 0) {
		fprintf(stderr, "can't open %s\n", t2filename);
		cleanup(-1);
	}

	/* Find bounding box info. For left/right, work inwards till we find
	 * a non-zero byte. For top/bottom find rows containing non-zero bytes.
	 * Note that Ghostscript now has a special "bbox" device that would
	 * give the bounding box information, but at the time we wrote this
	 * program, either that didn't exist yet, or at least we didn't yet
	 * know about its existence. Since this works, we just leave it.
	 */
	leftmost = PGWIDTH;
	rightmost = -1;
	top = -1;
	for (i = 0; i < PGHEIGHT; i++) {
		/* read a row of bits */
		if (read(f, buff, PGWIDTH) != PGWIDTH) {
			fprintf(stderr, "read error on %s\n", t2filename);
			cleanup(-1);
		}

		/* see if any farther left than we found before */
		for (j = 0; j < leftmost; j++) {
			if (buff[j]) {
				leftmost = j;
				break;
			}
		}

		/* see if any farther right */
		for (j = PGWIDTH - 1; j > rightmost; j--) {
			if (buff[j]) {
				rightmost = j;
				break;
			}
		}

		/* check if anything in row, for finding top/bottom */
		for (j = 0; j < PGWIDTH; j++) {
			if (buff[j]) {
				if (top == -1) {
					top = i;
				}
				bottom = i;
				break;
			}
		}
	}
	close(f);
	if (top < 0) {
		fprintf(stderr, "no output generated\n");
		cleanup(-1);
	}

	/* Add a little padding all around. We already have up to 1/8" of
	 * horizontal padding, since we only checked left/right to within a
	 * byte, not a bit. Since it is just padding, it doesn't have to
	 * be exact, just not too small.
	 */
	*llx_p = (leftmost - 2) * 8;
	*urx_p = (rightmost + 2) * 8;
	/* PostScript goes up positive, so adjust for that. */
	*lly_p = PGHEIGHT - (bottom + 8);
	*ury_p = PGHEIGHT - (top - 8);

	/* create a name for the mup example output file */
	sprintf(namebuff, "mugex%d.ps", fnumber++);
	*fname_p = namebuff;

	/* now copy just the relevant part of the mup output to the
	 * picture output file */
	if ((src = fopen(t1filename, "r")) == NULL) {
		fprintf(stderr, "can't open %s\n", t1filename);
		cleanup(-1);
	}
	if ((dest = fopen(namebuff, "w")) == NULL) {
		fprintf(stderr, "can't open %s\n", namebuff);
		cleanup(-1);
	}

	/* convince gnu troff the file is structured */
	fprintf(dest, "%%!PS-Adobe-1.0\n");
	/* add bounding box comment */
	fprintf(dest, "%%%%BoundingBox: %d %d %d %d\n", *llx_p, *lly_p,
				*urx_p, *ury_p);
	/* Throw away everything in prolog, or up to the user-defined
	 * font stuff, if any, because we need to keep that if it is there. */
	while (fgets(buff, BUFSIZ, src)) {
		if (strncmp(buff, "%%EndProlog", 11) == 0 ||
			strncmp(buff, "% Set up extended character set fonts", 37) == 0 ||
			strncmp(buff, "% Create font of user defined symbols", 37) == 0) {
			break;
		}
	}
	/* copy the rest, except for quit, and the PageSize stuff */
	/* and any Trailer lines */
	while (fgets(buff, BUFSIZ, src)) {
		if (strncmp(buff, "%%BeginFeature", 14) == 0 ||
				strncmp(buff, "%%EndFeature", 12) == 0 ||
				strncmp(buff, "<< /PageSize", 12) == 0 ||
				strncmp(buff, "%%Trailer", 9) == 0 ||
				strncmp(buff, "%%DocumentFonts", 15) == 0 ||
				strncmp(buff, "%%Page", 6) == 0 ||
				strncmp(buff, "quit\n", 5) == 0) {
			continue;
		}
		fprintf(dest, "%s", buff);
	}
	
	fclose(src);
	fclose(dest);
}


/* get rid of temp files and exit */

void
cleanup(int sig)
{
	unlink(tfilename);
	unlink(t1filename);
	unlink(t2filename);
	if (sig > 1) {
		fprintf(stderr, "exiting due to signal %d\n", sig);
	}
	exit(sig);
}


/* save info about a parameter for parameter index */

void
save_param_info(char *mark, char *name)
{
	int index;
	struct PARAM *info;
	struct PARAM **where2link_p_p;

	if ( ! islower(name[0]) ) {
		fprintf(stderr, "name %s for %s does not start with lower case letter\n", name, mark);
		cleanup(-1);
	}

	/* find which letter of the alphabet to file it under */
	index = name[0] - 'a';

	for (where2link_p_p = &(param_list[index]); *where2link_p_p != 0;
				where2link_p_p = &((*where2link_p_p)->next)) {
		;
	}
	if ((info = (struct PARAM *) malloc (sizeof(struct PARAM))) == 0) {
		fprintf(stderr, "malloc failed\n");
		cleanup(-1);
	}
	info->mark = mark;
	info->name = name;
	info->next = 0;
	*where2link_p_p = info;
}


/* generate the parameter index */

void
make_param_index()
{
	char *filename = "param_index";	/* file that tr2html will use */
	FILE *file;			/* for param_index */
	int i;				/* loop through letters of alphabet */
	int label;			/* ASCII value for "A", "B", etc */
	struct PARAM *param_p;		/* to walk list for each letter */


	if ((file = fopen(filename, "w")) == 0) {
		fprintf(stderr, "can't open %s file\n", filename);
		cleanup(-1);
	}

	fprintf(file, "</P>\n<H2>Index of parameters</H2>\n");
	/* generate link for each letter of alphabet that has parameters */
	for (i = 0; i < 26; i++) {
		if (param_list[i] != 0) {
			label = i + 'A';
			fprintf(file, "<A HREF=\"param.html#index%c\">%c</A> ",
							label, label);
		}
	}

	/* generate links for each parameter */
	for (i = 0; i < 26; i++) {
		if (param_list[i] != 0) {
			label = i + 'A';
			fprintf(file, "<H3><A NAME=\"index%c\">%c</A></H3><BR>\n",
							label, label);
			for (param_p = param_list[i]; param_p != 0;
						param_p = param_p->next) {
				fprintf(file, "<A HREF=\"param.html#%s\">%s</A><BR>\n",
						param_p->mark, param_p->name);
			}
		}
	}

	fclose(file);
}
