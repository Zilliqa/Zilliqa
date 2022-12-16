
.PHONY: all release

all: release

cmake_extra_args=$(shell echo $(EXTRA_CMAKE_ARGS))

major=$(shell tail -n +2 ../VERSION | head -n1)
minor=$(shell tail -n +4 ../VERSION | head -n1)
fix=$(shell tail -n +6 ../VERSION | head -n1)

tag="v$(major).$(minor).$(fix)"

release: docker build -t zilliqa:$tag --build-arg EXTRA_CMAKE_ARGS="$(cmake_extra_args)" -f Dockerfile ..
