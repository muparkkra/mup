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
	circledtext.mup clefsig.mup coord.mup \
	crossbeams.mup css.mup curves.mup \
	emptymeas.mup endings.mup extchar.mup fonts.mup \
	grace.mup groupalign.mup gtc.mup hasspace.mup \
	ifclause.mup interfere.mup keysig.mup \
	labels.mup latin1.mup ledger.mup lyrics.mup \
	macros.mup marks.mup measnum.mup miditest.mup \
	multcontext.mup multistuff.mup muschar.mup \
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
