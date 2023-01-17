
.PHONY: all release

all: release

commit_or_tag=$(shell git rev-parse --short=7 "$(COMMIT_OR_TAG)")
cmake_extra_args=$(shell echo $(EXTRA_CMAKE_ARGS))

major=$(shell tail -n +2 ../VERSION | head -n1)
minor=$(shell tail -n +4 ../VERSION | head -n1)
fix=$(shell tail -n +6 ../VERSION | head -n1)

tag="v$(major).$(minor).$(fix)"

check-commit:
	@if [ -z "$(commit_or_tag)" ]; \
	then \
		echo "COMMIT_OR_TAG=xxxxxxx is not passed in after make target"; \
		exit 1; \
	fi
	@if [ $(len) -lt 7 ]; \
	then \
		echo "COMMIT_OR_TAG doesn't correspond to a proper commit"; \
		exit 1; \
	fi

# FIXME: change to debug build
dev: check-commit
	DOCKER_BUILDKIT=1 docker build -t zilliqa:$(commit_or_tag) \
		--build-arg EXTRA_CMAKE_ARGS="$(cmake_extra_args)" -f Dockerfile ..
release:
	DOCKER_BUILDKIT=1 docker build -t zilliqa:v$(major).$(minor).$(fix) \
		--build-arg EXTRA_CMAKE_ARGS="$(cmake_extra_args)" -f Dockerfile ..
