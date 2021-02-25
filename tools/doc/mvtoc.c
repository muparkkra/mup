/*
 Copyright (c) 1995-2021  by Arkkra Enterprises.
 All rights reserved.

 Redistribution and use in source and binary forms,
 with or without modification, are permitted provided that
 the following conditions are met:

 1. Redistributions of source code must retain
 the above copyright notice, this list of conditions
 and the following DISCLAIMER.

 2. Redistributions in binary form must reproduce the above
 copyright notice, this list of conditions and
 the following DISCLAIMER in the documentation and/or
 other materials provided with the distribution.

 3. Any additions, deletions, or changes to the original files
 must be clearly indicated in accompanying documentation,
 including the reasons for the changes,
 and the names of those who made the modifications.

	DISCLAIMER

 THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS
 OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* This utility program takes a PostScript file produced by troff,
 * and writes out a new file with the table of contents
 * relocated to before the page containing the heading "1. INTRODUCTION".
 * It is intended for use on the Mup User's Guide,
 * so it's unlikely to work on just any arbitrary troff output.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/* information about a page from the input */
struct Page {
	long	offset;		/* where in input file the page begins */
	long	length;		/* how many lines in page */
	char	pagelabel[8];	/* like "10", or "ii" */
	struct Page *next;	/* linked list */
};


struct Page *Pagelist;		/* the list of all pages */
struct Page **Last_p_p;		/* where current page is attached to Pagelist */

void addpage(long offset, int pagenum, int pagetype);

