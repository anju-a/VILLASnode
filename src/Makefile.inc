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

# Executables
SRCS = src/node.c \
	   src/pipe.c  \
	   src/signal.c \
	   src/test-rtt.c \
	   src/test-cmp.c \
	   src/test-shmem.c \
	   src/hook.c

SRC_LDLIBS = $(LDLIBS) -pthread -lm -lvillas
SRC_CFLAGS = $(CFLAGS)
SRC_LDFLAGS = $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'

# Enable VILLASfpga support when libxil is available
ifeq ($(shell $(PKGCONFIG) libxil; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, fpga.c)
	LIB_SRCS    += $(addprefix lib/kernel/, pci.c vfio.c)
	LIB_SRCS    += $(wildcard  lib/fpga/*.c)
	SRCS        += src/fpga.c
	PKGS        += libxil

	# Add support for LAPACK / BLAS benchmarks / solvers
	ifeq ($(shell $(PKGCONFIG) blas lapack; echo $$?),0)
		PKGS        += blas lapack
		BENCH_OBJS  += fpga-bench-overruns.o
	endif
endif

BINS = $(patsubst src/%.c, $(BUILDDIR)/villas-%, $(SRCS))
OBJS = $(patsubst src/%.c, $(BUILDDIR)/src/%.o, $(SRCS))
DEPS = $(patsubst src/%.c, $(BUILDDIR)/src/%.d, $(SRCS))

src: $(BINS) lib

$(BINS): $(BUILDDIR)/villas-%: $(BUILDDIR)/src/%.o

# Some additional prereqs for individual binaries
$(BUILDDIR)/villas-fpga: $(addprefix $(BUILDDIR)/src/,fpga-bench.o $(BENCH_OBJS))

# Compile executable objects
$(BUILDDIR)/src/%.o: src/%.c $(BUILDDIR)/defines | $$(dir $$@)
	$(CC) $(SRC_CFLAGS) -c $< -o $@

# Build villas-shmem only with libvillas-ext (to ensure that libext contains everything needed)
$(BUILDDIR)/villas-shmem: $(BUILDDIR)/src/shmem.o libvillas-ext
	$(CC) $(SRC_LDFLAGS) $(BUILDDIR)/src/shmem.o $(filter-out -lvillas,$(SRC_LDLIBS)) -lvillas-ext -o $@

# Link target executables
$(BINS): | $(LIBS)
	$(CC) $(SRC_LDFLAGS) $^ $(SRC_LDLIBS) -o $@

# Install
install-src: src install-lib
	install -m 0755 -D -t $(DESTDIR)$(PREFIX)/bin $(BINS)

clean-src:
	rm -f $(DEPS) $(OBJS) $(BINS)

.PHONY: src install-src clean-src
