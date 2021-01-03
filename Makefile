LIBRARY := libcregex.a
OBJS := \
           src/compile.o \
           src/parse.o \
           src/vm.o
deps := $(OBJS:%.o=%.o.d)
SUBDIRS := tests

CFLAGS  += -std=c11 -Wall -pedantic
CFLAGS  += -Iinclude 

.PHONY: all
all: CFLAGS   += -DNDEBUG -O2
all: $(LIBRARY) $(SUBDIRS)

.PHONY: debug
debug: CFLAGS   += -DDEBUG -g
debug: LDFLAGS  += -g
debug: $(LIBRARY) $(SUBDIRS)

.PHONY: clean
clean:
	for subdir in $(SUBDIRS); do       \
		$(MAKE) -C $$subdir clean; \
	done
	$(RM) $(LIBRARY) $(OBJS) $(deps)

$(LIBRARY): $(OBJS)
	$(AR) rcs $@ $^

$(SUBDIRS): $(LIBRARY)
	$(MAKE) -C $@

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MF $@.d -c -o $@ $<

-include $(deps)
