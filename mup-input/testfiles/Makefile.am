# These Mup input files are for testing error cases,
# so running Mup on them should exit non-zero.
XFAIL_TESTS = bad-input/chordinput_err.mup \
	bad-input/context_err.mup \
	bad-input/input01_err.mup \
	bad-input/input02_err.mup \
	bad-input/input03_err.mup \
	bad-input/input04_err.mup \
	bad-input/input05_err.mup \
	bad-input/input06_err.mup \
	bad-input/input07_err.mup \
	bad-input/input08_err.mup \
	bad-input/input09_err.mup \
	bad-input/input10_err.mup \
	bad-input/input11_err.mup \
	bad-input/macparm_err.mup \
	bad-input/mrpt_defoct_err.mup \
	bad-input/mrpt_midmeas_err.mup \
	bad-input/mrpt_mix1_err.mup \
	bad-input/mrpt_mix2_err.mup \
	bad-input/mrpt_multirest_err.mup \
	bad-input/mrpt_nonspace_err.mup \
	bad-input/mrpt_numstaffs1_err.mup \
	bad-input/mrpt_numstaffs2_err.mup \
	bad-input/mrpt_numstaffs3_err.mup \
	bad-input/mrpt_params_err.mup \
	bad-input/mrpt_repeat_err.mup \
	bad-input/mrpt_time1_err.mup \
	bad-input/mrpt_time2_err.mup \
	bad-input/mrpt_toofewa2_err.mup \
	bad-input/mrpt_toofewa4_err.mup \
	bad-input/mrpt_toofewb1_err.mup \
	bad-input/mrpt_toofewb2_err.mup \
	bad-input/mrpt_toofewb4_err.mup \
	bad-input/restart_err.mup \
	bad-input/restart_err.mup \
	bad-input/roll_err.mup \
	bad-input/roll_err.mup \
	bad-input/tab_err.mup \
	bad-input/tab_err.mup \
	bad-input/usersym_err.mup

# These tests fails are expected to pass
SUCCESS_TESTS = accparen.mup addtimes.mup alignped.mup allchars.mup assign.mup \
	beaming.mup beamstem.mup bulge.mup \
	cancelkey.mup cancelkey2.mup chordinput.mup chordtrans.mup \
	chordtranslation.mup \
	circledtext.mup clefsig.mup coord.mup \
	crossbeams.mup css.mup curves.mup \
	emptymeas.mup endings.mup extchar.mup fonts.mup \
	grace.mup groupalign.mup gtc.mup hasspace.mup \
	ifclause.mup interfere.mup keysig.mup \
	labels.mup latin1.mup ledger.mup lyrics.mup \
	macros.mup marks.mup measnum.mup miditest.mup \
	multcontext.mup multistuff.mup muschar.mup \
	mrpt_defoct.mup mrpt_numstaffs.mup  mrpt_params1.mup \
	mrpt_params2.mup mrpt_row.mup mrpt_time.mup \
	oneline.mup ontheline.mup optsemi.mup \
	packexp.mup paper_a4.mup paper_a5.mup paper_a6.mup paper_flsa.mup \
	paper_halfletter.mup paper_legal.mup \
	paragraph.mup pedal.mup piledtext.mup \
	rehearsal.mup relvert.mup restart.mup restart2.mup \
	restc.mup rests.mup roll.mup \
	setgrps.mup setnotes.mup shaped.mup sizes.mup \
	slashalt.mup split.mup stacking.mup staffscale.mup \
	subbar.mup subbeam.mup symoverride.mup \
	tabrepeat.mup tiecarry.mup tieslur.mup tiewarn.mup til.mup \
	timesig.mup transpose.mup trantab.mup tuplets.mup \
	underscore.mup unset.mup useaccs.mup usersyms.mup \
	vcombine.mup voice3.mup warnings.mup withadjust.mup
TESTS = $(SUCCESS_TESTS) $(XFAIL_TESTS) ../../tools/test/reggen2
EXTRA_DIST = $(SUCCESS_TESTS) $(XFAIL_TESTS) bad-input/midi_err.mup
# Run the mup program on all the files with .mup suffix
TEST_EXTENSIONS = .mup
MUP_LOG_COMPILER = ../../src/mup/mup
AM_MUP_LOG_FLAGS = -f /dev/null

# There are some midi-specific test cases in sub-directory
SUBDIRS = test-midi
