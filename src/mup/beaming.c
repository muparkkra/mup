
/*
 Copyright (c) 1995-2024  by Arkkra Enterprises.
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

/* This file contains functions called at parse time to deal with beaming. */


#include "defines.h"
#include "structs.h"
#include "globals.h"


static void subbeam P((struct SSV *ssv_p, RATIONAL outer_time, int obi,
	struct GRPSYL *begin_gs_p, struct GRPSYL *end_gs_p));
static struct GRPSYL *verify_crossbeam P((struct GRPSYL *gs_p,
	struct GRPSYL *other_gs_p, RATIONAL start_time, RATIONAL *end_time_p,
	int staffno, int size));
static void
slopelencheck P((struct GRPSYL *first_p, struct GRPSYL *last_p, char *bmtype));


/* this function will get called whenever a group is not the start or end
 * of a custom beam group. This will fill in the beamloc based on past
 * history. If we were most recently in a custom beam group, we still are.
 * If we weren't before, we aren't now either. The non-custom beaming
 * gets done later after we have collected a whole bar in do_beaming()
 * which is called via do_bar(). */

void
setbeamloc(curr_grp_p, last_grp_p)

struct GRPSYL *curr_grp_p;	/* the group we're working on */
struct GRPSYL *last_grp_p;	/* the last group we did */

{
	if (curr_grp_p->grpvalue == GV_ZERO) {
		/* grace notes don't get handled here */
		curr_grp_p->beamloc = NOITEM;
		return;
	}

	/* if the previous group is a grace group, that doesn't count. Back
	 * up in the list to the first non-grace or beginning of list */
	for (  ; last_grp_p != (struct GRPSYL *) 0
					&& last_grp_p->grpvalue == GV_ZERO;
					last_grp_p = last_grp_p->prev) {
		;
	}

	if (last_grp_p != (struct GRPSYL *) 0) {

		switch (last_grp_p->beamloc) {

		case STARTITEM:
		case INITEM:
			curr_grp_p->beamto = last_grp_p->beamto;
			/* Make sure notes are 8th or shorter. Spaces (which
			 * are allowed in cross-staff beams) can be longer. */
			if (curr_grp_p->basictime < 8) {
				/* At this point, spaces are still
				 * pseudo-pitches, so have to figure out
				 * if this group is really notes */
				int letter;	/* pitch */
				int n;		/* index through notelist */
				for (n = 0; n < curr_grp_p->nnotes; n++) {
					letter = curr_grp_p->notelist[n].letter;
					if ((letter >= 'a' && letter <= 'g') ||
							letter == PP_NO_PITCH) {
						yyerror("beamed notes must be 8th or shorter");
						break;
					}
				}
			}

			/* if previous began beaming, we must be inside now */
			/* if in beam before, we still are */
			curr_grp_p->beamloc = INITEM;
			break;

		case ENDITEM:
		case NOITEM:
		default:
			/* if previous was ending beaming, or
			 * we weren't inside a beam, must be no beam now */
			curr_grp_p->beamloc = NOITEM;
			break;
		}
	}
	else {
		/* nothing specified before, so no beam */
		curr_grp_p->beamloc = NOITEM;
	}
}


/* take a quick jaunt through a GRPSYL list, seeing if custom
 * beaming was already done. If so, return YES, otherwise NO.
 * If there are any non-grace groups with beaming info, there must
 * have been custom beaming. Also, verify that user didn't attempt to
 * custom beam a mixture of normal and cue size notes,
 * or try to put illegal rests or spaces inside a beam.
 * Spaces are only allowed in cross-staff beams, unless user explicitly
 * says to beam across spaces. Rests are also only allowed in beams if
 * the user explicitly states they should be. Rests and spaces cannot be on
 * the ends, and must be eighth note or shorter.
 * We allow beaming together chords that are stemmed to another staff
 * only if all such instances are to the same staff--not some to staff above
 * and others to staff below--because beaming involving 3 staffs at once is
 * just too hard to deal with.
 * Also check that any esbm has at least 2 notes before and after it.
 * This function also checks for abm/eabm, as marked by the autobeam field in
 * GRPSYL having STARTITEM and ENDITEM, and sets the autobeam field in the
 * remaining ones appropriately.
 * YES is returned if autobeaming needs to be done, NO if not.
 */

int
needs_auto_beaming(grp_p)

struct GRPSYL *grp_p;	/* list of GRPSYLs to check */

{
	int has_cust = NO;
	int size = GS_NORMAL;
	int numnotes = 0;	/* how many notes groups, for esbm check */
	short stemto = CS_SAME;	/* to check for mixed CS_ABOVE/CS_BELOW */
	struct GRPSYL *start_p = 0;	/* first beamed group */
	struct GRPSYL *grpsyl_p;
	int has_explicit_abm = NO;
	int in_autobeam = NO;
	int in_custom = NO;


