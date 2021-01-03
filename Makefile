PROGS := driver cli re2dot
PROGS := $(addprefix tests/,$(PROGS))

OBJS := src/compile.o \
        src/parse.o \
        src/vm.o
deps := $(OBJS:%.o=%.o.d) $(PROGS:%=%.o.d)

include mk/common.mk

CC      ?= gcc
CFLAGS  += -std=c11 -Wall -pedantic
CFLAGS  += -Iinclude 

.PHONY: all
all: CFLAGS   += -DNDEBUG -O2
all: $(OBJS) $(PROGS)

.PHONY: debug
debug: CFLAGS   += -DDEBUG -g
debug: LDFLAGS  += -g
debug: $(OBJS) $(PROGS)

include mk/test-data.mk
tests/driver.c: tests/generator.rb $(TESTDATA)
	$(VECHO) "  GEN\t$@\n"
	$(Q)tests/generator.rb $(TESTDATA) > $@
check: tests/driver
	$(Q)$<

%.o: %.c
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) $(CFLAGS) -MMD -MF $@.d -c -o $@ $<

tests/%: tests/%.o $(OBJS)
	$(VECHO) "  CC+LD\t$@\n"
	$(Q)$(CC) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) $(PROGS) $(PROGS:%=%.o) $(OBJS) $(deps)
distclean: clean
	-$(RM) tests/driver.c $(TESTDATA)

-include $(deps)
