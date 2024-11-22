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
/*
 *	globals.h
 *
 *	This file has externs for the global variables defined in global.c.
 *	It also has externs for all nonstatic functions.
 */

#ifndef _GLOBALS
#define _GLOBALS

#ifdef __WATCOMC__
#include <sys\types.h>
#endif
#include <string.h>
#include <ctype.h>
#include "structs.h"
#ifdef VMS
#include <unixio.h>
#endif

/*
 * Some non-ANSI compilers may not have these defined in stdio.h, so if they
 * aren't defined, define them now.
 */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

/* ====== externs for variables--see globals.c for comments about them ===== */

extern struct SSV Score;
extern struct SSV Staff[MAXSTAFFS];
extern struct SSV Voice[MAXSTAFFS][MAXVOICES];

extern short Staff_vis[MAXSTAFFS + 1];
extern short Voice_vis[MAXSTAFFS + 1][MAXVOICES + 1];

extern struct MAINLL *Mainllhc_p;
extern struct MAINLL *Mainlltc_p;

extern int Optch;
extern int Mupmate;
extern int Errorcount;
extern int Maxverses;
extern short Meas_num;
extern int Preproc;
extern int Ppcomments;

extern UINT32B Context;
extern int Curr_family;
extern int Curr_font;
extern int Curr_size;
extern struct USERFONT *Userfonts;
extern int Got_some_data;
extern int Doing_tab_staff;
extern int Doing_MIDI;
extern char *Curr_filename;
extern char *Outfilename;
extern int Vcombused;
extern int CSBused;
extern int CSSused;
extern int CSSpass;
extern int Keymap_used;
extern int Tuning_used;
extern int Mrptused;

extern int yylineno;
/* This 8192 must agree with YYLMAX in lex.c.  We shouldn't have to specify
 * the size here, but at least one compiler generates bad code if we don't. */
extern char yytext[8192];

extern int Lineno_increment;
extern unsigned char Resttab[];
extern int Letshift[];
extern char Circle[];
extern char Acclets[];
extern char *Acctostr[];
extern char Accidental_map[MAXMIDINOTES][MAX_ACCS * 2];
extern char Deferred_acc[MAXMIDINOTES][MAX_ACCS * 2];
extern short Tie_table[MAXMIDINOTES];
extern struct ACCIDENTALS *Acc_contexts_list_p;
extern char No_accs[MAX_ACCS * 2];
extern char Ped_start[];
extern char Ped_stop[];

extern struct STRINGINFO Guitar[DEFTABLINES];
extern int Octave_adjust[MAXSTAFFS+1];
extern int Octave_bars[MAXSTAFFS+1];
extern float Octave_count[MAXSTAFFS+1];

extern float _Page	[NUMCTYPE];
extern float _Win	[NUMCTYPE];
extern float _Cur	[NUMCTYPE];

extern struct PAPER_SIZES Paper_sizes[];

extern struct BLOCKHEAD Header,  Leftheader,  Rightheader;
extern struct BLOCKHEAD Footer,  Leftfooter,  Rightfooter;
extern struct BLOCKHEAD Header2, Leftheader2, Rightheader2;
extern struct BLOCKHEAD Footer2, Leftfooter2, Rightfooter2;
extern short Gotheadfoot;

extern struct PRINTDATA * PostScript_hooks[PU_MAX];

extern float *Score_location_p;
extern float Staffs_y[MAXSTAFFS + 1];

extern float _Score[NUMCTYPE];
extern float _Staff[MAXSTAFFS][NUMCTYPE];

extern struct RANGELIST *Staffrange_p;
extern struct RANGELIST *Vnorange_p;
extern struct RANGELIST *VCrange_p;

extern short Place;

extern short Using_staff;
extern short Using_voice;

extern short Ped_snapshot[MAXSTAFFS + 1];

extern struct MAINLL *Staffmap_p[MAXSTAFFS + 1];
extern struct MAINLL *List_of_staffs_p;

extern struct SVRANGELIST *Svrangelist_p;
extern struct GRPSYL *Curr_gs_list_p;
extern short Got_multirest;
extern short Got_group;

extern short Pagenum;
extern int Last_pagenum;
extern int Firstpageside;
extern int Curr_pageside;

extern struct ATEND_INFO Atend_info;

extern struct RECTAB *Rectab;
extern int Reclim;

extern int Ignore_staffscale;

extern float Staffscale;
extern float Stdpad;
extern float Stepsize;
extern float Flagsep;
extern float Smflagsep;
extern float Tupheight;

extern RATIONAL Zero;
extern RATIONAL One_fourth;
extern RATIONAL One_third;
extern RATIONAL One_half;
extern RATIONAL Two_thirds;
extern RATIONAL Three_fourths;
extern RATIONAL One;
extern RATIONAL Two;
extern RATIONAL Three;
extern RATIONAL Four;
extern RATIONAL Eight;

extern char *Alt_timesig_list;
extern char *Next_alt_timesig;
extern short Tsig_visibility;

extern RATIONAL Maxtime;

extern int Debuglevel;
extern int exprdebug;
extern float Debug_coords[2][NUMCTYPE];

/* =========== defines that depend on the above variables ============= */

/* size of the actual paper, scaled so that the right amount of music fits */
/* these versions do not take musicscale into account, only scale */
#define	PGHEIGHT	(Score.pageheight / Score.scale_factor)
#define	PGWIDTH		(Score.pagewidth  / Score.scale_factor)

/* must divide margin variables by scale to cancel out the scaling for them */
#define EFF_TOPMARGIN	(Score.topmargin   / Score.scale_factor)
#define EFF_BOTMARGIN	(Score.botmargin   / Score.scale_factor)
/* right & left margins use functions eff_rightmargin() & eff_leftmargin() */

