# Annoyingly our build systems needs to invoke `git submodule update`, which means we need `.git` in the Docker build
# context. Not unreasonably, Tilt doesn't let this happen: https://docs.tilt.dev/file_changes.html#git. Therefore, we
# need to use a `custom_build` instead as advised.
# What we want:
#docker_build("zilliqa/zilliqa", ".", dockerfile="docker/Dockerfile")
custom_build("zilliqa/zilliqa", "DOCKER_BUILDKIT=1 docker build . -t $EXPECTED_REF -f docker/Dockerfile", ["docker/Dockerfile"])

#k8s_yaml("infra/k8s/nginx-ingress.yaml")

k8s_yaml(kustomize("infra/k8s/base"))
k8s_resource("devnet-explorer", port_forwards="8110:80")

# kubectl apply -f https://raw.githubusercontent.com/kubernetes/ingress-nginx/main/deploy/static/provider/kind/deploy.yaml
# kubectl wait --namespace ingress-nginx --for=condition=ready pod --selector=app.kubernetes.io/component=controller --timeout=300s
