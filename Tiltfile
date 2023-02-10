# Annoyingly our build systems needs to invoke `git submodule update`, which means we need `.git` in the Docker build
# context. Not unreasonably, Tilt doesn't let this happen: https://docs.tilt.dev/file_changes.html#git. Therefore, we
# need to use a `custom_build` instead as advised.
# What we want:
#docker_build("zilliqa/zilliqa", ".", dockerfile="docker/Dockerfile")
custom_build("zilliqa/zilliqa", "DOCKER_BUILDKIT=1 docker build . -t $EXPECTED_REF -f docker/Dockerfile", ["docker/Dockerfile"])

k8s_yaml("infra/k8s/nginx-ingress.yaml")

OBSERVABILITY = os.getenv('OBSERVABILITY', False)

if OBSERVABILITY:
    # Install the observability stack: prometheus, grafana, tempo, loki
    k8s_yaml(kustomize("infra/k8s/monitoring"))
    k8s_resource(objects=["prometheus-ingress:Ingress:monitoring"], new_name="prometheus-ingress")
    k8s_resource("prometheus-ingress", resource_deps=['ingress-nginx-controller'])
    k8s_resource(objects=["grafana-ingress:Ingress:monitoring"], new_name="grafana-ingress")
    k8s_resource("grafana-ingress", resource_deps=['ingress-nginx-controller'])
    k8s_resource("loki-loki-stack-test", resource_deps=['loki'])
    k8s_yaml("infra/k8s/prometheus-metrics-reader.yaml")


# Install the observability stack: prometheus, grafana, tempo
k8s_yaml(kustomize("infra-devops/k8s/monitoring"))
k8s_resource(objects=["prometheus-ingress:Ingress:monitoring"], new_name="prometheus-ingress")
k8s_resource("prometheus-ingress", resource_deps=['ingress-nginx-controller'])
k8s_resource(objects=["grafana-ingress:Ingress:monitoring"], new_name="grafana-ingress")
k8s_resource("grafana-ingress", resource_deps=['ingress-nginx-controller'])
k8s_resource("loki-loki-stack-test", resource_deps=['loki'])
# Move the prometheus-metrics scraper out from the basic config.
# It can been run optionally when the observability platform is `enabled`
k8s_yaml("infra-devops/k8s/prometheus-metrics-reader.yaml")


k8s_yaml("infra-devops/k8s/localstack.yaml")
k8s_resource("localstack", port_forwards="4566:4566")

k8s_yaml(kustomize("infra-devops/k8s/base"))
k8s_resource("devnet-explorer", port_forwards="8110:80")