	for (grpsyl_p = grp_p; grpsyl_p != (struct GRPSYL *) 0;
				grpsyl_p = grpsyl_p->next) {

		/* Handle abm / eabm (autobeaming of a portion of measure) */
		if (grpsyl_p->autobeam == STARTITEM) {
			if (in_autobeam == YES) {
				l_yyerror(grpsyl_p->inputfile,
					grpsyl_p->inputlineno,
					"abm cannot be nested");
			}
			if (in_custom == YES) {
				l_yyerror(grpsyl_p->inputfile,
					grpsyl_p->inputlineno,
					"abm cannot overlap with bm");
			}
			has_explicit_abm = YES;
			in_autobeam = YES;
		}
		else if (grpsyl_p->autobeam == ENDITEM) {
			if (in_autobeam == NO) {
				l_yyerror(grpsyl_p->inputfile,
				grpsyl_p->inputlineno, "eabm without abm");
			}
			in_autobeam = NO;
		}
		else if (in_autobeam == YES) {
			grpsyl_p->autobeam = INITEM;
		}
		else {
			grpsyl_p->autobeam = NOITEM;
		}

		if ((grpsyl_p->grpvalue != GV_ZERO)
				&& (grpsyl_p->beamloc != NOITEM)) {
			/* have non-grace with beam info set */
			has_cust = YES;

			/* check for size or cross-staff stem mixtures */
			if (grpsyl_p->beamloc == STARTITEM) {
				size = grpsyl_p->grpsize;
				numnotes = 0;
				stemto = CS_SAME;
			}
			/* check for size mixture. But only do non-cross-staff
			 * beams here, because it's a lot easier to do the
			 * cross-staff beam check in chk_crossbeams() */
			else if (grpsyl_p->grpsize != size
					&& grpsyl_p->beamto == CS_SAME) {
				l_yyerror(grpsyl_p->inputfile,
					grpsyl_p->inputlineno,
					"can't beam normal and cue notes together");
			}

			if (grpsyl_p->grpcont != GC_NOTES) {
				if (grpsyl_p->grpcont == GC_REST) {
					if (grpsyl_p->basictime < 8) {
						l_yyerror(grpsyl_p->inputfile,
							grpsyl_p->inputlineno,
							"rests inside a beam must be less than quarter note duration");
					}
					if (grpsyl_p->beamloc != INITEM) {
						l_yyerror(grpsyl_p->inputfile,
							grpsyl_p->inputlineno,
							"beam cannot %s with a rest",
						grpsyl_p->beamloc == STARTITEM ?
						"begin" : "end");
					}
				}
				else if (grpsyl_p->beamto == CS_SAME) {
					if (grpsyl_p->beamloc != INITEM) {
						l_yyerror(grpsyl_p->inputfile,
						grpsyl_p->inputlineno,
						"beam cannot begin or end with a space");
					}
					if (grpsyl_p->basictime < 8) {
						l_yyerror(grpsyl_p->inputfile,
						grpsyl_p->inputlineno,
						"spaces inside a beam must be less than quarter note duration");
					}
				}
			}
			else if (grpsyl_p->grpvalue != GV_ZERO) {
				numnotes++;
			}

			if (grpsyl_p->stemto != CS_SAME) {
				if (stemto != CS_SAME && grpsyl_p->stemto
								!= stemto) {
					l_yyerror(grpsyl_p->inputfile,
					grpsyl_p->inputlineno,
					"beam cannot include chords with stems to both above and below staffs");
				}
				stemto = grpsyl_p->stemto;
			}

			if (grpsyl_p->beamloc == INITEM &&
					IS_STEMLEN_KNOWN(grpsyl_p->stemlen)) {
				l_yyerror(grpsyl_p->inputfile,
					grpsyl_p->inputlineno,
					"stem len specification not allowed inside a beam");
			}
	
			if (grpsyl_p->beamloc == STARTITEM) {
				if (in_autobeam == YES) {
					l_yyerror(grpsyl_p->inputfile,
						grpsyl_p->inputlineno,
						"abm cannot overlap with bm");
				}
				in_custom = YES;
				start_p = grpsyl_p;
			}
			else if (grpsyl_p->beamloc == ENDITEM) {
				in_custom = NO;
				if (start_p != 0) {
					slopelencheck(start_p, grpsyl_p, "beam");
					start_p = 0;
				}
			}

			if (grpsyl_p->breakbeam == YES
					&& grpsyl_p->beamto == CS_SAME) {
				if (numnotes < 2) {
					l_warning(grpsyl_p->inputfile,
						grpsyl_p->inputlineno,
						"esbm must be preceded by at least 2 beamed notes");
					grpsyl_p->breakbeam = NO;
				}
				else {
					struct GRPSYL *g_p;

					/* Check that there are
					 * at least 2 following beamed notes */
					numnotes = 0;
					for (g_p = grpsyl_p->next; g_p != 0;
							g_p = g_p->next) {
						if (g_p->grpcont == GC_NOTES) {
						    if (g_p->grpvalue == GV_ZERO) {
							continue;
						    }
						    else {
							numnotes++;
						    }
						}
						if (g_p->breakbeam == YES) {
							break;
						}
						if (g_p->beamloc == ENDITEM) {
							break;
						}
					}
					if (numnotes < 2) {
						l_warning(grpsyl_p->inputfile,
							grpsyl_p->inputlineno,
							"esbm must be followed by at least 2 beamed notes");
						grpsyl_p->breakbeam = NO;
					}
					else if (grpsyl_p->grpcont != GC_NOTES) {
						/* User really should have put
						 * the esbm on the preceding
						 * NOTES group. We'll be nice
						 * and move it for them.
						 */
						grpsyl_p->breakbeam = NO;
						for (g_p = grpsyl_p->prev;
								g_p->grpcont != GC_NOTES;
								g_p = g_p->prev) {
							;
						}
						g_p->breakbeam = YES;
					}
				}
				numnotes = 0;
			}
		}
	}