/* 
 * The following macros are used when both scale and musicscale are to be
 * taken into account.
 *
 * "scale" affects the music window, and header/footer/top/bottom windows, but
 * not margins.  So to scale music, we pretend the page size and margins are
 * 1/scale as big, draw the other things normally, then apply "scale" to the
 * whole page.
 *
 * "musicscale" affects only the music window.  So, we pretend that the page,
 * margins, and headers etc. are 1/musicscale as big (printing headers etc. that
 * big), draw music normally, then apply "musicscale" to the whole page.
 *
 * "scale" and "musicscale" interact by multiplying them.  Also, note that
 * headers etc. don't affect music on the left and right, only top and bottom.
 */
#define	EFF_PG_HEIGHT	(Score.pageheight /	\
				(Score.scale_factor * Score.musicscale))
#define	EFF_PG_WIDTH	(Score.pagewidth  /	\
				(Score.scale_factor * Score.musicscale))
#define EFF_TOP_MARGIN	(Score.topmargin /	\
				(Score.scale_factor * Score.musicscale))
#define EFF_BOT_MARGIN	(Score.botmargin /	\
				(Score.scale_factor * Score.musicscale))
/*
 * Not worth having macros for effective header etc. height; just divide the
 * normal values by Score.musicscale.
 */

/* =========== externs for global variables in generated code ============= */

extern int yydebug;	/* yacc debug flag */
extern FILE *yyin;	/* where lex will read from */

/* =========== externs for functions ============= */

#ifdef __STDC__
#include <stdlib.h>
#include <unistd.h>
#else
/* UNIX utilities that don't appear in official header files in SV_R3 */
extern char *malloc();
extern char *calloc();
extern char *realloc();
extern void free();

extern void exit();
extern int abs();
extern int atoi();
extern double atof();
extern void qsort();
extern long strtol();
#endif

/* abshorz.c */
extern void abshorz P((void));

/* absvert.c */
extern void absvert P((void));

/* assign.c */
extern void assign_int P((int var, int value, struct MAINLL *mainll_item_p));
extern void assign_float P((int var, double value,
		struct MAINLL *mainll_item_p));
extern void assign_vscheme P((int numvoices, int vtype,
		struct MAINLL *mainll_item_p));
extern void assign_key P((int num, int acc, int minor,
		struct MAINLL *mainll_item_p));
extern void assign_string P((int var, char *string,
		struct MAINLL *mainll_item_p));
extern void assign_unit P((int unittype, struct MAINLL *mainll_p));
extern char *copy_string P((char *string, int font, int size));
extern void assign_timesig P(( struct MAINLL *mainll_item_p, int invisible,
		char **next_alternation_p));
extern void assign_font P((int var, int value, struct MAINLL *mainll_item_p));
extern void new_staffset P((void));
extern void add_staffset P((int start, int end, char *label1, char *label2));
extern void set_staffset P((int var, struct MAINLL *mainll_item_p));
extern void new_barstlist P((void));
extern void add_barst P((int start, int end, int between, int all));
extern void set_barstlist P((struct MAINLL *mainll_item_p,
		struct SUBBAR_APPEARANCE *sb_app_p));
extern void new_beamlist P((void));
extern void add_beamlist P((RATIONAL value));
extern void set_beamlist P((struct MAINLL *mainll_item_p));
extern void asgn_stafflines P((int numlines, int printclef,
		struct MAINLL *mainll_item_p));
extern void set_font P((int var, int value, struct MAINLL *mll_p));
extern void add_tab_string_info P((int pitch, int accidental, int nticks,
		int octave, struct SSV *ssv_p));
extern void set_mnum P((struct BAR *bar_p, int mnum));
extern void assign_2floats P((int var, double value1, double value2,
		struct MAINLL *mainll_item_p));
extern void chkmargin P((double topmargin, double botmargin, double leftmargin,
		double rightmargin));
extern double adjust2inches P((double value));
extern void begin_subbeam P((void));
extern void end_subbeam P((void));
extern void assign_vcombine P((int qualifier, int bymeas,
		struct MAINLL *mainll_p, struct TIMEDSSV *tssv_p));
extern int has_tab_staff P((void));
extern void chk_tab P((struct MAINLL *mll_p));
extern void set_keymap P((int which, char *name, struct MAINLL *mll_p));
extern void assign_firstpage P((int pagenum, int firstside,
		struct MAINLL *mainll_item_p));
extern void set_margin P((int var, double leftval, double rightval,
		struct MAINLL *mainll_item_p));
extern void assign_direction P((int param, int value, struct MAINLL *mainll_p));
extern void check_beamstyle P((struct SSV *ssv_p ));

/* beaming.c */
extern void setbeamloc P((struct GRPSYL *curr_grp_p,
		struct GRPSYL *last_grp_p));
extern int needs_auto_beaming P((struct GRPSYL *grpsyl_p));
extern void do_beaming P((struct GRPSYL *grpsyl_p, int grpsize, int staffno,
		int vno));
extern void set_alt_beams P((struct GRPSYL *grpsyl_p));
extern int chk_crossbeam P((struct GRPSYL *gs_p, struct MAINLL *mll_p));

/* beamstem.c */
extern void beamstem P((void));
extern void embedrest P((struct GRPSYL *first_p, struct GRPSYL *start1_p,
		struct GRPSYL *start2_p, double b1, double b0));

/* brac.c */
extern int brac_check P((struct STAFFSET *bracelist_p, int nbrace,
		 struct STAFFSET *bracklist_p, int nbrack));
extern int pr_brac P((int is_restart, double x_offset, int has_labels));
extern double width_left_of_score P((struct MAINLL *mll_p));
extern double pwidth_left_of_score P((struct MAINLL *mll_p,
		struct MAINLL *prev_feed_mll_p));
extern double prep_brac P((int is_restart, double x_offset,
		struct MAINLL *mll_p, int *has_labels_p));

/* charinfo.c */
extern double height P((int font, int size, int ch));
extern double width P((int font, int size, int ch));
extern double ascent P((int font, int size, int ch));
extern double descent P((int font, int size, int ch));
extern char *fix_string P((char *string, int font, int size, char *fname,
		int lineno));
