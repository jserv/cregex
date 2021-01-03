PROGS := driver cli re2dot

PROGS := $(addprefix tests/,$(PROGS))

OBJS := src/compile.o \
        src/parse.o \
        src/vm.o
deps := $(OBJS:%.o=%.o.d)

CFLAGS  += -std=c11 -Wall -pedantic
CFLAGS  += -Iinclude 

.PHONY: all
all: CFLAGS   += -DNDEBUG -O2
all: $(PROGS)

.PHONY: debug
debug: CFLAGS   += -DDEBUG -g
debug: LDFLAGS  += -g
debug: $(PROGS)

tests/cli: tests/cli.o $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

tests/re2dot: tests/re2dot.o $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

tests/driver: tests/driver.o $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

TESTDATA = basic.dat nullsubexpr.dat repetition.dat
TESTDATA := $(addprefix tests/,$(TESTDATA))

tests/basic.dat:
	wget -O $@ https://golang.org/src/regexp/testdata/basic.dat?m=text
	# FIXME: clarify if it was an imcomplete test item
	sed -i '/9876543210/d' $@

tests/nullsubexpr.dat:
	wget -O $@ https://golang.org/src/regexp/testdata/nullsubexpr.dat?m=text

tests/repetition.dat:
	wget -O $@ https://golang.org/src/regexp/testdata/repetition.dat?m=text

tests/driver.c: tests/generator.rb $(TESTDATA)
	tests/generator.rb $(TESTDATA) > $@

check: $(PROGS)
	tests/driver

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MF $@.d -c -o $@ $<

.PHONY: clean
clean:
	$(RM) $(PROGS) tests/*.o $(OBJS) $(deps)

distclean: clean
	$(RM) tests/driver.c $(TESTDATA)

-include $(deps)