	if (in_autobeam == YES) {
		l_warning(grp_p->inputfile, grp_p->inputlineno,
			"missing eabm");
	}

	/* If implicit autobeaming of entire measure, mark the first group as
	 * starting autobeam, the last as ending it, and those in between
	 * as being in it. That way, do_beaming() doesn't have to directly
	 * care if the whole measure or only part of it is autobeamed.
	 * However, don't mark if there is only a single group.
	 */
	if (has_cust == NO && has_explicit_abm == NO && grp_p->next != 0) {
		for (grpsyl_p = grp_p; grpsyl_p != 0; grpsyl_p = grpsyl_p->next) {
			/* We could skip grace groups, but doesn't seem
			 * worth the check. */
			if (grpsyl_p == grp_p) {
				grpsyl_p->autobeam = STARTITEM;
			}
			else if (grpsyl_p->next == 0) {
				grpsyl_p->autobeam = ENDITEM;
			}
			else {
				grpsyl_p->autobeam = INITEM;
			}
		}
	}

	/* If there was explicit abm, auto beaming will need to be done */
	if (has_explicit_abm == YES) {
		return(YES);
	}
	/* If there was custom beaming without abm, then no auto beaming */
	return(has_cust ? NO : YES);
}


/* beam notes together according to user-specified default beaming style */

void
do_beaming(gs_p, grpsize, staffno, vno)

struct GRPSYL *gs_p;		/* list of GRPSYLs to do beaming on */
int grpsize;			/* GS_NORMAL or GS_SMALL
				 * (grace are handled separately) */
int staffno;
int vno;			/* voice number */

