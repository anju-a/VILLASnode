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

TAROPTS = --exclude-ignore-recursive=.distignore --transform='s|^\.|villas-node-$(VERSION_NUM)|' --show-transformed-names

TAR_VILLAS = $(BUILDDIR)/packaging/villas-node-$(VERSION_NUM)-1.$(GIT_BRANCH)_$(subst -,_,$(VARIANT)).$(shell date +%Y%m%d)git$(GIT_REV).tar.gz

# Source tarballs
dist: $(TAR_VILLAS)

$(TAR_VILLAS): | $$(dir $$@)
	tar $(TAROPTS) -C $(SRCDIR) -czf $@ .

deploy-dist: $(TAR_VILLAS)
	rsync $(TAR_VILLAS) $(DEPLOY_USER)@$(DEPLOY_HOST):$(DEPLOY_PATH)/src

clean-dist:
	rm -f $(TAR_VILLAS)

.PHONY: dist clean-dist deploy-dist