extern int str2mfont P((int str));
extern int mfont2str P((int mfont));
extern int is_music_symbol P((char *str));
extern double strascent P((char *str));
extern double strdescent P((char *str));
extern double strheight P((char *str));
extern double strwidth P((char *str));
extern char *tranchstr P((char *chordstring, int staffno));
extern int restchar P((struct GRPSYL *grp_p, int *font_p));
extern char *dashstr P((char *str));
extern void end_fontsize P((char *str, int *font_p, int *size_p));
extern char *ascii_str P((char *str, int verbose, int pagenum, int textmod));
extern char *split_string P((char *string, double desired_width));
extern int adj_size P((int size, double factor, char *filename, int lineno));
extern char *resize_string P((char *string, double factor, char *filename,
		int lineno));
extern double left_width P((char *string));
extern double center_left_width P((char *string));
extern char *acc_trans P((char *string));
extern char *modify_chstr P((char *string, int modifier));
extern int is_bold_font P((int font));
extern int is_ital_font P((int font));
extern double circled_dimensions P((char *str, float* height_p, float *width_p,
		float *ascent_adjust, float *x_offset_p));
extern char *get_reh_string P((struct BAR *bar_p, int staffnum));
extern struct USER_SYMBOL *alloc_usym P((char *symname));
extern void define_usym P((void));
extern int valid_charname P((char *name));
extern void init_charinfo_table P((void));
extern char *get_charname P((int code, int font));
extern unsigned char find_char P((char * name, int *font_p, int *is_small_p,
		int errmsg));
extern char *fix_tagged_string P((char *string, int font, int size,
		char *fname, int lineno));
extern int is_bad_char P((int ch));
extern char *fix_pat_rep_string P((char *string, int font, int size,
		char *fname, int lineno, char *str_type));
extern void bs_init P((int len));
extern void bs_push P((int code, int font));
extern int bs_pop P((char *fname, int linenum, int *error_p));
extern double endspace_width P((char *str));
extern int has_align_point P((char *str));

/* check.c */
extern int rangecheck P((int n, int min, int max, char *name));
extern int frangecheck P((double n, double min, double max, char *name));
extern int power_of2check P((int n, char *name));
extern int contextcheck P((UINT32B validcontext, char *action));
extern char *contextname P((UINT32B cont));
extern void check_at_least1visible P((void));
extern void chk_vis_feed P((void));
extern void chk_interval P((int inttype, int intnum));
extern void used_check P((struct MAINLL *mll_p, int var, char *name));
extern int l_rangecheck P((int num, int min, int max, char *name, char *fname,
		int lineno));
extern void combine_rests P((int c));
extern char *markname P((int mark));
extern void chk_order P((struct SSV *ssv_p, int place));
extern int erangecheck P((int n, int min, int max, int empty_value,
		char *name));
extern void chk_x_arg P((char *x_arg, int *start_p, int *end_p));
extern void extract P((int start, int end));
extern void set_all_default_acc_offsets P((void));
extern int set_firstpageside P((int p_option_side));
extern void chk4matching_repeatends P((void));
extern void expand_repeats P((void));
extern void set_mrpt_info P((void));
extern int mrptchar P((struct GRPSYL *grp_p, int *font_p));
extern void set_meas_num P((int value, char *filename, int lineno));

/* debug.c */
extern char *stype_name P((int stype));
extern void print_mainll P((void));

/* errors.c */
extern void ufatal P((char *format, ...));
extern void pfatal P((char *format, ...));
extern void l_ufatal P((char *filename, int lineno, char *format, ...));
extern void l_pfatal P((char *filename, int lineno, char *format, ...));
extern void l_no_mem P((char *filename, int lineno));
extern void cant_open P((char *filename));
extern void warning P((char *format, ...));
extern void l_yyerror P((char *fname, int lineno, char *format, ...));
extern void l_warning P((char *filename, int lineno, char *format, ...));
extern void debug P((int level, char *format, ...));
extern int debug_on P((int level));
extern void doraterr P((int code));
extern void error_exit P((void));
extern void print_offending_line P((char *filename, int lineno));

/* font.c */
extern double fontascent P((int font, int size));
extern double fontdescent P((int font, int size));
extern double fontheight P((int font, int size));
extern void parse_font_file P((char *filename));
extern int lookup_font P((char *fontname));
extern int font_index P((int font));
extern char *fontnum2name P((int font));

/* fontdata.c */
extern struct FONTINFO Fontinfo[MAXFONTS];
extern void init_psfont_metrics P((void));

/* gram.y */
extern int yyparse P((void));
extern int yyerror P((char *msg));
extern void check_same_ended P((void));

/* grpsyl.c */
extern struct GRPSYL *newGRPSYL P((int grp_or_syl));
extern void copy_attributes P((struct GRPSYL *newgrp_p,
		struct GRPSYL *oldgrp_p));
extern struct TIMELIST *copy_timeunit P((struct GRPSYL *newgrp_p,
		struct GRPSYL *oldgrp_p, struct TIMELIST *timelist_p));
extern void copy_notes P((struct GRPSYL *newgrp_p, struct GRPSYL *oldgrp_p));
extern void link_notegroup P((struct GRPSYL *newgrp_p,
		struct GRPSYL *last_grp_p));
extern void add_note P((struct GRPSYL *grpsyl_p, int pitch, char *acclist,
		int octave, int nticks, int has_paren, char *bend_string));
#ifdef __STDC__
extern int notecomp P((const void *item1_p, const void *item2_p));
#else
extern int notecomp P((char *item1_p, char *item2_p));
#endif
extern int reconstruct_basictime P((RATIONAL fulltime));
extern int recalc_dots P((RATIONAL fulltime, int basictime));
extern void clone_withlist P((struct GRPSYL *newgrp_p,
		struct GRPSYL *oldgrp_p));
extern char *stringname P((int stringno, int staffno));
extern char *format_string_name P((int letter, int accidental,
		int nticks));
