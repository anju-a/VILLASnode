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

FMI_SRCDIR = $(SRCDIR)/plugins/fmi
FMI_BUILDDIR = $(BUILDDIR)/plugins/fmi

FMI_SRCS = $(wildcard $(FMI_SRCDIR)/*.c)
FMI_OBJS = $(patsubst %.c, $(FMI_BUILDDIR)/%.o, $(FMI_SRCS))

FMI_LDFLAGS = $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'
FMI_LDLIBS = -lvillas

PLUGINS += $(FMI_BUILDDIR)/villas.fmu

fmi: $(FMI_BUILDDIR)/villas.fmu

$(FMI_BUILDDIR)/villas.fmu: $(FMI_CONTENTS)
	zip -x \*.o -r $@ $(FMI_BUILDDIR)

# TODO: Link statically

ifeq ($(PLATFORM),Linux)
FMI_CONTENTS += $(FMI_BUILDDIR)/binaries/linux64/villas.so

$(FMI_BUILDDIR)/binaries/linux64/villas.so: $(FMI_OBJS) lib
	$(LD) -o $@ $< $(FMI_LDFLAGS) $(FMI_LDLIBS)

endif

ifeq ($(PLATFORM),Darwin)
FMI_CONTENTS += $(FMI_BUILDDIR)/binaries/darwin64/villas.dylib

$(FMI_BUILDDIR)/binaries/darwin64/villas.dylib: $(FMI_OBJS) lib

ifeq ($(PLATFORM),Win)
FMI_CONTENTS += $(FMI_BUILDDIR)/binaries/win64/villas.dll

$(FMI_BUILDDIR)/binaries/win64/villas.dll: $(FMI_OBJS) lib

endif

.PHONY: fmi

#include $(SRCDIR)/plugins/fmi/documentation/Makefile.inc
