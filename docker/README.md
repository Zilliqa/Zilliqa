# Zilliqa Dockerfiles

This folder includes multiple Dockerfiles

- `Dockerfile`: The Dockerfile to build Zilliqa image
- `Dockerfile.deps`: The Dockerfile to build Zilliqa dependencies image

## Getting Started

A few common commands are available in `Makefile`.

- `make`: build `release` and `deps`
- `make deps`: build the dependencies
- `make release`: build a release version image for public usage

Also for development:

- `make k8s COMMIT=XXXXXXX`: build Zilliqa image for Kubernetes testnets
