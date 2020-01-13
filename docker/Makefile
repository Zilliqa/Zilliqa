
.PHONY: all release release-cuda

# build the default zilliqa docker image same as the one on Docker Hub
all: release release-cuda

len=$(shell echo $(COMMIT) | wc -c)
commit=$(shell echo $(COMMIT) | cut -c 1-7)
cmake_extra_args=$(shell echo $(EXTRA_CMAKE_ARGS))

major=$(shell tail -n +2 ../VERSION | head -n1)
minor=$(shell tail -n +4 ../VERSION | head -n1)
fix=$(shell tail -n +6 ../VERSION | head -n1)

check-commit:
	@if [ -z "$(COMMIT)" ]; \
	then \
		echo "COMMIT=xxxxxxx is not passed in after make target"; \
		exit 1; \
	fi
	@if [ $(len) -lt 7 ]; \
	then \
		echo "COMMIT doesn't have enough length (at least 7)"; \
		exit 1; \
	fi

# build zilliqa docker image for Kubernetes usage
k8s: check-commit
	docker build -t zilliqa:$(commit) -f Dockerfile \
		--build-arg COMMIT=$(commit) --build-arg EXTRA_CMAKE_ARGS="$(cmake_extra_args)" .

# build release version docker image for public usage
release:
	docker build -f Dockerfile -t zilliqa/zilliqa:v$(major).$(minor).$(fix) .

release-cuda:
	docker build -f Dockerfile.cuda -t zilliqa/zilliqa:v$(major).$(minor).$(fix)-cuda .
