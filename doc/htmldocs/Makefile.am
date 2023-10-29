mup_html_docs = blind.html faq.html Macinst.html mupfeat.html overview.html usercomm.html whatsnew.html winrun.html quickref.html
mup_htmldoc_dir = $(datadir)/doc/packages/$(PACKAGE)
mup_htmldoc__DATA = $(mup_html_docs)
EXTRA_DIST = $(mup_html_docs)

quickref.html: ../quickref.t ../../tools/doc/tr2html
	../../tools/doc/tr2html < ../quickref.t > /dev/null
