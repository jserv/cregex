PROGRAM := driver cli re2dot

CFLAGS  += -std=c11 -Wall -pedantic
CFLAGS  += -I ../include

.PHONY: all
all: CPPFLAGS += -DNDEBUG
all: CFLAGS   += -O2
all: $(PROGRAM) $(SUBDIRS)

.PHONY: debug
debug: CPPFLAGS += -DDEBUG
debug: CFLAGS   += -g
debug: LDFLAGS  += -g
debug: $(PROGRAM) $(SUBDIRS)

cli: cli.o ../libcregex.a
	$(CC) $(LDFLAGS) -o $@ $^ ../libcregex.a

re2dot: re2dot.o ../libcregex.a
	$(CC) $(LDFLAGS) -o $@ $^ ../libcregex.a

driver: driver.o ../libcregex.a
	$(CC) $(LDFLAGS) -o $@ $^ ../libcregex.a

../libcregex.a:
	$(MAKE) -C .. libcregex.a

basic.dat:
	wget -O $@ https://golang.org/src/regexp/testdata/basic.dat?m=text
	# FIXME: clarify if it was an imcomplete test item
	sed -i '/9876543210/d' $@

nullsubexpr.dat:
	wget -O $@ https://golang.org/src/regexp/testdata/nullsubexpr.dat?m=text

repetition.dat:
	wget -O $@ https://golang.org/src/regexp/testdata/repetition.dat?m=text

driver.c: generator.rb basic.dat nullsubexpr.dat repetition.dat
	./generator.rb basic.dat nullsubexpr.dat repetition.dat > $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	$(RM) $(PROGRAM) *.o
distclean: clean
	$(RM) driver.c basic.dat nullsubexpr.dat repetition.dat
