
.PHONY: all k8s release release-cuda

# build the default zilliqa docker image same as the one on Docker Hub
all: release release-cuda

len=$(shell echo "$(COMMIT_OR_TAG)" | wc -c)
commit_or_tag=$(shell git rev-parse --short=7 "$(COMMIT_OR_TAG)")
cmake_extra_args=$(shell echo $(EXTRA_CMAKE_ARGS))

major=$(shell tail -n +2 ../VERSION | head -n1)
minor=$(shell tail -n +4 ../VERSION | head -n1)
fix=$(shell tail -n +6 ../VERSION | head -n1)

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

# Build docker images for public usage
base:
	docker build -f Dockerfile.base -t zilliqa-base:v$(major).$(minor).$(fix) .

devbase: check-commit
	docker build -t zilliqa-devbase:v$(major).$(minor).$(fix) \
		--build-arg ZILLIQA_BASE_VERSION="v$(major).$(minor).$(fix)" \
		--build-arg COMMIT_OR_TAG="$(COMMIT_OR_TAG)" -f Dockerfile.devbase .

deps: check-commit
	docker build -t zilliqa-deps:v$(major).$(minor).$(fix) \
		--build-arg ZILLIQA_DEVBASE_VERSION="v$(major).$(minor).$(fix)" \
		--build-arg COMMIT_OR_TAG="$(COMMIT_OR_TAG)" -f Dockerfile.deps .

ccache: check-commit
	docker build -t zilliqa-ccache:v$(major).$(minor).$(fix) \
		--build-arg ZILLIQA_DEVBASE_VERSION="v$(major).$(minor).$(fix)" \
		--build-arg ZILLIQA_DEPS_VERSION="v$(major).$(minor).$(fix)" \
		--build-arg COMMIT_OR_TAG="$(COMMIT_OR_TAG)" -f Dockerfile.ccache .

# FIXME: change to debug build
dev: check-commit
	docker build -t zilliqa:v$(major).$(minor).$(fix) \
		--build-arg ZILLIQA_BASE_VERSION="v$(major).$(minor).$(fix)" \
		--build-arg ZILLIQA_CCACHE_VERSION="v$(major).$(minor).$(fix)" \
		--build-arg COMMIT_OR_TAG="$(COMMIT_OR_TAG)" \
		--build-arg EXTRA_CMAKE_ARGS="$(cmake_extra_args)" -f Dockerfile .

release: check-commit
	docker build -t zilliqa:v$(major).$(minor).$(fix) \
		--build-arg ZILLIQA_BASE_VERSION="v$(major).$(minor).$(fix)" \
		--build-arg ZILLIQA_CCACHE_VERSION="v$(major).$(minor).$(fix)" \
		--build-arg COMMIT_OR_TAG="$(COMMIT_OR_TAG)" \
		--build-arg EXTRA_CMAKE_ARGS="$(cmake_extra_args)" -f Dockerfile .

