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

REQS_CMAKE = libxil libwebsockets criterion jansson nanomsg
REQS_AUTOCONF = libnl libconfig libcurl libzmq

REQS = $(REQS_CMAKE) $(REQS_AUTOCONF)

ifdef DEBUG
	CMAKE_OPTS += -DCMAKE_BUILD_TYPE=DEBUG
	
	AC_CPPFLAGS=-DDEBUG
	AC_CFLAGS=-g -O0
	AC_CXXFLAGS=-g -O0
endif

CONFIGURE_OPTS = --prefix=$(PREFIX)

thirdparty:

# Install & compile autotools based projects
$(REQS_AUTOCONF): CPPFLAGS=$(AC_CPPFLAGS) CFLAGS=$(AC_CFLAGS) CXXFLAGS=$(AC_CXXFLAGS)
$(REQS_AUTOCONF): | $(BUILDDIR)/thirdparty/$$@/
	autoreconf -fi $(SRCDIR)/thirdparty/$@
	cd $(BUILDDIR)/thirdparty/$@ && $(SRCDIR)/thirdparty/$@/configure $(CONFIGURE_OPTS) && make

# Install & compile CMake based projects
$(REQS_CMAKE): | $(BUILDDIR)/thirdparty/$$@/
	cmake -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
	      -DCMAKE_INSTALL_LIBDIR=$(PREFIX)/lib64 -DLIB_SUFFIX=64 \
	      -H$(SRCDIR)/thirdparty/$@ \
	      -B$(BUILDDIR)/thirdparty/$@ $(CMAKE_OPTS)
	make  -C$(BUILDDIR)/thirdparty/$@

$(addprefix install-,$(REQS)): install-%: %
	make -C$(BUILDDIR)/thirdparty/$(@:install-%=%) install
	ldconfig

$(addprefix clean-,$(REQS)):
	if [ -d $(BUILDDIR)/thirdparty/$(@:clean-%=%) ]; then make -C$(BUILDDIR)/thirdparty/$(@:clean-%=%) clean; fi

install-thirdparty: $(addprefix install-,$(REQS))

clean-thirdparty: $(addprefix clean-,$(REQS))

# libconfig's build system is currently broken.
# This is a workaround for: https://github.com/hyperrealm/libconfig/issues/53
libconfig: | libconfig-fix
libconfig-fix:
	rm -f $(SRCDIR)/thirdparty/libconfig/lib/scanner.[hc]

libwebsockets: CMAKE_OPTS += -DLWS_IPV6=1 -DLWS_WITH_STATIC=0 -DLWS_WITHOUT_TESTAPPS=1 -DLWS_WITH_HTTP2=1
libzmq:    CONFIGURE_OPTS += --with-libsodium --with-pgm --enable-drafts

.PHONY: $(REQS) thirdparty clean-thirdparty install-thirdparty