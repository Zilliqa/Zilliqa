# Zilliqa Dockerfiles

This folder includes multiple Dockerfiles

- `Dockerfile`: main Zilliqa Dockerfile containing build instructions, also read by Docker Hub Automated Build.
- `Dockerfile.ci`: CI environment Dockerfile containing CI tools and dependencies
- `Dockerfile.k8s`: *partial* Dockerfile containing dependencies needed for testnets running on Kubernetes

## Getting Started

A few most useful commands are provided through `Makefile` for quick access. The Dockerfiles can be passed to `docker build` with proper build arguments, when operated by experienced users.

- `make`: build a standard Zilliqa image for the latest commit on `master` branch
- `make ci`: build CI image used by CI services for testing
- `make k8s-zilliqa COMMIT=XXXXXXX`: build Zilliqa image for Kubernetes testnets
- `make k8s-scilla COMMIT=XXXXXXX`: build Zilliqa image with Scilla for Kubernetes testnets
- `make release COMMIT=XXXXXXX`: build a release version image for public usage
- `make release-cuda COMMIT=XXXXXXX`: build a release version image with CUDA driver for public usage
