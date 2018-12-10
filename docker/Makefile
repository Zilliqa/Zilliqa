
.PHONY: all ci k8s-zilliqa k8s-scilla ubuntu-1804 release release-cuda

# build the default zilliqa docker image same as the one on Docker Hub
all:
	docker build -t zilliqa .

# build zilliqa docker image for CI usage
ci:
	docker build -t zilliqa-ci -f Dockerfile.ci .

# test build on Ubuntu 18.04
ubuntu-1804: check-commit
	docker build --build-arg COMMIT=$(commit) --build-arg BASE=ubuntu:18.04 -t zilliqa .

len=$(shell echo $(COMMIT) | wc -c)
commit=$(shell echo $(COMMIT) | cut -c 1-7)

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

get-scilla:
	docker pull zilliqa/scilla:latest

# build zilliqa docker image for Kubernetes usage
k8s-zilliqa: check-commit
	cat Dockerfile Dockerfile.k8s | docker build -t zilliqa:$(commit) \
		--build-arg COMMIT=$(commit) -

k8s-scilla: check-commit get-scilla
	cat Dockerfile Dockerfile.k8s | docker build -t zilliqa:$(commit).scilla \
		--build-arg COMMIT=$(commit) --build-arg BASE=zilliqa/scilla -

# build release version docker image for public usage
release: check-commit get-scilla
	docker build -t zilliqa:release --build-arg BASE=zilliqa/scilla --build-arg COMMIT=$(commit) .

release-cuda: check-commit get-scilla
	docker build -t scilla:cuda --build-arg BASE=zilliqa/scilla -f Dockerfile.cuda .
	docker build -t zilliqa:release-cuda --build-arg BASE=scilla:cuda --build-arg EXTRA_CMAKE_ARGS="-DCUDA_MINE=1" --build-arg COMMIT=$(commit) .