extern void do_bar P((int bartype));
extern void begin_tuplet P((void));
extern void end_tuplet P((int tupcont, RATIONAL tuptime, int printtup,
		int tupside, double tupslop));
extern void check4barline_at_end P((void));
extern void add_multirest P((int nummeas));
extern struct GRPSYL *clone_gs_list P((struct GRPSYL *list_p,
		int copy_noteinfo));
extern void add_slurto P((struct GRPSYL *grpsyl_p, int pitch, int octave,
		int note_index, int slurstyle));
extern void free_grpsyls P((struct GRPSYL *gs_p));
extern void resize_notelist P((struct GRPSYL *gs_p));
extern void add_meas_space P((struct GRPSYL **gs_p_p, int staff, int voice));
extern int is_mrpt P((struct GRPSYL *gs_p));
extern void report_mix_error P((void));
extern void free_notelist P((struct GRPSYL *gs_p));
extern void begin_slur P((struct GRPSYL *grpsyl_p, int note_index));
extern void set_slurdir P((struct GRPSYL *grpsyl_p, int note_index, int dir,
		int slurred_to_voice));
extern int has_pickup P((void));
extern struct GRPSYL *expandgrp P((struct GRPSYL *grpsyl_p,
		struct TIMELIST *timelist_p));
extern RATIONAL calcfulltime P((RATIONAL basictime, int dots));
extern struct MAINLL *add_bar P((int bartype, int linetype, int endingloc,
		int endending_type, struct MAINLL *insert_p));
extern int proc_emptymeas P((int startstaff));
extern int is_internal_token P((char *token));
extern void emptym_err P((char *severity));

/* keymap.c */
extern struct KEYMAP *get_keymap P((char *name));
extern void map_all_strings P((void));
extern void new_keymap P((char * name));
extern void add_to_keymap P((char *pattern, char *replacement));
extern int keymap_handle P((char *name));
extern char *keymap_name P((int map));
extern char *map_print_str P((char *str, char *fname, int linenum));

/* lex.l */
extern void chk_ifdefs P((void));
extern int save_macro P((FILE *file));
extern int yylex P((void));
extern void get_parameters P((char *macname));
extern int get_mac_arguments P((char *macname, int num_args));
extern void pushback P((int c));
extern void begin_raw P(());
extern void end_raw P(());
extern void new_lexbuff P((FILE *file));
extern void del_lexbuff P((void));
extern void new_lexstrbuff P((char *buff, int len));
extern void set_lex_mode P((int doing_expression));

/* locvar.c */
extern void fix_locvars P((void));
extern void eval_coord P((struct INPCOORD *inpcoord_p, char *inputfile,
		int inputlineno));

/* lyrics.c */
extern void lyr_verse P((int begin, int end));
extern void proc_lyrics P((struct GRPSYL *grpsyl_p, char *lyrstring));
extern void attach_lyrics2staffs P((struct MAINLL *mll_staffs_p));
extern void sylwidth P((char *lyrstring, float *wid_b4_syl_p,
		float *wid_real_syl_p, float *wid_after_syl_p));
extern void set_maxverses P((void));
extern struct SSV *get_lyr_dflt_timeunit_ssv P((void));
extern void setlyrfont P((int staffno, int font));
extern void setlyrsize P((int staffno, int size));
extern void lyr_compensate P((struct GRPSYL *gs_p));
extern struct GRPSYL *derive_lyrtime P((void));
extern void lyr_new_bar P((void));
extern void not_derived P((void));

/* macros.c */
extern int not_in_mac P((int inc_dec));
extern void mac_error P((void));
extern void includefile P((char *fname));
extern int popfile P((void));
extern void cmdline_macro P((char *macdef));
extern void define_macro P((char *macname, int is_expression));
extern void undef_macro P((char *macname));
extern void call_macro P((char *macname));
extern int is_defined P((char *macname, int paramtoo));
#ifndef unix
extern void mac_cleanup P((void));
#endif
extern void add_parameter P((char *macname, char *param_name));
extern void set_parm_value P((char *macname, char *argbuff, int argnum));
extern char *add2argbuff P((char *argbuff, int c));
extern FILE *find_file P((char **filename_p));
extern void preproc P((void));
extern void mac_saveto P((char *name));
extern void mac_restorefrom P((char *name));
extern void macro_concat P((char *concat_name));

/* main.c */
extern int onpagelist P((int pagenum));
extern int yywrap P((void));
extern int last_page P((void));

/* mainlist.c */
extern struct MAINLL *newMAINLLstruct P((int structtype, int lineno));
extern void insertMAINLL P((struct MAINLL *info_p, struct MAINLL *where));
extern void unlinkMAINLL P((struct MAINLL *which_p));

/* map.c */
extern void begin_map P((void));
extern void end_map P((void));
extern void map_item P((void));
extern void save_map P((void));
extern struct SSV *get_dflt_timeunit_ssv P((void));
extern void reset_input_style P((void));
extern int input_style P((int staff, int voice));
extern void merge_dup_notes P((struct GRPSYL *gs_p, int n));
extern void link_groups P((void));
extern int is_tab_range P((void));
extern int timelists_equal P((struct TIMELIST *tlist1_p,
		struct TIMELIST *tlist2_p));

/* mkchords.c */
extern void makechords P((void));

/* midi.c */
extern void gen_midi P((char *midifilename));
extern UINT32B write_delta P((int mfile));
extern int voice_used P((int staffno, int vno));
extern void insert_midistufflist P((struct STUFF *stuff_p));

/* midigrad.c */
extern void nix_til P((struct STUFF *stuff_p, char *miditype));
extern void process_to_list P((struct STUFF *stuff_p, char * miditype,
		int usec_per_quarter, int minval, int maxval));
extern void do_pending_gradual_midi P((void));

/* miditune.c */
extern void gen_tuning_maps P((int track, short Track2staff_map[],
		short Track2voice_map[]));
extern int find_note_number P((int staff, struct NOTE *note_p,
		int raw_notenum));
