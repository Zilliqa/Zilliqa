# Annoyingly our build systems needs to invoke `git submodule update`, which means we need `.git` in the Docker build
# context. Not unreasonably, Tilt doesn't let this happen: https://docs.tilt.dev/file_changes.html#git. Therefore, we
# need to use a `custom_build` instead as advised.
# What we want:
#docker_build("zilliqa/zilliqa", ".", dockerfile="docker/Dockerfile")
custom_build("zilliqa/zilliqa", "DOCKER_BUILDKIT=1 docker build . -t $EXPECTED_REF -f docker/Dockerfile", ["docker/Dockerfile"])

# TODO: Deploy nginx ingress with Tilt, so the user doesn't need to do it manually.
#k8s_yaml("infra/k8s/nginx-ingress.yaml")

k8s_yaml("infra/k8s/localstack.yaml")
k8s_resource("localstack", port_forwards="4566:4566")

k8s_yaml(kustomize("infra/k8s/base"))
k8s_resource("devnet-explorer", port_forwards="8110:80")

USE_ZILSEED = os.path.exists("../zilseed/Dockerfile") and os.path.exists("../zilseed/k8s.yaml")
if USE_ZILSEED:
    custom_build("zilliqa/zilseed", "DOCKER_BUILDKIT=1 docker build ../zilseed -t $EXPECTED_REF", ["../zilseed/Dockerfile"])
    k8s_yaml("../zilseed/k8s.yaml")
