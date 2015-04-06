
# this works in gnu make
SYSNAME:=${shell uname}
OBJTYPE:=${shell uname -m | sed 's;i.86;386;; s;/.*;;'}

# this works in bsd make
SYSNAME!=uname
OBJTYPE!=uname -m | sed 's;i.86;386;; s;/.*;;'

# the gnu rules will mess up bsd but not vice versa,
# hence the gnu rules come first.

include Make.$(SYSNAME)-$(OBJTYPE)

PREFIX=/usr/local

NUKEFILES=

TGZFILES=

LIB=libutf.a
VERSION=2.0
PORTPLACE=devel/libutf
NAME=libutf

OFILES=\
	rune.$O\
	runestrcat.$O\
	runestrchr.$O\
	runestrcmp.$O\
	runestrcpy.$O\
	runestrdup.$O\
	runestrlen.$O\
	runestrecpy.$O\
	runestrncat.$O\
	runestrncmp.$O\
	runestrncpy.$O\
	runestrrchr.$O\
	runestrstr.$O\
	runetype.$O\
	utfecpy.$O\
	utflen.$O\
	utfnlen.$O\
	utfrrune.$O\
	utfrune.$O\
	utfutf.$O\

HFILES=\
	utf.h\

all: $(LIB)

install: $(LIB)
	test -d $(PREFIX)/man/man3 || mkdir $(PREFIX)/man/man3
	install -m 0644 isalpharune.3 $(PREFIX)/man/man3/isalpharune.3
	install -m 0644 utf.7 $(PREFIX)/man/man7/utf.7
	install -m 0644 rune.3 $(PREFIX)/man/man3/rune.3
	install -m 0644 runestrcat.3 $(PREFIX)/man/man3/runestrcat.3
	install -m 0644 utf.h $(PREFIX)/include/utf.h
	install -m 0644 $(LIB) $(PREFIX)/lib/$(LIB)

# for now - we probably should connect this with the Google makefile so
# we use the same compiler at all times...
CC = /home/build/buildtools/production/gcc-2.95.3/bin/i686-google-linux-gcc

$(LIB): $(OFILES)
	$(AR) $(ARFLAGS) $(LIB) $(OFILES)

NUKEFILES+=$(LIB)
.c.$O:
	$(CC) $(CFLAGS) -I$(PREFIX)/include $*.c

%.$O: %.c
	$(CC) $(CFLAGS) -I$(PREFIX)/include $*.c


$(OFILES): $(HFILES)

tgz:
	rm -rf $(NAME)-$(VERSION)
	mkdir $(NAME)-$(VERSION)
	cp Makefile Make.* README LICENSE NOTICE *.[ch137] rpm.spec bundle.ports $(TGZFILES) $(NAME)-$(VERSION)
	tar cf - $(NAME)-$(VERSION) | gzip >$(NAME)-$(VERSION).tgz
	rm -rf $(NAME)-$(VERSION)

clean:
	rm -f $(OFILES)

nuke:
	rm -f $(OFILES) *.tgz *.rpm $(NUKEFILES)

rpm:
	make tgz
	cp $(NAME)-$(VERSION).tgz /usr/src/RPM/SOURCES
	rpm -ba rpm.spec
	cp /usr/src/RPM/SRPMS/$(NAME)-$(VERSION)-1.src.rpm .
	cp /usr/src/RPM/RPMS/i586/$(NAME)-$(VERSION)-1.i586.rpm .
	scp *.rpm rsc@amsterdam.lcs.mit.edu:public_html/software

PORTDIR=/usr/ports/$(PORTPLACE)

ports:
	make tgz
	rm -rf $(PORTDIR)
	mkdir $(PORTDIR)
	cp $(NAME)-$(VERSION).tgz /usr/ports/distfiles
	cat bundle.ports | (cd $(PORTDIR) && awk '$$1=="---" && $$3=="---" { ofile=$$2; next} {if(ofile) print >ofile}')
	(cd $(PORTDIR); make makesum)
	(cd $(PORTDIR); make)
	(cd $(PORTDIR); /usr/local/bin/portlint)
	rm -rf $(PORTDIR)/work
	shar `find $(PORTDIR)` > ports.shar
	(cd $(PORTDIR); tar cf - *) | gzip >$(NAME)-$(VERSION)-ports.tgz
	scp *.tgz rsc@amsterdam.lcs.mit.edu:public_html/software

.phony: all clean nuke install tgz rpm ports
