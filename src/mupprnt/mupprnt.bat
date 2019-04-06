@echo off
rem  --- Before running this, make sure you edit the 'printertype'
rem  --- below to match your specific printer. Also make sure GS_LIB
rem  --- is set properly to be able to access the Ghostscript font files,
rem  --- and that your path includes the directories for MUP.EXE and GS386.EXE
rem
rem  --- run Mup. If Mup is not in your path, you could give the
rem  --- complete path here, like C:\MUP\MUP
MUP %1 %2 %3 %4 %5 %6 %7 %8 %9 >mupxxx.tmp
if errorlevel 1 goto end
rem
rem    NOTE:	replace the 'printertype' in the gs386 command below with the
rem		appropriate name for your specific printer.
rem		Different versions of Ghostscript may support
rem		different printer types, so execute GS386 -h
rem		to see what your version supports.
rem
rem	printertype		printer
rem
rem	bj10e	  Canon BubbleJet BJ10e
rem 	cdeskjet  H-P DeskJet 500C with 1 bit/pixel color
rem 	cdjcolor  H-P DeskJet 500C with 24 bit/pixel color and
rem		     high-quality color (Floyd-Steinberg) dithering
rem 	cdjmono   H-P DeskJet 500C printing black only
rem 	deskjet   H-P DeskJet and DeskJet Plus
rem 	dfaxhigh  DigiBoard, Inc.'s DigiFAX software format (high resolution)
rem 	dfaxlow   DigiFAX low (normal) resolution
rem	djet500   H-P DeskJet 500
rem 	djet500c  H-P DeskJet 500C
rem	epson	  Epson-compatible dot matrix printers (9- or 24-pin)
rem 	eps9high  Epson-compatible 9-pin, interleaved lines
rem		     (triple resolution)
rem 	epsonc	  Epson LQ-2550 and Fujitsu 3400/2400/1200 color printers
rem 	laserjet  H-P LaserJet
rem 	la50	  DEC LA50 printer
rem 	la75	  DEC LA75 printer
rem 	lbp8	  Canon LBP-8II laser printer
rem 	ln03	  DEC LN03 printer
rem 	lj250	  DEC LJ250 Companion color printer
rem 	ljet2p	  H-P LaserJet IId/IIp/III* with TIFF compression
rem 	ljet3	  H-P LaserJet III* with Delta Row compression
rem 	ljetplus  H-P LaserJet Plus
rem 	necp6	  NEC P6/P6+/P60 printers at 360 x 360 DPI resolution
rem	paintjet  H-P PaintJet color printer
rem 	pjetxl	  H-P PaintJet XL color printer
rem 	r4081	  Ricoh 4081 laser printer
rem 	tek4696   Tektronix 4695/4696 inkjet plotter
rem
rem  --- run Ghostscript on output of Mup
rem  *** change the 'printertype' to match your printer
rem  You also need to make sure the GS386.EXE command is available in your path
GS386 -sDEVICE=printertype -sOutputFile=mupgs.tmp -dNOPAUSE mupxxx.tmp
rem
rem  --- send output to printer
copy /b mupgs.tmp prn
rem
rem  --- remove temp files
del mupgs.tmp
:end
del mupxxx.tmp
