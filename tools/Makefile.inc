# Makefile.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###################################################################################

TOOLS_SRCS = tools/conf2json.c

TOOLS_CFLAGS = $(CFLAGS)
TOOLS_LDLIBS = -lconfig -ljansson -lvillas
TOOLS_LDFLAGS = $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'

ifeq ($(shell $(PKGCONFIG) libzmq; echo $$?),0)
	TOOLS_SRCS   += tools/zmq-keygen.c
	TOOLS_CFLAGS += $(shell $(PKGCONFIG) --cflags libzmq)
	TOOLS_LDLIBS += $(shell $(PKGCONFIG) --libs libzmq)
endif

TOOLS_BINS = $(patsubst tools/%.c, $(BUILDDIR)/villas-%, $(TOOLS_SRCS))
TOOLS_DEPS = $(patsubst tools/%.c, $(BUILDDIR)/tools/%.d, $(TOOLS_SRCS))
TOOLS_OBJS = $(patsubst tools/%.c, $(BUILDDIR)/tools/%.o, $(TOOLS_SRCS))

# Compile executable objects
$(BUILDDIR)/tools/%.o: tools/%.c $(BUILDDIR)/defines | $$(dir $$@)
	$(CC) $(TOOLS_CFLAGS) -c $< -o $@

# Link target executables
$(TOOLS): $(BUILDDIR)/villas-%: $(BUILDDIR)/tools/%.o | $(LIBS)
	$(CC) $(TOOLS_LDFLAGS) $^ $(TOOLS_LDLIBS) -o $@

tools: $(TOOLS)

clean-tools:
	rm -f $(TOOLS_BINS) $(TOOLS_OBJS) $(TOOLS_DEPS)

install-tools: $(TOOLS)
	install -m 0755 tools/villas.sh $(DESTDIR)$(PREFIX)/bin/villas
	install -m 0755 -D -t $(DESTDIR)$(PREFIX)/bin $(TOOLS)

.PHONY: tools clean-tools install-tools