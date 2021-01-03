TESTDATA = basic.dat nullsubexpr.dat repetition.dat
TESTDATA := $(addprefix tests/,$(TESTDATA))

tests/basic.dat:
	$(VECHO) "  Downloading $@ ...\n"
	$(Q)wget -q -O $@ https://golang.org/src/regexp/testdata/basic.dat?m=text
	# FIXME: clarify if it was an imcomplete test item
	$(Q)sed -i '/9876543210/d' $@

tests/nullsubexpr.dat:
	$(VECHO) "  Downloading $@ ...\n"
	$(Q)wget -q -O $@ https://golang.org/src/regexp/testdata/nullsubexpr.dat?m=text

tests/repetition.dat:
	$(VECHO) "  Downloading $@ ...\n"
	$(Q)wget -q -O $@ https://golang.org/src/regexp/testdata/repetition.dat?m=text