extern int notemap_size P((int map_index));
extern int notefreq P((int map_index, int slot_index, float *freq_p));
extern int num_notemaps P((void));
extern void set_map_index P((int idx));

/* midiutil.c */
extern void fix_track_size P((int mfile, long track_start, long track_size));
extern int parse_octave P((char *string, int place, char *fname, int lineno));
extern int clocks P((int num));
extern int getkeyword P((char *string, char **key_p, int *leng_p,
		char **arg_p_p));
extern int matches P((char *key, int leng, char *cmd));
extern int hexdig P((int ch));
extern UINT32B midi_wrstring P((int mfile, char *str, int internalform));
extern UINT32B wr_varlength P((int mfile, UINT32B num));
extern UINT32B midi_keysig P((int mfile, int sharps, int is_minor));
extern UINT32B midi_timesig P((int mfile));
extern void add_rest P((struct GRPSYL *gs_p, RATIONAL fulltime));
extern struct GRPSYL *grp_before P((struct GRPSYL *gs_p, struct MAINLL *mll_p,
                int staffno, int v));
extern void midi_squeeze P((void));
extern void guitar_transpose P((void));
extern void other_voice_accidentals P((struct STAFF *staff_p));
extern void mv_midi_items P((struct MAINLL *mll_p,
		struct MAINLL *topstaff_mll_p));
extern int get_param P((char *arg, char *inputfile, int inputlineno,
		int *parmnum_p, int *parmval_p));
extern int get_raw_notenum P((struct NOTE  *note_p, char *fname, int lineno,
		int staff));
extern void init_accidental_map P((int staffno));
extern void init_tie_table P((void));
extern void mark_accidental P((int pitch_offset, int acc));
extern int staff_audible P((int staff));
extern int out_notemap P((int mfile, int map_number));
extern int accs_offset P((char *acclist));
extern void midiwrite P((int fd, unsigned char * data, int length));
extern void reset_sigs P((void));

/* from musfont.c */
extern void init_musfont_metrics P((void));

/* nxtstrch.c */
extern double backsp_width P((int size));
extern int next_str_char P((char **str_p, int *font_p, int *size_p));
extern int nxt_str_char P((char **str_p, int *font_p, int *size_p,
		int *textfont_p, double *vertical_p, double *horizontal_p,
		int *in_pile_p, int slash));
extern double align_distance P((char *string, int font, int size));
extern double pile_width P((void));
extern int pile_size P((int size, int in_pile));

/* parstssv.c */
extern struct TIMEDSSV *tssv_create P((UINT32B context));
extern void tssv_update P((struct TIMEDSSV *timedssv_p, int param, int value));
extern void tssv_setgrpsyl P((struct GRPSYL *gs_p));
extern void tssv_line P((void));
extern struct TIMEDSSV *tssv_sort P((void));

/* parstuff.c */
extern void chk_stuff_header P((int size, int modifier, int place,
		int dist_usage));
extern void add_stuff_item P((double start_count, double start_steps,
		int start_gracebackup, char *string, int bars, double count,
		double end_steps, int end_gracebackup, double dist,
		int dist_usage, int aligntag, char *grid_label));
extern int string_is_sym P((char *string, int sym, int font));
extern void attach_stuff P((void));
extern void meas_stuff_chk P((void));
extern void chk4dangling_til_clauses P((char *boundary_desc));
extern struct STUFF *newSTUFF P((char *string, double dist, int dist_usage,
		int aligntag, double start_count, double start_steps,
		int start_gracebackup, int bars, double end_count,
		double end_steps, int end_gracebackup, int stuff_type,
		int modifier, int place, char *inputfile, int inputlineno));
extern void reset_ped_state P((void));
extern void set_reh_string P((struct BAR *bar_p, int fontfamily, int font,
		int size, char *string));
extern void set_stuff_type P((int stuff_type));
extern int dflt_place P((void));
extern void ped_endings P((int endingloc));
extern void multi_stuff P((int nmeas));
extern int get_stuff_type P((void));
extern char *stuff_modifier P((int modifier));
extern char *pad_string P((char *string, int modifier));
extern void connect_stuff P((struct STAFF *staff_p, struct STUFF *stufflist_p));
extern void init_reh P((int rehnumber, char *rehletter,
		struct MAINLL *mainbar_p));
extern void conv_ph_eph P((void));

/* phrase.c */
extern void phrase_points P((struct MAINLL *mll_p, struct STUFF *stuff_p));
extern void tieslur_points P((struct MAINLL *mll_p, struct STUFF *stuff_p));
extern void bend_points P((struct MAINLL *mll_p, struct STUFF *stuff_p));
extern void tabslur_points P((struct MAINLL *mll_p, struct STUFF *stuff_p));

/* plutils.c */
extern struct GRPSYL *nextnongrace P((struct GRPSYL *gs_p)); 
extern struct GRPSYL *prevnongrace P((struct GRPSYL *gs_p));
extern struct GRPSYL *nextglobnongrace P((struct GRPSYL *gs_p,
		struct MAINLL **mll_p_p)); 
extern struct GRPSYL *prevglobnongrace P((struct GRPSYL *gs_p,
		struct MAINLL **mll_p_p));
extern struct GRPSYL *nextnongracenonspace P((struct GRPSYL *gs_p)); 
extern int drmo P((int num));
extern double tieslurpad P((struct MAINLL *mll_p, struct STAFF *staff_p,
		struct GRPSYL *gs_p));
extern int hasspace P((struct GRPSYL *gs_p, RATIONAL vtime, RATIONAL vtime2));
extern int has_collapsible_space P((struct GRPSYL *gs_p, RATIONAL vtime,
		RATIONAL vtime2));
extern int has_space_pvno P((struct GRPSYL *gs_p, struct GRPSYL *gsv3_p,
		RATIONAL vtime, RATIONAL vtime2));
extern int has_with P((struct GRPSYL *gs_p, int place));
extern struct GRPSYL *closestgroup P((double count, struct GRPSYL *firstgs_p,
		int timeden));
