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

DOCKER_IMAGE ?= villas/node
DOCKER_OPTS ?= -it -p 80:80 -p 443:443 -p 1234:1234 -p 12000:12000/udp -p 12001:12001/udp --privileged --security-opt seccomp:unconfined -v "$(PWD):/villas"

deploy-docker-dev: DOCKER_IMAGE := $(DOCKER_IMAGE)-dev
docker-dev: DOCKER_IMAGE := $(DOCKER_IMAGE)-dev

deploy-docker deploy-docker-dev:
	docker push $(DOCKER_IMAGE):$(GIT_BRANCH)

# Docker targets
run-docker-dev: docker-dev
	docker run $(DOCKER_OPTS) $(DOCKER_IMAGE)-dev:$(GIT_BRANCH)

docker-dev:
	docker pull fedora:25
	docker build -f Dockerfile.dev -t $(DOCKER_IMAGE):$(GIT_BRANCH) $(SRCDIR)
	
docker:
	docker pull fedora:25
	docker build -f Dockerfile -t $(DOCKER_IMAGE):$(GIT_BRANCH) $(SRCDIR)

.PHONY: docker deploy-docker docker-dev run-docker-dev