int
main(int argc, char **argv)
{
	FILE *f;		/* input file */
	int foundtrailer = 0;	/* if found %Trailer */
	long offset;		/* place in file before reading current line */
	struct Page *toc_p;	/* points to beginning of table of contents */
	struct Page **start_p_p;/* where table of contents will be inserted */
	struct Page *intro_p = 0;/* page containing INTRODUCTION line */
	struct Page *page_p;	/* index through Pagelist */
	char buff[BUFSIZ];	/* input/output buffer */
	int n;			/* count of lines output */
	int i;			/* page number of output */
	int len;		/* strlen of an input line */
	int toc_pages = 0;	/* how many pages in table of contents */
	int groff = 0;		/* 1 if processing groff-generated output */
	int marker = 0;		/* special comment marker to tell where
				 * to splice things in */


	if (argc != 2) {
		fprintf(stderr, "usage: %s file.ps\n", argv[0]);
		exit(1);
	}

	if ((f = fopen(argv[1], "r")) == (FILE *) 0) {
		fprintf(stderr, "can't open %s\n", argv[1]);
		exit(1);
	}

	/* init list for prolog */
	addpage(0L, 0, 0);

	/* start at beginning of file, and read to end */
	offset = 0;
	toc_p = (struct Page *) 0;
	while (fgets(buff, BUFSIZ, f)) {

		if (strncmp(buff, "%%Page: ", 8) == 0) {
			/* found beginning of a page. save info about it */
			if (toc_p != 0) {
				addpage(offset, toc_pages + 1, 1);
			}
			else if (intro_p == 0 && atoi(buff+10) < 3) {
				addpage(offset, atoi(buff+8), 2);
			}
			else {
				addpage(offset, atoi(buff+8), 0);
			}
			if (marker == 1) {
				/* This should be first real page of document.
				 * Save location,  so we can attach
				 * table of contents later */
				start_p_p = Last_p_p;
				intro_p =  *Last_p_p;
				marker = 0;
			}
			else if (marker == 2) {
				/* found beginning of table of contents. Keep
				 * track of where it is in list */
				toc_p = *Last_p_p;
				strcpy(toc_p->pagelabel, "i");
				marker = 0;
			}
		}

		else if (strncmp(buff, "%%Trailer", 9) == 0) {

			foundtrailer = 1;

			/* splice table of contents before intro */
			(*Last_p_p)->next = *start_p_p;

			if (toc_p == 0) {
				fprintf(stderr, "%s: failed to find table of contents\n", argv[0]);
				exit(1);
			}
			*start_p_p = toc_p;

			/* reset to end of list and add trailer */
			for (Last_p_p = &(toc_p->next);
					*Last_p_p != 0 &&
					(*Last_p_p)->next != toc_p;
					Last_p_p = &((*Last_p_p)->next)) {
				;
			}
			addpage(offset, 0, 0);
		}

		else if (strncmp(buff, "(- i -)", 7) == 0) {
			/* found beginning of table of contents. Keep
			 * track of where it is in list */
			toc_p = *Last_p_p;
			strcpy(toc_p->pagelabel, "i");
		}

		else if (intro_p == 0 &&
				strncmp(buff, "(1. INTRODUCTION)", 17) == 0) {
			/* found first page of document. Save location,
			 * so we can attach table of contents later */
			start_p_p = Last_p_p;

			intro_p =  *Last_p_p;
		}
		else if (strncmp(buff, "%-marker", 8) == 0) {
			groff = 1;
			marker = atoi(buff+8);
		}

		/* count number of TOC pages */
		if (toc_p != 0 && (strncmp(buff, "%%EndPage: ", 11) == 0 ||
				strncmp(buff, "%%BeginPageSetup", 16) == 0)) {
			toc_pages++;
		}

		/* update file offset */
		offset = ftell(f);
		((*Last_p_p)->length)++;
	}

	/* do a little check to make sure things worked */
	if (intro_p == 0) {
		fprintf(stderr, "%s: failed to find INTRODUCTION\n", argv[0]);
		exit(1);
	}

	if (foundtrailer == 0) {
		fprintf(stderr, "%s: failed to find trailer\n", argv[0]);
		exit(1);
	}

	/* okay. now we know where everything is and have the table of
	 * contents in the right place on the list. Write out file in
	 * new order */
	for (i = 0, page_p = Pagelist; page_p != (struct Page *) 0;
							page_p = page_p->next) {

		/* go to where page begins */
		fseek(f, page_p->offset, SEEK_SET);

		/* copy the page to output */
		for (n = 0; n < page_p->length; n++) {
			if (fgets(buff, BUFSIZ, f) == 0) {
				fprintf(stderr, "unexpected end of file\n");
				exit(1);
			}

			/* patch up the Page line with proper number */
			if (strncmp(buff, "%%Page: ", 8) == 0) {
				printf("%%%%Page: %s %d\n", page_p->pagelabel, ++i);
			}
			else if (strncmp(buff, "%%EndPage: ", 11) == 0) {
				/* patch up EndPage line with correct number */
				printf("%%%%EndPage: %s %d\n", page_p->pagelabel, i);
			}
			/* Patch up Pages: to account for extra blank page
			 * when odd # of pages in table of contents. */
			else if (strncmp(buff, "%%Pages: ", 9) == 0 && groff
						&& (toc_pages & 0x1)) {
				int tot_pages;
				tot_pages = atoi(buff + 9) + 1;
				printf("%%%%Pages: %d\n", tot_pages);
			}
			else {
				/* patch up any pagesetup commands */
				len = strlen(buff);
				if (isdigit(*buff) && len >= 12 && strcmp(buff + len - 11, " pagesetup\n") == 0) {
					printf("%d pagesetup\n", i);
				}
				else {
					/* output everything else as is */
					printf("%s", buff);
				}
			}
		}

		/* If we are at the end of the table of contents and it
		 * had an odd number of pages, add a blank page, so
		 * document body will start on a right-hand page. */
		if (page_p->next == intro_p && (toc_pages & 1)) {
			char* pglabel;

			if (strcmp(page_p->pagelabel, "iii") == 0){
				pglabel = "iv";
			}
			else {
				pglabel = "?";
			}
			printf("%%%%Page: %s %d\n", pglabel, ++i);
			printf("save\nmark\n");
			if (groff) {
				printf("200 200 moveto\n/TimesRoman findfont 10 scalefont setfont ([This page intentionally left blank]) show\n");
			}
			else {
				printf("%d pagesetup\n", i);
				printf("10 R f\n([This page intentionally blank]) 2200 5000 w\n");
			}
			printf("cleartomark\nshowpage\nrestore\n");
		}

	}

	return(0);
}


/* allocate space for info about a new page and fill it in */

void
addpage(long offset, int pagenum, int pagetype)
{
	/* figure out where to attach to Pagelist */
	if (Last_p_p == (struct Page **) 0) {
		Last_p_p = &Pagelist;
	}
	else {
		Last_p_p = &( (*Last_p_p)->next);
	}

	/* allocate space */
	if ((*Last_p_p = (struct Page *) malloc(sizeof(struct Page))) ==
					(struct Page *) 0) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	/* fill it it */
	(*Last_p_p)->offset = offset;
	(*Last_p_p)->length = 0;
	if (pagetype == 1) {
		char * label;
		switch(pagenum) {
		case 2:
			label = "ii";
			break;
		case 3:
			label = "iii";
			break;
		case 4:
			label= "iv";
			break;
		case 5:
			label = "v";
			break;
		default:
			label = "?";
			break;
		}
		strcpy((*Last_p_p)->pagelabel, label);
	}
	else if (pagetype == 2) {
		if (pagenum == 0)  {
			pagenum = 2;
		}
		sprintf((*Last_p_p)->pagelabel, "T-%d", pagenum);
	}
	else {
		sprintf((*Last_p_p)->pagelabel, "%d", pagenum);
	}
	(*Last_p_p)->next = (struct Page *) 0;
}