{
	struct SSV *ssv_beaminfo_p;	/* ssv having relevent beam info */
	register int n;			/* index into beamstyle list */
	RATIONAL styletime;		/* accumulated time to beam together */
	RATIONAL tot_time;		/* cumulative grpsyl time */
	struct GRPSYL *first_p;		/* first in beam group */
	struct GRPSYL *last_p;		/* last in beam group */
	int stop;			/* YES if need to stop beaming */
	int beamrests;			/* if to include rests inside beams */
	int beamspaces;			/* if to include spaces inside beams */
	short stemto = CS_SAME;		/* check for mixed CS_ABOVE/CS_BELOW */
	short restart = NO;		/* YES if could start another beam
					 * with current group, even though
					 * it can go with previous */



	debug(4, "do_beaming file=%s line=%d grpsize=%d staff=%d voice=%d",
		gs_p->inputfile, gs_p->inputlineno, grpsize, staffno, vno);

	/* if no default beaming scheme for this voice, then nothing to do--
	 * any custom beaming would have already been done */
	ssv_beaminfo_p = vvpath(staffno, vno, BEAMSTLIST);
	if (ssv_beaminfo_p->nbeam == 0) {
		return;
	}

	/* ok. We may need to do some beaming. Go through the beamstlist and
	 * see if there are any groups to beam together */

	/* initialize */
	/* point to first non-grace group */
	for (  ; gs_p != (struct GRPSYL *) 0 && gs_p->grpcont == GC_NOTES
			&& gs_p->grpvalue == GV_ZERO; gs_p = gs_p->next) {
		;
	}

	/* if no groups, nothing to do */
	if (gs_p == (struct GRPSYL *) 0) {
		return;
	}

	beamrests = vvpath(staffno, vno, BEAMSTLIST)->beamrests;
	beamspaces = vvpath(staffno, vno, BEAMSTLIST)->beamspaces;
	styletime = tot_time = Zero;
	for (n = 0; n < ssv_beaminfo_p->nbeam; n++) {
		styletime = radd(styletime, ssv_beaminfo_p->beamstlist[n]);

		if (GE(tot_time, styletime)) {
			/* we're already past this beamstyle segment */
			continue;
		}

		for (first_p = last_p = 0, stop = NO;
				LT(tot_time, styletime); gs_p = gs_p->next) {

			if (gs_p == 0) {
				/* Must be too few groups in measure.
				 * This error will already have been
				 * reported elsewhere.
				 */
				return;
			}

			/* ignore grace */
			while (gs_p->grpvalue == GV_ZERO) {
				gs_p = gs_p->next;
				if (gs_p == 0) {
					/* Must have tried to end a measure
					 * with grace. Already reported. */
					return;
				}
			}

			tot_time = radd(tot_time, gs_p->fulltime);
			if (GE(tot_time, styletime)) {
				/* This group puts us at or past
				 * the current beamstyle segment */
				stop = YES;
			}

			if (gs_p->autobeam == NOITEM) {
				stop = YES;
			}
			/* only 8th and shorter get beamed */
			if (gs_p->basictime < 8 || (gs_p->grpcont == GC_SPACE &&
						beamspaces == NO) ) {
				stop = YES;
			}
			else if (gs_p->grpcont == GC_REST && beamrests == NO) {
				stop = YES;
			}
			else if (gs_p->grpsize != grpsize) {
				/*  Wrong size to beam on this call */
				stop = YES;
			}
			else if (gs_p->stemto != CS_SAME && stemto != CS_SAME
					&& gs_p->stemto != stemto) {
				/* We don't allow beaming across three staffs,
				 * so have to stop current beam, but could
				 * possibly beam this group with following
				 * groups, as long as they don't have a
				 * conflicting stemto */
				stop = YES;
				restart = YES;
			}
			else if (gs_p->grpcont == GC_NOTES && 
						(gs_p->autobeam != NOITEM) &&
						LE(tot_time, styletime)) {
				/* found something beam-able */
				if (first_p == 0) {
					first_p = gs_p;
				}
				last_p = gs_p;
			}
			if (gs_p->stemto != CS_SAME) {
				stemto = gs_p->stemto;
			}

			if (stop == YES) {
				if (first_p != 0 && last_p != 0
							&& first_p != last_p) {
					/* Disallow illegal combinations of
					 * slope and stem length */
					slopelencheck(first_p, last_p, "beam");

					/* If there are subbeam groupings,
					 * do those. */
					subbeam(ssv_beaminfo_p,
						rsub(styletime, ssv_beaminfo_p->beamstlist[n]),
						n, first_p, last_p);

					/* mark beginning of beam group */
					first_p->beamloc = STARTITEM;

					/* mark all intermediate groups,
					 * skipping grace */
					for (first_p = first_p->next;
							first_p != last_p;
							first_p = first_p->next) {

						if (first_p->grpvalue
								== GV_ZERO) {
							continue;
						}

						first_p->beamloc = INITEM;

						if (IS_STEMLEN_KNOWN(first_p->stemlen)) {
							l_yyerror(first_p->inputfile, first_p->inputlineno,
							    "stem len specification not allowed inside a beam");
						}
					}

					/* mark the end of the beam group */
					last_p->beamloc = ENDITEM;
				}

				/* Re-init for any more bunches to beam */
				first_p = last_p = 0;
				stop = NO;
				stemto = CS_SAME;
				if (restart == YES) {
					if (gs_p->grpcont == GC_NOTES) {
						first_p = gs_p;
					}
					restart = NO;
				}
			}
		}
	}
}


/* Once a STARTITEM and ENDITEM groups of the regular beamstyle
 * have been identified, go through them to see if there should
 * be subgroups. If so, mark breakbeam = YES on the last group of
 * each subgroup.
 */

static void
subbeam(ssv_p, outer_time, obi, begin_gs_p, end_gs_p)

struct SSV *ssv_p;	/* to get beamstlist and subbeamstlist */
RATIONAL outer_time;	/* Time in measure when outer beam begins */
int obi;		/* outer beam index, subscript into ssv_p->beamstlist */
struct GRPSYL *begin_gs_p;
struct GRPSYL *end_gs_p;

{
	int sbi;	/* sub beam index, subscript of ssv_p->subbeamstlist */
	RATIONAL subgroup_time;	/* duration of items in subbeamstlist */
	RATIONAL tot_time;	/* sum of note groups in subbeaming */
	struct GRPSYL *gs_p;	/* walk through groups */
	struct GRPSYL *last_notegroup_p;/* Most recent GC_NOTES GRPSYL */


	/* Check if more than one beam subgroup
	 * makes up the outer beam grouping. */
	if (ssv_p->nbeam == ssv_p->nsubbeam) {
		/* There are no sub-beam groupings anywhere in the measure */
		return;
	}

	/* Find the subbeamlist entry that matches with the outer beam entry */
	subgroup_time = Zero;
	for (sbi = 0; LT(subgroup_time, outer_time); sbi++) {
		subgroup_time = radd(subgroup_time, ssv_p->subbeamstlist[sbi]);
	}

	if ( EQ(ssv_p->beamstlist[obi], ssv_p->subbeamstlist[sbi]) ) {
		/* Outer and subbeam have the same time duration,
		 * so there aren't any subgroups in this outer beam grouping. */
		return;
	}

	/* There are subgroups inside the outer beam grouping,
	 * so we may need to set one or more breakbeams. */
	subgroup_time = ssv_p->subbeamstlist[sbi];