extern int chkallspace P((struct MAINLL *msbeg_p, struct STUFF *stuff_p,
		int vno));
extern int allspace P((int vno, struct MAINLL *msbeg_p, RATIONAL begtime,
		struct MAINLL *msend_p, RATIONAL endtime));
extern struct MAINLL *getendstuff P((struct MAINLL *mainll_p,
		struct STUFF *stuff_p, int *timeden_p));
extern void accdimen P((int staffno, struct NOTE *note_p,
		float *ascent_p, float *descent_p, float *width_p));
extern int has_accs P((char *acclist));
extern int eq_accs(char *acclist1, char *acclist2);
extern int standard_acc P((char *acclist));
extern void standard_to_acclist P((int acc, char *acclist));
extern double acc_beam P((struct GRPSYL *gs_p, double b1, double b0,
		int ycoordtype, int numbeams));
extern double staffvertspace P((int s));
extern double halfstaffhi P((int s));
extern RATIONAL ratbend P((struct NOTE *note_p));
extern double notehorz P((struct GRPSYL *gs_p, struct NOTE *note_p, int coord));
extern int allsmall P((struct GRPSYL *gs1_p, struct GRPSYL *gs2_p));
extern int size_def2font P((int defsize));
extern void finalstemadjust P((struct GRPSYL *gs_p));
extern double getstemshift P((struct GRPSYL *gs_p));
extern int vscheme_voices P((int vscheme));
extern struct MAINLL *chmgrp2staffm P((struct MAINLL *mll_p,
		struct GRPSYL *gs_p));
extern void shiftgs P((struct GRPSYL *gs_p, double shift));
extern double nearestline P((double offset));
extern void vfyoffset P((struct GRPSYL *g_p[]));
extern double adjslope P((struct GRPSYL *g_p, double oldslope, int betweencsb,
		int param));
extern double eos_bar_adjust P((struct BAR *bar_p));
extern double curve_y_at_x P((struct CRVLIST *first_p, double x));
extern double findcubic P((struct CRVLIST *left_p, struct CRVLIST *right_p,
		float *a_p, float *b_p, float *c_p));
extern double solvecubic P((double a, double b, double c, double d,
		double lo, double hi, double thresh));
extern int css_affects_stemtip P((struct GRPSYL *gs1_p));
extern struct CHORD *gs2ch P((struct MAINLL *mll_p, struct GRPSYL *gs_p));
extern int css_affects_tieslurbend P((struct STUFF *stuff_p, struct MAINLL *mll_p));
extern int css_affects_phrase P((struct STUFF *stuff_p, struct MAINLL *mll_p));
extern struct GRPSYL *nextsamecont P((struct GRPSYL *gs_p));
extern struct GRPSYL *nextnonspace P((struct GRPSYL *gs_p));
extern struct GRPSYL *nextsimilar P((struct GRPSYL *gs_p));
extern struct GRPSYL *prevsimilar P((struct GRPSYL *gs_p));
extern double stemroom P((struct GRPSYL *gs_p));
extern double stemextsteps P((struct GRPSYL *gs_p));
extern struct GRPSYL *stafftime2firstgrp P((struct STAFF *staff_p,
		RATIONAL time));
extern void init_rectab P((void));
extern void inc_reclim P((void));
extern void free_rectab P((void));
extern int clef_vert_overlap P((int clef, struct GRPSYL *gs_p));
extern void allow_subbar P((struct CHORD *cp_p,
		struct SUBBAR_APPEARANCE *subbar_app_p, short subbar_ok[]));
extern char *pad_string P((char *string, int modifier));
extern char *string_func P((int num, char *transform));
extern int stem_x_position P((struct GRPSYL *gs_p));
extern double find_x_stem P((struct GRPSYL *gs_p));
extern double size2factor P((int size));
extern double slash_xlen P((struct GRPSYL *grpsyl_p));
extern double count2coord P((double count, struct BAR *bar_p,
		struct CHHEAD *chhead_p, int timeden));

/* print.c */
extern void print_music P((void));
extern void do_linetype P((int ltype));
extern void draw_line P((double x1, double y1, double x2, double y2));
extern void pr_muschar P((double x, double y, int ch, int size, int font));
extern double pr_clefsig P((struct MAINLL *mll_p, struct CLEFSIG *clefsig_p,
		int really_print));
extern void pr_string P((double x, double y, char *string, int justify,
		char *fname, int lineno));
extern void end_curve P((int ncoord, double x, double y));
extern void outcoord P((double val));
extern void draw_wavy P((double x1, double y1, double x2, double y2));
extern void do_pr_brac P((double x, double y1, double y2, int which));
extern void pr_linenum P((char *inputfile, int inputlineno));
extern void do_moveto P((double x, double y));
extern void do_line P((double x, double y));
extern void do_fill P((void));
extern void do_stroke P((void));
extern void do_curveto P((double x1, double y1, double x2, double y2,
		double x3, double y3));
extern void do_newpath P((void));
extern void do_closepath P((void));
extern void trailer P((void));
extern void do_closepath P((void));
extern void do_whitebox P((double x1, double y1, double x2, double y2));
extern void draw_prop_line P((double x1, double y1, double x2, double y2,
		int size, int ltype));
extern void do_grid P((double x, double y, double space, struct GRID *grid_p,
		int staff, double horzscale));
extern void newpage P((struct MAINLL *mll_p));
extern void pr_feed P((struct MAINLL *main_feed_p));
extern double pr_clef P((int staffno, double x, int really_print, int size));
extern void pr_scrunched_string P((double x, double y, char *string,
		int justify, double horzscale, char *fname, int lineno));
extern void do_rotate P((int angle));
extern void draw_parallelogram P((double x1, double y1, double x2, double y2,
		double halfwidth));
extern void print_blank_page P((void));

