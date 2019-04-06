/* This task used to be done with a 'sed' script, but it seemed to be
 * too hard to do in a way that would work with both the Mac and gnu sed,
 * so is done in C here. It concatenates the first two file name arguments,
 * and writes to the third, adding a "tranpose" after the first "save" line
 * of the second file, and writes a -gXxY string to stdout. The transpose
 * and -g values are from the %BoundingBox line of the second file. The mkuggifs
 * script then uses the output file and -g string to run Ghostscript and convert
 * the result to a .gif file.
 */

#include <stdio.h>
#include <stdlib.h>

int
main(argc, argv)
int argc;
char **argv;
{
	/* The next 4 are for the BoundingBox value */
	int xoff;
	int yoff;
	int urx;
	int ury;
	int x;
	int y;
	int first;	/* to make sure we add the transpose only after the 
			* first instance of a "save" line */
	char buff[BUFSIZ];
	FILE *in;
	FILE *out;
	int got_bb;	/* to verify we saw the BoundingBox line */

	/* Open output file */
	if ((out = fopen(argv[3], "w")) == 0) {
		fprintf(stderr, "cannot open '%s'\n", argv[3]);
		exit(1);
	}
	/* Copy first input file to output as is */
	if((in = fopen(argv[1], "r")) == 0) {
		fprintf(stderr, "cannot open '%s\n", argv[1]);
		exit(1);
	}
	while (fgets(buff, sizeof(buff), in)) {
		fprintf(out, "%s", buff);
	}
	fclose(in);

	/* Copy second file, but remembering th BoundingBox info encountered
	 * and adding a transpose after the first save. */
	if((in = fopen(argv[2], "r")) == 0) {
		fprintf(stderr, "cannot open '%s'\n", argv[2]);
		exit(1);
	}
	first = 1;
	got_bb = 0;
	while (fgets(buff, sizeof(buff), in)) {
		if (strncmp(buff, "%%BoundingBox:", 14) == 0) {
			if (sscanf(buff+14, "%d %d %d %d", &xoff, &yoff, &urx, &ury) != 4) {
				fprintf(stderr, "failed to get BoundingBox info\n");
				exit(1);
			}
			got_bb = 1;
		}
		if (first && strncmp(buff, "save", 4) == 0 &&
				((buff[4] == '\n') || (buff[4] == '\r'))) {
			if ( ! got_bb ) {
				fprintf(stderr, "No BoundingBox found before first save\n");
				exit(1);
			}
			fprintf(out, "save -%d -%d translate %s", xoff, yoff, buff+4);
			first = 0;
			continue;
		}
		fprintf(out, "%s", buff);
	}
	fclose(in);
	fclose(out);

	/* output the -g option to be passed to Ghostscript */
	x = (urx - xoff) * 3 / 2;
	y = (ury - yoff) * 3 / 2;
	
	printf("-g%dx%d\n", x, y);
	
	exit(0);
}