	/* If beam starts later than the outer beamstyle item begins,
	 * (e.g., if there was a rest at the beginning of the beam time),
	 * we have to count that time as already taken up from the subbeam.
	 */
	for (tot_time = Zero, gs_p = begin_gs_p->prev; gs_p != 0;
							gs_p = gs_p->prev) {
		tot_time = radd(tot_time, gs_p->fulltime);
	}
	tot_time = rsub(tot_time, outer_time);
	last_notegroup_p = 0;
	for (gs_p = begin_gs_p; gs_p != end_gs_p; gs_p = gs_p->next) {

		/* Grace notes are irrelevant */
		if (gs_p->grpvalue == GV_ZERO) {
			continue;
		}

		/* Remember where last note group is, in case we
		 * need to set breakbeam on it. */
		if (gs_p->grpcont == GC_NOTES) {
			last_notegroup_p = gs_p;
		}

		/* Add up group time values until the total equals
		 * or exceeds that of the subgroup. */
		tot_time = radd(tot_time, gs_p->fulltime);
		if (LT(tot_time, subgroup_time)) {
			/* not far enough yet */
			continue;
		}
	
		/* If the value exceeds, there is a note spanning the
		 * subgroup boundary, so just ignore the subgrouping. */
		if (GT(tot_time, subgroup_time)) {
			tot_time = rsub(tot_time, subgroup_time);
			subgroup_time = ssv_p->subbeamstlist[++sbi];
			if (gs_p->grpcont != GC_NOTES) {
				last_notegroup_p = 0;
			}
		}

		else {
			/* A group ends right at the subbeam boundary. 
			 * Set breakbeam on last group, if there was one. 
			 */
			if (last_notegroup_p != 0) {
				last_notegroup_p->breakbeam = YES;
			}

			/* The current subbeam is finished.
			 * Move on to the next subbeam, if there is one. */
			if (++sbi < ssv_p->nsubbeam) {
				subgroup_time = ssv_p->subbeamstlist[sbi];
				/* Since we know the subbeam we just
				 * finished ended exactly
				 * at the subbeam boundary,
				 * we set to time taken up so far
				 * by the new subbeam to zero. 
				 */
				tot_time = Zero;
				last_notegroup_p = 0;
			}
		}
	}
}


/* alt groups must always have beamloc set, so fix them */

void
set_alt_beams(gs_p)

struct GRPSYL *gs_p;	/* a measure's worth of GRPSYLs for a voice */

{
	struct GRPSYL *other_gs_p;	/* group on other end of alt pair */


	debug(4, "set_alt_beams file=%s line=%d",
			gs_p->inputfile, gs_p->inputlineno);

	/* walk through the list, fixing any alt groups */
	for (   ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		/* check if is an alt pair */
		if (gs_p->slash_alt < 0) {

			if (gs_p->next == (struct GRPSYL *) 0) {
				/* no second group in alt, will be flagged
				 * elsewhere */
				continue;
			}

			/* set the pair as a beam group */
			gs_p->beamloc = STARTITEM;
			gs_p->next->beamloc = ENDITEM;

			slopelencheck(gs_p, gs_p->next, "alt");

			/* middle phase wants to have both notes in an alt group
			 * have their alt field set, so do that */
			gs_p->next->slash_alt = gs_p->slash_alt;

			/* adjust preceding and following groups if necessary.
			 * If was already in a beam group, split off the other
			 * parts into their own groups or put flags on the
			 * extras if they are down to one group */

			/* find previous normal group if any and adjust */
			for (other_gs_p = gs_p->prev;
					other_gs_p != (struct GRPSYL *) 0;
					other_gs_p = other_gs_p->prev) {

				if (other_gs_p->grpvalue != GV_ZERO) {
					if (other_gs_p->grpcont == GC_REST) {
						other_gs_p->beamloc = NOITEM;
					}
					else {
						break;
					}
				}
			}

			if (other_gs_p != (struct GRPSYL *) 0) {

				switch (other_gs_p->beamloc) {

				case INITEM:
					other_gs_p->beamloc = ENDITEM;
					break;

				case STARTITEM:
					other_gs_p->beamloc = NOITEM;
					break;

				default:
					break;
				}
			}
					
			/* now do the same for the following group */
			for (other_gs_p = gs_p->next->next;
					other_gs_p != (struct GRPSYL *) 0;
					other_gs_p = other_gs_p->next) {

				if (other_gs_p->grpvalue != GV_ZERO) {
					if (other_gs_p->grpcont == GC_REST) {
						other_gs_p->beamloc = NOITEM;
					}
					else {
						break;
					}
				}
			}

			if (other_gs_p != (struct GRPSYL *) 0) {

				switch (other_gs_p->beamloc) {

				case INITEM:
					other_gs_p->beamloc = STARTITEM;
					break;

				case ENDITEM:
					other_gs_p->beamloc = NOITEM;
					break;

				default:
					break;
				}
			}

			/* skip over the second in the pair */
			gs_p = gs_p->next;
		}
	}
}


