/*   Show Mup Allocation Debugging Information 
 * This program reads a Mup allocdebug file and prints it in
 * a human readable format.
 *
 * Some day, this could potentially look up the addresses where the
 * allocs/frees were called from, and print them symbolically.
 * For now, the user has to deduce the locations from nm output.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <string.h>
#include <errno.h>

#include "allocdebug.h"
/* The debugging information to print */
struct ALLOC_INFO * Alloc_info;
/* How many possible entries there are. (Those at the end may be unused.) */
int Alloc_info_slots;


/* Memory map the allocdebug file containing the information to print,
 * and set Alloc_info_slots value. */

void
alloc_debug_init(void)

{
	int file;
	struct stat info;


	if ((file = open("allocdebug", O_RDWR, 0660)) < 0) {
		fprintf(stderr, "No allocdebug file\n");
		exit(1);
	}
	if( fstat(file, &info) != 0) {
		fprintf(stderr, "cannot stat allocdebug file\n");
		exit(1);
	}
	Alloc_info = mmap(0, info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
	if (Alloc_info == (void*)-1) {
		fprintf(stderr, "cannot map allocdebug file err %s\n", strerror(errno));
		exit(1);
	}
	Alloc_info_slots = info.st_size / sizeof(struct ALLOC_INFO);
}


int
main()
{
	int i;		/* index though the entries */
	int j;		/* to search backwards for alloc matching a free */
	char *t;	/* AI_* type name as a string */
	void *base;	/* where malloc space begins */
	void *arg_addr;	/* address passed to realloc or free */
	void *ret_value;/* value returned by malloc/calloc/realloc */

	/* Initialize */
	alloc_debug_init();
	printf("%d slots available in allocdebug file, %ld bytes each\n",
			Alloc_info_slots, sizeof(struct ALLOC_INFO));

	/* Because of memory location randomization, different runs
	 * may end up at different addresses, so we print relative
	 * to the first allocated address, so it is easier to compare runs.
	 */
	base = Alloc_info[1].ret_value;

	/* Loop through, printing the information */
	for (i = 0; i < Alloc_info_slots; i++) {
		if (Alloc_info[i].ai_type == 0) {
			/* Reached the end of the slots actually used */
			break;
		}

		/* Set the name of the entry type */
		switch (Alloc_info[i].ai_type) {
		case AI_MALLOC: t = "MALLOC"; break;
		case AI_CALLOC: t = "CALLOC"; break;
		case AI_REALLOC: t = "REALLOC"; break;

		case AI_MALLOCA: t = "MALLOCA"; break;
		case AI_CALLOCA: t = "CALLOCA"; break;
		case AI_REALLOCA: t = "REALLOCA"; break;

		case AI_FREE: t = "FREE"; break;
		case AI_FREE_SKIPPED: t = "FREE [*SKIPPED*]"; break;
		default: fprintf(stderr, "unexpected type %d\n", Alloc_info[i].ai_type);
			exit(1);
			break;
		}

		/* Adjust addresses to be relative to base */
		arg_addr = Alloc_info[i].arg_addr;
		if (Alloc_info[i].arg_addr != 0) {
			arg_addr = (void*) ((long)arg_addr - (long)base);
		}
		ret_value = Alloc_info[i].ret_value;
		if (Alloc_info[i].ret_value != 0) {
			ret_value = (void*) ((long)ret_value - (long)base);
		}

		/* Print the information */
		switch (Alloc_info[i].ai_type) {
		case AI_MALLOC:
		case AI_CALLOC:
		case AI_MALLOCA:
		case AI_CALLOCA:
			printf("[%d] %s: size=%ld, num_elem=%ld, type %s, new_p %s, ret %p, called from %p\n", i, t,
				Alloc_info[i].size,
				Alloc_info[i].num_elem,
				Alloc_info[i].type,
				Alloc_info[i].newp,
				ret_value, Alloc_info[i].call_addr);
				
			break;
		case AI_REALLOC:
		case AI_REALLOCA:
			printf("[%d] %s: size=%ld, num_elem=%ld, type %s, new_p %s, arg %p, ret %p, called from %p\n", i, t,
				Alloc_info[i].size,
				Alloc_info[i].num_elem,
				Alloc_info[i].type,
				Alloc_info[i].newp,
				arg_addr,
				ret_value,
				Alloc_info[i].call_addr);
			break;
		case AI_FREE:
		case AI_FREE_SKIPPED:
			for (j = i-1; j >= 0; j--) {
				if (Alloc_info[j].ai_type == 21) {
					if (Alloc_info[i].arg_addr == Alloc_info[j].arg_addr) {
						printf("  *** double free from slot %d\n", j);
					}
					continue;
				}
				if (Alloc_info[i].arg_addr == Alloc_info[j].ret_value) {
					printf("[%d] %s (#%ld) (%p)  was allocated from slot %d, %s %s, called from %p\n",
						i, t, Alloc_info[i].num_elem,
						(void*)(Alloc_info[i].arg_addr - base),
						j, Alloc_info[j].type,
						Alloc_info[j].newp,
						Alloc_info[j].call_addr);
					break;
				}
			}
			break;
		}
	}
	exit(0);
}
