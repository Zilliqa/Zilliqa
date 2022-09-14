
.PHONY: all k8s release release-cuda

# build the default zilliqa docker image same as the one on Docker Hub
all: release release-cuda

len=$(shell echo $(COMMIT_OR_TAG) | wc -c)
commit_or_tag=$(shell echo $(COMMIT_OR_TAG) | cut -c 1-7)
cmake_extra_args=$(shell echo $(EXTRA_CMAKE_ARGS))

major=$(shell tail -n +2 ../VERSION | head -n1)
minor=$(shell tail -n +4 ../VERSION | head -n1)
fix=$(shell tail -n +6 ../VERSION | head -n1)

check-commit:
	@if [ -z "$(COMMIT_OR_TAG)" ]; \
	then \
		echo "COMMIT_OR_TAG=xxxxxxx is not passed in after make target"; \
		exit 1; \
	fi
	@if [ $(len) -lt 7 ]; \
	then \
		echo "COMMIT_OR_TAG doesn't have enough length (at least 7)"; \
		exit 1; \
	fi

# build zilliqa docker image for Kubernetes usage
dev: check-commit
	docker build -t zilliqa:$(commit_or_tag) \
		--build-arg COMMIT_OR_TAG=$(commit_or_tag) --build-arg EXTRA_CMAKE_ARGS="$(cmake_extra_args)" -f Dockerfile .

# build release version docker image for public usage
release:
	docker build -f Dockerfile -t zilliqa/zilliqa:v$(major).$(minor).$(fix) .

release-cuda:
	docker build -f Dockerfile.cuda -t zilliqa/zilliqa:v$(major).$(minor).$(fix)-cuda .