/* Given a list of GRPSYLs on a visible voice
 * having "bm with staff below" do all the error checking.
 * This list of groups has to be for the bottom visible voice
 * for the duration of the beam.
 * There has to be a set of groups on the top non-space visible
 * voice of the first visible staff below,
 * which starts a "bm with staff above" at exactly
 * the same time value. The ebm values also have to match. At every
 * point inside the beam, one voice must have notes and the other voice
 * must have spaces.
 *
 * Have to be careful in this function,
 * because the gs_p->staffno and gs_p->vno may not be filled in yet,
 * so have to use the staffno from the STAFF struct, and get vno from the
 * first gs_p, which the caller is supposed to have filled in correctly.
 *
 * Returns the staff number of the staff containing the matching
 * "bm with staff above" or -1 if no such staff was found.
 */

int
chk_crossbeam(gs_p, mll_p)

struct GRPSYL *gs_p;	/* first group in above voice of cross staff beam */
struct MAINLL *mll_p;	/* gs_p hangs off of here */

{
	struct GRPSYL *g_p;		/* for walking through group list */
	struct GRPSYL *other_p;		/* group on other staff */
	struct MAINLL *assoc_mll_p;	/* other staff hangs off of here */
	struct GRPSYL *assoc_grps_p;	/* the measure-worth of groups in
					 * the voice being beamed to */
	RATIONAL start_time, end_time;	/* of the above voice */
	RATIONAL other_start, other_end;/* time of groups on below staff */
	int size = GS_NORMAL;
	struct STAFF *staff_p;
	int staffno;
	int vno;


	/* only the first gs_p is guaranteed to have the right vno at this
	 * point, so save that. */
	vno = gs_p->vno;
	staff_p = mll_p->u.staff_p;
	staffno = staff_p->staffno;

	/* find where in the measure the beam begins, by adding up the
	 * time values of all the groups prior to the first beamed group */
	for (start_time = Zero, g_p = gs_p->prev; g_p != (struct GRPSYL *) 0;
					g_p = g_p->prev) {
		if (g_p->grpvalue == GV_ZERO) {
			/* Skip grace groups */
			continue;
		}
		start_time = radd(start_time, g_p->fulltime);
	}

