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

LIB_CFLAGS  = $(CFLAGS) -fPIC

include lib/hooks/Makefile.inc
include lib/api/Makefile.inc
include lib/web/Makefile.inc
include lib/kernel/Makefile.inc
include lib/nodes/Makefile.inc
include lib/rtds/Makefile.inc

include lib/Makefile.villas.inc
include lib/Makefile.villas-ext.inc

# Compile
$(BUILDDIR)/lib/%.o: lib/%.c $(BUILDDIR)/defines | $$(dir $$@)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

lib: $(LIBS)

install-lib: install-libvillas install-libvillas-ext
	
clean-lib: clean-libvillas clean-libvillas-ext

.PHONY: lib install-lib clean-lib
