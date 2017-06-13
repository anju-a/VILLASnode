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

LIB_NAME = libvillas
LIB_ABI_VERSION = 1
LIB = $(BUILDDIR)/$(LIB_NAME).so.$(LIB_ABI_VERSION)

LIBS += $(LIB)

# Object files for libvillas
LIB_SRCS += $(addprefix lib/nodes/, file.c cbuilder.c shmem.c) 		\
            $(addprefix lib/, sample.c path.c node.c hook.c	\
               log.c log_config.c utils.c super_node.c hist.c timing.c pool.c	\
               list.c queue.c queue_signalled.c memory.c advio.c web.c api.c	\
               plugin.c node_type.c stats.c mapping.c sample_io.c shmem.c \
               json.c crypt.c compat.c \
            )

LIB_LDFLAGS = -shared
LIB_LDLIBS  = $(LDLIBS) -ldl -lrt -Wl,-soname,$(LIB_NAME).so.$(LIB_ABI_VERSION)

LIB_PKGS += libwebsockets openssl

# Add flags by pkg-config
LIB_LDLIBS += $(shell $(PKGCONFIG) --libs $(LIB_PKGS))
LIB_CFLAGS += $(shell $(PKGCONFIG) --cflags $(LIB_PKGS))

LIB_OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(LIB_SRCS))
LIB_DEPS = $(patsubst %.c, $(BUILDDIR)/%.d, $(LIB_SRCS))

$(LIB_NAME): $(LIB)

$(LIB): $(LIB_OBJS)
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LIB_LDLIBS)
	ln -srf $@ $(BUILDDIR)/$(LIB_NAME).so

install-$(LIB_NAME): $(LIB_NAME)
	install -m 0755 -D -T $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so.$(LIB_ABI_VERSION)
	install -m 0644 -D -t $(DESTDIR)$(PREFIX)/include/villas/ include/villas/*.h
	ln -srf $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME).so.$(LIB_ABI_VERSION)
	ldconfig

clean-$(LIB_NAME):
	rm -f $(LIB_OBJS) $(LIB_DEPS) $(LIB)

.PHONY: install-$(LIB_NAME) clean-$(LIB_NAME) $(LIB)