	/* find how long the beam lasts. Also see if there are any small
	 * groups */
	for (end_time = start_time, g_p = gs_p; g_p != (struct GRPSYL *) 0;
					g_p = g_p->next) {
		/* accumulate the time */
		end_time = radd(end_time, g_p->fulltime);

		/* check for small groups */
		if (g_p->grpcont == GC_NOTES && g_p->grpsize == GS_SMALL
						&& g_p->grpvalue != GV_ZERO) {
			size= GS_SMALL;
		}

		/* end of the beam? */
		if (g_p->beamloc == ENDITEM && g_p->grpvalue != GV_ZERO) {
			break;
		}
	}
	if (g_p == (struct GRPSYL *) 0) {
		/* maybe this should be silent, since another error message
		 * should already be printed, but this will point out that
		 * the problem was on a cross-staff beam */
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
					"can't find end of cross-staff beam");
		return(-1);
	}

	/* Make sure this is the bottom voice of the above staff.
	 * If it's voice 2 (subscript 1) then it is for sure.
	 * Otherwise, have to make sure the second and third voices, if any,
	 * have all spaces for the duration of the cross-staff beam */
	if (vno != 1) {
		/* If voice 2 is visible and not all spaces,
		 * or if we are working on voice 1 while voice 3 is
		 * visible and not all space, there is a problem. */
		if ((vvpath(staffno, 2, VISIBLE)->visible == YES &&
				hasspace(staff_p->groups_p[1], start_time,
				end_time) == NO)
				|| (vno == 0 &&
				vvpath(staffno, 3, VISIBLE)->visible == YES &&
				hasspace(mll_p->u.staff_p->groups_p[2],
				start_time, end_time) == NO)) {
			l_yyerror(gs_p->inputfile, gs_p->inputlineno,
				"cross-staff beam must be from bottom voice of staff %d",
				mll_p->u.staff_p->staffno);
			return(-1);
		}
	}

	/* Find the associated voice, and the associated bm group
	 * in that voice. First find the next visible staff */
	for (assoc_mll_p = mll_p->next;   ; assoc_mll_p = assoc_mll_p->next) {
		if (assoc_mll_p == (struct MAINLL *) 0 ||
						assoc_mll_p->str != S_STAFF) {
			l_yyerror(gs_p->inputfile, gs_p->inputlineno,
					"no visible staff below to beam with");
			return(-1);
		}

		if (svpath(assoc_mll_p->u.staff_p->staffno, VISIBLE)->visible
							== YES) {
			/* found the right staff */
			break;
		}
	}

	/* Associated voice is probably voice 1 of the below staff.
	 * But there is a slight possibility it is voice 2, or even voice 3.
	 * Skip over voices that are all spaces for the duration of the beam.
	 * Since voice 3 is the "middle" voice, we check 1, then 3, then 2.
	 */
	if (vvpath(staffno, 1, VISIBLE)->visible == YES &&
			hasspace(assoc_mll_p->u.staff_p->groups_p[0],
			start_time, end_time) == NO) {
		assoc_grps_p = assoc_mll_p->u.staff_p->groups_p[0];
	}
	else if (vvpath(staffno, 3, VISIBLE)->visible == YES &&
			hasspace(assoc_mll_p->u.staff_p->groups_p[2],
			start_time, end_time) == NO) {
		assoc_grps_p = assoc_mll_p->u.staff_p->groups_p[2];
	}
	else if (vvpath(staffno, 2, VISIBLE)->visible == YES &&
			hasspace(assoc_mll_p->u.staff_p->groups_p[1],
			start_time, end_time) == NO) {
		assoc_grps_p = assoc_mll_p->u.staff_p->groups_p[1];
	}
	else {
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
				"cross-staff beam has no notes on staff %d",
				assoc_mll_p->u.staff_p->staffno);
		return(-1);
	}

	/* Tab staffs can't be involved in cross-staff beaming */
	if (is_tab_staff(mll_p->u.staff_p->staffno) ||
			is_tab_staff(assoc_mll_p->u.staff_p->staffno)) {
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
				"cross-staff beaming not allowed on tab staff");
		return(-1);
	}

	/* We don't allow the different staffs to have different staffscale
	 * values: it doesn't really make much sense to allow it, and avoids
	 * all the issues like how wide to make the beams.
	 */
	if (svpath(mll_p->u.staff_p->staffno, STAFFSCALE)->staffscale !=
			svpath(assoc_mll_p->u.staff_p->staffno,
			STAFFSCALE)->staffscale) {
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
			"staffs involved with cross-staff beams must have identical staffscale values");
		/* We did find which to associate with, even though
		 * its staffscale was wrong. */
		return(assoc_mll_p->u.staff_p->staffno);
	}

	/* find the group that ought to be the "bm with staff above" group, by
	 * going that far time-wise into the measure on the associated voice */
	for (other_start = Zero, other_p = assoc_grps_p;
				other_p != (struct GRPSYL *) 0;
				other_p = other_p->next) {

		if (GT(other_start, start_time)) {
			/* too far. pretend to be at end of list so we
			 * and fall out of loop to print the error message
			 * for this case */
			other_p = (struct GRPSYL *) 0;
			break;
		}

		if (EQ(other_start, start_time)) {
			/* found it! */
			break;
		}

		if (other_p->grpvalue == GV_ZERO) {
			continue;
		}

		/* have to keep going. Keep track of how far we are in time */
		other_start = radd(other_start, other_p->fulltime);
	}

	/* skip past any grace groups */
	while (other_p != 0 && other_p->grpvalue == GV_ZERO) {
		other_p = other_p->next;
	}

	/* If we didn't find a voice below, or that voice's group
	 * isn't the start of a beam with above, there is a problem.
	 * In the second case, maybe user really meant to beam with some
	 * lower voice, but that would collide, which we don't allow.
	 */
	if (other_p == (struct GRPSYL *) 0 || other_p->beamloc != STARTITEM
					|| other_p->beamto != CS_ABOVE) {
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
			"'bm with staff below' has no matching 'bm with staff above' (may be missing, invisible, or on wrong voice)");
		return(assoc_mll_p->u.staff_p->staffno);
	}

	/* go through the two voices. For each note group, verify that
	 * the other voice has space during that time period. Do the "other"
	 * staff first, because in a previous version of this function it
	 * had to be done in that order to avoid possible null pointer
	 * dereference. Now things have changed, so that doesn't matter
	 * any more, but I don't want to change the order, to make sure I
	 * don't break something.
	 */
	other_p = verify_crossbeam(other_p,
			mll_p->u.staff_p->groups_p[vno], start_time,
			&other_end, assoc_mll_p->u.staff_p->staffno, size);
	gs_p = verify_crossbeam(gs_p, assoc_grps_p, start_time, &end_time,
			mll_p->u.staff_p->staffno, size);

	/* we should be pointing to the ebm group for each staff,
	 * unless of course, something went wrong, like user didn't
	 * specify an ebm */
	if (gs_p == (struct GRPSYL *) 0 || other_p == (struct GRPSYL *) 0) {
		/* maybe this should be silent, since another error message
		 * should already be printed, but this will point out that
		 * the problem was on a cross-staff beam */
		l_yyerror(assoc_grps_p->inputfile, assoc_grps_p->inputlineno,
			"failed to find ebm for cross-staff beam");
		return(assoc_mll_p->u.staff_p->staffno);
	}

	if (NE(end_time, other_end)) {
		l_yyerror(gs_p->inputfile, gs_p->inputlineno,
			"ebm not at same time in measure for both voices of cross-staff beam");
	}

	/* Disallow illegal combinations of slope and stem length */
	slopelencheck(gs_p, other_p, "beam");

	return(assoc_mll_p->u.staff_p->staffno);
}


/* Given the first group of a cross-staff beam, and the beginning of the
 * list of GRPSYLs in the associated voice (the voice beamed to), and the
 * time into the measure where the beam starts, check each group. Verify
 * that each GC_NOTES group has GC_SPACE in the other voice and vice-versa.
 * Also check that all note groups are the same size, and mark the spaces
 * as the correct size so that everything in the beam has the same size.
 * Return a pointer to the last group in the beam (null if something goes
 * wrong). Also, return the time into the measure of the end of the beam,
 * via the end_time_p pointer.
 */