/* prntdata.c */
extern void pr_staff P((struct MAINLL *mll_p));
extern int tupdir P((struct GRPSYL *gs_p, struct STAFF *staff_p));
extern char *num2str P((int num));
extern char *mr_num P((struct MAINLL *mll_p, double *x_p, double *y_offset_p,
		double *height_p, double *width_p));
extern int tupgetsbrack P((struct GRPSYL *gs_p));
extern char *tupnumsize P((struct GRPSYL *gs_p, float *west_p,
		float *east_p, float *height_p, struct STAFF *staff_p));
extern int pbeamside P((struct GRPSYL *gs_p, struct GRPSYL *first_p));
extern struct GRPSYL *nextbmgrp P((struct GRPSYL *gs_p, struct GRPSYL *first_p,
		struct GRPSYL *endnext_p));
extern struct GRPSYL *prevbmgrp P((struct GRPSYL *gs_p,
		struct GRPSYL *first_p));
extern double end_bm_offset P((struct GRPSYL *top_first_p, struct GRPSYL *end_p,
		int basictime));
extern void pr_slashes P((struct GRPSYL *grpsyl_p, double x, double y,
		double sign, double offset, double y_tilt));
extern void pr_tab_groups P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
extern void pr_withlist P((struct GRPSYL *gs_p));
extern void pr_mrpt P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
extern void pr_multirest P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
extern double extwidth P((struct STUFF *stuff_p));
extern struct GRPSYL *next_bm_grp_w_rests P((struct GRPSYL *gs_p,
		struct GRPSYL *first_p, struct GRPSYL *endnext_p));
extern struct GRPSYL *prev_bm_grp_w_rests P((struct GRPSYL *gs_p,
		struct GRPSYL *first_p));

/* prntmisc.c */
extern void pr_endings P((struct MAINLL *main_feed_p));
extern int has_ending P((int staffno));
extern void pr_ped_char P((struct STUFF *stuff_p, int staffno));
extern void pr_ped_bar P((struct MAINLL *mll_p, struct BAR *bar_p));
extern void pr_phrase P((struct CRVLIST *crvlist_p, int linetype, int tapered,
		int staffno));
extern void pr_allcurve P((float x[], float y[], int num, double cwid,
		int tapered));
extern void saveped P((struct MAINLL *mll_p, struct BAR *bar_p));
extern double ped_offset P((void));
extern void pr_bend P((struct CRVLIST *crvlist_p));
extern void pr_tabslur P((struct CRVLIST *crvlist_p, int ts_style));
extern void pr_sm_bend P((double x, double y));
extern void pr_atend P((void));
extern void user_symbols P((int fontnumber, struct USERFONT *ufont_p));
extern void user_overrides P((int fontnumber, struct USERFONT *ufont_p));

/* prnttab.c */
extern char *fret_string P((struct NOTE *note_p, struct GRPSYL *gs_p));
extern char *bend_string P((struct NOTE *note_p));
extern void pr_tab_groups P((struct GRPSYL *gs_p, struct MAINLL *mll_p));
extern double pr_tabclef P((int staffno, double x, int really_print, int size));

/* prolog.c */
extern void ps_prolog P((void));

/* range.c */
extern void begin_range P((int place));
extern void save_staff_range P((int beginstaffno, int endstaffno));
extern void save_vno_range P((int begin, int end));
extern void free_rlists P((void));
extern void chk_range_type P((int has_ampersand));
extern void create_staffs P((void));
extern void all P((void));
extern void free_staffrange P((void));
extern void free_vnorange P((void));
extern int is_tab_staff P((int staffno));
extern int leadstaff P((int *place_p));
extern void add_to_sv_list P((void));
extern void free_sv_list P((struct SVRANGELIST *svlist_p));
extern void begin_sv_list P((void));
extern void save_vcombine_range P((int begin, int end));
extern void free_vcombine_range P((void));

/* relvert.c */
extern void relvert P((void));

/* restsyl.c */
extern void restsyl P((void));
extern double rightped P((int pedstyle, int pedchar));
extern double leftped P((int pedstyle, int pedchar));
double effwest P((struct MAINLL *mainll_p, struct CHORD *ch_p,
		struct GRPSYL *gs_p));
double effeast P((struct CHORD *ch_p, struct GRPSYL *gs_p));
extern struct GRPSYL *finalgroupproc P((struct GRPSYL *gs1_p,
                struct CHORD *pch_p));

/* roll.c */
extern void newROLLINFO P((void));
extern void setrolldir P((int dir));
extern void rollparam P((int topstaff, int topvoice, int botstaff,
		int botvoice));
extern void rolloffset P((double offset));
extern void do_rolls P((struct MAINLL *mll_p));
extern void print_roll P((struct GRPSYL *gs_p));
extern int gets_roll P((struct GRPSYL *gs_p, struct STAFF *staff_p, int v));

/* setgrps.c */
extern void setgrps P((void));
extern void applyaccstrs P((struct GRPSYL *g_p[], int numgrps));

/* setnotes.c */
extern void setnotes P((void));

/* from shapes.c */
extern int get_shape_override P((int staffno, int vno, int *font_p,
		int *code_p));
extern void init_new_shape_map P((char *name));
extern void add_shape_map_entry P((char *from_sym_name, char *to_sym_name));
extern void finish_shape_map P((void));
extern struct SHAPE_MAP *get_shape_map P((char *name));

/* ssv.c */
extern void initstructs P((void));
extern void zapssv P((struct SSV *s_p));
extern struct SSV *svpath P((int s, int field));
extern struct SSV *vvpath P((int s, int v, int field));
extern void asgnssv P((struct SSV *i_p));
extern void setssvstate P((struct MAINLL *mainll_p));
extern void savessvstate P((void));
extern void restoressvstate P((void));
extern struct MAINLL *restoreparms P((struct MAINLL *save_p,
		struct MAINLL *insert_p));
extern int staff_field_used P((int field, int staffno));
extern int voice_field_used P((int field, int staffno, int voiceno));

/* stuff.c */
extern void stuff P((void));

