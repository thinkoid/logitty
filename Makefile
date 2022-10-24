# -*- mode: makefile; -*-

PREFIX = /usr/local

WARNINGS = -W -Wall -Werror

CFLAGS = -g -O -pedantic $(WARNINGS)
CPPFLAGS = -I.

LDFLAGS =
LIBS = -lncurses -lform -lpam

DEPENDDIR = ./.deps
DEPENDFLAGS = -M

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

TARGET = logitty

all: $(TARGET)

DEPS = $(patsubst %.o,$(DEPENDDIR)/%.d,$(OBJS))
-include $(DEPS)

$(DEPENDDIR)/%.d: %.c $(DEPENDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPENDFLAGS) $< >$@

$(DEPENDDIR):
	@[ ! -d $(DEPENDDIR) ] && mkdir -p $(DEPENDDIR)

%: %.c

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ -c $<

clean:
	@rm -rf $(TARGET) $(OBJS)

realclean:
	@rm -rf $(TARGET) $(OBJS) $(DEPENDDIR)

install: $(TARGET)
	@install $^ $(PREFIX)/bin
	@cp -r etc/pam.d/* /etc/pam.d/
	@cp -r etc/s6/logitty-log /etc/s6/sv/
	@cp -r etc/s6/logitty-srv /etc/s6/sv/