static struct GRPSYL *
verify_crossbeam(gs_p, other_gs_p, start_time, end_time_p, staffno, size)

struct GRPSYL *gs_p;	/* first group in list to be checked */
struct GRPSYL *other_gs_p; /* the groups_p of the associated voice */
RATIONAL start_time;	/* when the beam begins */
RATIONAL *end_time_p;	/* time through end of beam will be returned here */
int staffno;
int size;		/* GS_NORMAL or GS_SMALL */

{
	RATIONAL end_time;
	int has_at_least_1_note_group = NO;


	/* go through each group in the beam */
	for (  ; gs_p != (struct GRPSYL *) 0; gs_p = gs_p->next) {

		/* skip over any grace groups */
		if (gs_p->grpvalue == GV_ZERO) {
			continue;
		}

		/* find the end time of the group, for passing to hasspace() */
		end_time = radd(start_time, gs_p->fulltime);

		/* if notes, other voice must have space */
		if (gs_p->grpcont == GC_NOTES) {
			if (hasspace(other_gs_p, start_time, end_time) == NO) {
				l_yyerror(gs_p->inputfile, gs_p->inputlineno,
					"cross-staff beam must always have notes in one voice and space in the other voice");
				return (struct GRPSYL *) 0;
			}
			has_at_least_1_note_group = YES;
			if (gs_p->grpsize != size) {
				l_yyerror(gs_p->inputfile, gs_p->inputlineno,
				"can't mix normal and cue size chords in cross-staff beam");
			}
		}

		/* conversely, if space, other voice must not have space */
		else if (gs_p->grpcont == GC_SPACE) {
			struct GRPSYL *g_p;
			RATIONAL t;
			int oldcont = GC_SPACE;

			/* This is somewhat like hasspace() except that checks
			 * that the entire duration is space. Here we need
			 * to check if there is space at least somewhere during
			 * the time period. If so, user error.
			 */
			for (g_p = other_gs_p, t = Zero; LT(t, start_time);
							g_p = g_p->next) {
				if (g_p->grpvalue == GV_ZERO) {
					continue;
				}
				t = radd(t, g_p->fulltime);
				oldcont = g_p->grpcont;
			}
			if (GT(t, start_time) && oldcont == GC_SPACE) {
				/* group spilling into this time is space */
				l_yyerror(gs_p->inputfile, gs_p->inputlineno,
					"cross-staff beam must always have notes in one voice and space in the other voice");
				return (struct GRPSYL *) 0;
			}
			for (   ; g_p != 0 && LT(t, end_time); g_p = g_p->next) {
				if (g_p->grpvalue == GV_ZERO) {
					continue;
				}
				if (g_p->grpcont == GC_SPACE) {
					l_yyerror(gs_p->inputfile, gs_p->inputlineno,
						"cross-staff beam must always have notes in one voice and space in the other voice");
					return (struct GRPSYL *) 0;
				}
				t = radd(t, g_p->fulltime);
			}

			/* mark size of spaces. Normally space can't be cue				 * size, but in this case, it makes it easier for later
			 * code (in print phrase at least) if everything in the
			 * beam--even spaces--is marked as cue size */
			gs_p->grpsize = size;
		}

		/* esbm is not currently allowed on cross-staff beams.
		 * It would much more complicated than normal beams,
		 * because the "primary" beam might perhaps best be the top,
		 * the bottom, or the middle, depending on where the notes are.
		 * Placement and print phase would have to know about that,
		 * so that stems could be adjusted properly,
		 * and beams drawn in the right places.
		 */
		if (gs_p->breakbeam == YES) {
			l_warning(gs_p->inputfile, gs_p->inputlineno,
					"esbm is not supported on cross-staff beams; being ignored");
			gs_p->breakbeam = NO;
		}

		/* see if we reached the end of the beam */
		if (gs_p->beamloc == ENDITEM) {
			*end_time_p = end_time;
			if (has_at_least_1_note_group == NO) {
				l_yyerror(gs_p->inputfile, gs_p->inputlineno,
					"cross-staff beam has no notes on staff %d",
					staffno);
			}

			return(gs_p);
		}

		/* arrange for next time through the loop, by moving the
		 * start_time to the next group */
		start_time = end_time;
	}

	/* failed to find an ebm */
	return (struct GRPSYL *) 0;
}


/* User is not allowed to specify length on both ends of a beam along with
 * a slope, because they could be contradictory. */

static void
slopelencheck(first_p, last_p, bmtype)

struct GRPSYL *first_p;		/* first beamed group */
struct GRPSYL *last_p;		/* last beamed group */
char *bmtype;			/* "beam" or "alt" */

{
	if (IS_STEMLEN_KNOWN(first_p->stemlen) == YES &&
			IS_STEMLEN_KNOWN(last_p->stemlen) == YES &&
			fabs(first_p->beamslope - NOBEAMANGLE) > 0.001) {
		l_yyerror(last_p->inputfile, last_p->inputlineno,
			"can't specify both end stem lengths and slope for %s",
			bmtype);
	}
}
