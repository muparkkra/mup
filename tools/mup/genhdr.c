/* This program generates .h files from specially formatted .h.in files.
 * We used to do this with small awk programs, but have done here in C
 * to remove any dependencies on awk.
 * The first script was for creating muschar.h and extchar.h,
 * the original awk script was:
 *	/_table/ { n = 32 }
 *	/^#define/ {
 *			if (length($$2) < 7) {
 *				pad = "\t"
 *			}
 *			else {
 *				pad = ""
 *			}
 *			$3 = pad "\t(" n ")\t"
 *			n++
 *		}
 *		{print}
 *
 * For any line containing the string "_table" the index number
 * is initialized to 32. (So it looks like an ASCII character beyond the
 * control characters.)
 * Then for any lines in the input that start with
 *	#define XXXX	(%)
 * the % is replaced by the next index number.
 * Everything else is passed through as is.
 *
 * The second script was used for generating ssvused.h,
 * and the original awk program was:
 * 
 *	BEGIN {n=0}
 *	/^[A-Z]/ {
 *			if (length>15)
 *				t="\t"
 *			else if (length>7)
 *				t="\t\t"
 *			else
 *				t="\t\t\t"
 *			printf "#define\t%s%s(%d)\n", $0, t, n++
 *			next
 *		}
 *		{ print $0 }
 *
 * Any line starting with an upper case letter is transformed to
 *	#define XXX (N)
 * where XXX was the original line and N is the next number.
 * Everything else is passed through as is. 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MODE_CHAR 0
#define MODE_USED 1

int
main(int argc, char **argv)
{
	FILE *in;
	FILE *out;
	int mode;
	char buff[BUFSIZ];
	char *val;
	char *tabs;
	int leng;
	int n;

	if (argc != 4) {
		(void) fprintf(stderr, "usage: %s {char|used} infile outfile\n", argv[0]);
		exit(1);
	}

	if (strcmp(argv[1], "char") == 0) {
		mode = MODE_CHAR;
	}
	else if (strcmp(argv[1], "used") == 0) {
		mode = MODE_USED;
	}
	else {
		(void) fprintf(stderr, "first argument must be char or used\n");
		exit(1);
	}

	if ((in = fopen(argv[2], "r")) == 0) {
		(void) fprintf(stderr, "can't open %s\n", argv[2]);
		exit(1);
	}
	if ((out = fopen(argv[3], "w")) == 0) {
		(void) fprintf(stderr, "can't open %s\n", argv[3]);
		exit(1);
	}

	n = 0;
	while (fgets(buff, sizeof(buff), in)) {
		if (mode == MODE_CHAR){
			if (strstr(buff, "_table") != 0) {
				n = 32;
			}
			if (strncmp(buff, "#define", 7) == 0) {
				if ((val = strchr(buff+8, '%')) == 0) {
					(void) fprintf(stderr, "missing expected %% on #define\n");
					exit(1);
				}
				*val = '\0';
				(void) fprintf(out, "%s%d%s", buff, n, val + 1);
				n++;
				continue;
			}
		}
		if ((mode == MODE_USED) && isupper(buff[0])) {
			leng = strlen(buff);
			/* zap newline */
			buff[leng-1] = '\0';
			leng--;

			if (leng  > 15) {
				tabs = "\t";
			}
			else if (leng > 7) {
				tabs = "\t\t";
			}
			else {
				tabs = "\t\t\t";
			}
			(void) fprintf(out, "#define\t%s%s(%d)\n", buff, tabs, n);
			n++;
			continue;
		}
		(void) fprintf(out, "%s", buff);
	}

	(void) fclose(in);
	(void) fclose(out);
}
