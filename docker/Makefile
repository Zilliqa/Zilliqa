
.PHONY: all ci k8s ubuntu-1804 release release-cuda

# build the default zilliqa docker image same as the one on Docker Hub
all: release release-cuda

len=$(shell echo $(COMMIT) | wc -c)
commit=$(shell echo $(COMMIT) | cut -c 1-7)

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

get-scilla:
	docker pull zilliqa/scilla:latest

# build zilliqa docker image for CI usage
ci:
	docker build -f dev/Dockerfile.ci -t zilliqa-ci .

# test build on Ubuntu 18.04
ubuntu-1804: check-commit
	docker build -f dev/Dockerfile --build-arg COMMIT=$(commit) --build-arg BASE=ubuntu:18.04 .

# build zilliqa docker image for Kubernetes usage
k8s: check-commit get-scilla
	cat dev/Dockerfile dev/Dockerfile.k8s | docker build -t zilliqa:$(commit) \
		--build-arg COMMIT=$(commit) -

# build release version docker image for public usage
release: get-scilla
	docker build -f Dockerfile -t zilliqa/zilliqa:v$(major).$(minor).$(fix) .

release-cuda: get-scilla
	docker build -f Dockerfile.cuda -t zilliqa/zilliqa:v$(major).$(minor).$(fix)-cuda .
