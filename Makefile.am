AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4
DIST_TARGETS = dist-gzip dist-bzip2
DIST_ARCHIVES = $(distdir).tar.gz $(distdir).tar.bz2

SUBDIRS =                              \
	src                                \
	doc                                \
	$(NULL)

DISTCLEANFILES =                       \
	config.log                         \
	config.status                      \
	Makefile                           \
	libtool                            \
	make.out                           \
	$(distdir).tar.*                   \
	$(NULL)

EXTRA_DIST =                           \
	.default-version                   \
	.gitignore                         \
	bootstrap.sh                       \
	README.md                          \
	LICENSE                            \
	etc/Dockerfile                     \
	etc/build-in-docker.sh             \
	etc/run-in-docker.sh               \
	m4/nl.m4                           \
	$(NULL)



HASH_VERSION = $(shell                                                   \
	git describe --dirty --match __poison__ --always 2> /dev/null        \
)

EXTRA_VERSION = $(shell                                                  \
	git describe --all --contains 99e0155e435a32d80adc80748b870d1daa7179c2 1> /dev/null 2> /dev/null \
		&& (test `git describe | sed 's:^full/::'` = $(VERSION) || echo -g$(HASH_VERSION))  \
)

distdir = $(PACKAGE)-$(VERSION)$(EXTRA_VERSION)