/* symtbl.c */
extern void init_symtbl P((void));
extern void addsym P((char *symname, float *coordlist_p, int coordtype));
extern float *symval P((char *symname, float **ref_p_p));
extern struct COORD_INFO *find_coord P((float *key));
extern void add_coord P((float *coordlist_p, int coordtype));
extern struct GRID *findgrid P((char *name));
extern void add_grid P((char *name, char *griddef));
extern struct GRID *nextgrid P((struct GRID *grid_p));
extern void set_win_coord P((float *coord_p));
extern void set_score_coord P((struct MAINLL *mll_p));
extern void add_shape P((char *name, char *shapes));
extern int nheadchar P((int headshape, int basictime, int stemdir,
		int *font_p));
extern int get_shape_num P((char *shapename));
extern double stem_yoff P((int headch, int font, int stemdir));
extern void remember_tsig_params P((struct MAINLL *mll_p));
extern void upd_ref P((float *old_p, float *new_p));
extern void rep_inpcoord P((struct INPCOORD *old_inpcoord_p,
		struct INPCOORD *new_inpcoord_p));
extern void add_user_head P((char *name, int fontnumber, int code,
		int upstem_yoffset, int downstem_yoffset));
extern int is_builtin_tag P((float *coord));
extern void save_tag_ref P((float *c_p, float **tag_ref_p_p));
extern void add_savemacs P((char *name, int index));
extern int find_savemacs P((char *name));
extern void add_saveparms P((char *name));
extern int do_restoreparms P((char *name));
extern void set_staff_x P((void));

/* tie.c */
extern void tie P((void));
extern void tie_carry P((void));
extern struct GRPSYL *find_1st_grp_in_nxt_measure P((struct MAINLL *mll_p,
		int vno));
extern struct GRPSYL *find_to_group P((struct MAINLL *mll_p,
		struct GRPSYL *gs_p, int to_voice, char *type));
extern struct GRPSYL *find_next_group P((struct MAINLL *mll_p,
		struct GRPSYL *gs_p, char *type));
extern struct NOTE *find_matching_note P((struct GRPSYL *gs_p,
		struct MAINLL *mll_p, int letter, int fretno, int octave,
		char *type_p));

/* trantab.c */
extern void tab2tabnote P((void));

/* trnspose.c */
extern void transgroups P((void));
extern char *tranchnote P((int letter, int acc, int staffno));
extern int eff_key P((int staff));
extern void totaltrans P((int do_notes, int s, int *type_p, int *num_p));
extern void apply_useaccs P((void));

/* undrscre.c */
extern int spread_extender P((struct GRPSYL *gs_p, struct MAINLL *mll_p,
		int verse, int sylplace, int really_print));
extern int has_extender P((char *syl));
extern void cont_extender P((struct MAINLL *mll_p, int sylplace,
		int verseno));
extern int last_char P((char *str));

/* utils.c */
extern void set_cur P((double x, double y));
extern void set_win P((double n, double s, double e, double w));
extern double width_barline P((struct BAR *bar_p));
extern double width_clefsig P((struct MAINLL *mainll_p,
		struct CLEFSIG *clefsig_p));
extern int clefchar P((int clef, int staffno, int *font_p));
extern void calc_headfoot_height P((void));
extern int numbeams P((int btime));
extern int acc2char P((int acc));
extern int char2acc P((int acc_code));
extern double inpc_x P((struct INPCOORD *inpcoord_p, char *fname, int lineno));
extern double inpc_y P((struct INPCOORD *inpcoord_p, char *fname, int lineno));
extern double find_y_stem P((struct GRPSYL *gs_p));
extern double find_x_stem P((struct GRPSYL *gs_p));
extern double width_keysig P((int sharps, int naturals));
extern struct GRPSYL *nextgrpsyl P((struct GRPSYL *gs_p,
		struct MAINLL **mll_p_p));
extern struct GRPSYL *prevgrpsyl P((struct GRPSYL *gs_p,
		struct MAINLL **mll_p_p));
extern void octave_transpose P((struct STAFF *staff_p, struct MAINLL *mll_p,
		int vno, int normdir));
extern int eff_acc P((struct GRPSYL *gs_p, struct NOTE *note_p,
		struct MAINLL *mll_p));
extern int acc_from_keysig P((int letter, int staffno, struct MAINLL *mll_p));
extern void set_staffscale P((int s));
extern void gridsize P((struct GRID *grid_p, int staff, float *north_ptr,
		float *south_ptr, float *east_ptr, float *west_ptr));
extern double gridspace P((int staff));
extern void gridinfo P((struct GRID *grid_p, int staff, int *frets_p,
		int *fretnum_p, int *numvert_p, int *topfret_p));
extern double clefspace P((int prevclef, double prevscale, int curclef,
		double curscale, int measnum));
extern double eff_rightmargin P((struct MAINLL *mainll_p));
extern double eff_leftmargin P((struct MAINLL *mainll_p));
extern short *findprimes P((int max));
extern short *factor P((int num));
extern double clefwidth P((int clef, int staffno, int is_small));
extern int clefvert P((int clef, int staffno, int is_small,
		float *north_p, float *south_p));
extern double widest_head P((struct GRPSYL *gs_p));
extern void calc_block_heights P((void));
extern void mnum_string P((char *dest_string, int measnum));
extern double withheight P((struct GRPSYL *gs_p, int place));
extern double mr_y_offset P((int staffno));
extern int index_type P((int index));
extern double cents2value P((double as_cents));
extern void set_default_acc_offsets P((struct ACCINFO *accinfo_p));
extern double beam_offset P((int nbeams, int gsize, int stemdir));
extern int page2side P((int phys_page_num));
extern int has_til P((struct STUFF *stuff_p));
extern double width_subbar P((struct SUBBAR_APPEARANCE *subbar_app_p));
extern int has_nonnormwith P((struct GRPSYL *gs_p));
extern int has_normwith P((struct GRPSYL *gs_p));
extern int references_page P((struct INPCOORD *coord_p));

#endif
