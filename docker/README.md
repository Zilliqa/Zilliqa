# Zilliqa Dockerfiles

This folder includes multiple Dockerfiles

- `Dockerfile`: The Dockerfile to build Zilliqa image
- `Dockerfile.cuda`: The Dockerfile to build Zilliqa image with CUDA support
- `dev/*`: Other Dockerfiles for development usage

## Getting Started

A few common commands are available in `Makefile`.

- `make`: build `release` and `release-cuda`
- `make release`: build a release version image for public usage
- `make release-cuda`: build a release version image with CUDA driver for public usage

Also for development:

- `make ci`: build CI image used by CI services for testing
- `make k8s COMMIT=XXXXXXX`: build Zilliqa image for Kubernetes testnets
