#
# Copyright (c) 2016 Nest Labs, Inc.
# All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/common \
    -I$(top_srcdir)/src/missing \
	$(NULL)

DISTCLEANFILES = \
	.deps \
	Makefile \
	$(NULL)

EXTRA_DIST = \
	concordd.conf \
    concordd-dbus.conf \
	$(NULL)

pkginclude_HEADERS = \
    concordd-dbus.h \
	$(NULL)

sysconf_DATA = \
	concordd.conf  \
	$(NULL)

bin_PROGRAMS = concordd

dbusconfdir = $(DBUS_CONFDIR)
dbusconf_DATA = concordd-dbus.conf

concordd_SOURCES = \
	main.c \
	concordd.c \
	concordd.h \
    concordd-dbus-server.c \
    concordd-dbus-server.h \
    concordd-dbus.h \
	ge-rs232.c \
	ge-rs232.h \
	concordd-config.h \
    ../common/time-utils.c \
    ../common/socket-utils.c \
    ../common/string-utils.c \
    ../common/config-file.c \
    ../common/crash-trace.c \
	$(NULL)

concordd_LDADD =  $(DBUS_LIBS) $(MISSING_LIBADD)

concordd_CPPFLAGS = $(AM_CPPFLAGS) $(DBUS_CFLAGS) $(MISSING_CPPFLAGS)

SOURCE_VERSION=$(shell                                            \
	git describe --dirty --always --match "[0-9].*" 2> /dev/null  \
)

BUILT_SOURCES  = $(top_builddir)/$(subdir)/version.c
CLEANFILES     = $(top_builddir)/$(subdir)/version.c
.INTERMEDIATE:   concordd-version.$(OBJEXT)

$(top_builddir)/$(subdir)/version.c: ../version.c.in Makefile
	sed 's/SOURCE_VERSION/"$(SOURCE_VERSION)"/' < $< > $@

concordd_SOURCES += $(top_builddir)/$(subdir)/version.c
