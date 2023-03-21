# Annoyingly our build systems needs to invoke `git submodule update`, which means we need `.git` in the Docker build
# context. Not unreasonably, Tilt doesn't let this happen: https://docs.tilt.dev/file_changes.html#git. Therefore, we
# need to use a `custom_build` instead as advised.
# What we want:
#docker_build("zilliqa/zilliqa", ".", dockerfile="docker/Dockerfile")
#custom_build("zilliqa/zilliqa", "DOCKER_BUILDKIT=1 docker build . -t $EXPECTED_REF -f docker/Dockerfile.all", ["docker/Dockerfile"])
# Can't use localhost here or OS X thinks it should use ipv6, but podman can't route to both ipv4 and ipv6 on the same port..
#default_registry("127.0.0.1:5001")

is_localdev = os.environ.get("LOCALDEV")
print("is_localdev %s"%is_localdev)

#if is_localdev:
#   print("Using localdev .. ")
#   custom_build("zilliqa/zilliqa", "scripts/localdev.py build $EXPECTED_REF", [ "scripts/build_dev.py", "docker/Dockerfile.dev" ], skips_local_docker=True)
#else:
#   print("Using full dockerfile .. ")
#   custom_build("zilliqa/zilliqa", "DOCKER_BUILDKIT=1 docker build . -t $EXPECTED_REF -f docker/Dockerfile", ["docker/Dockerfile"])

#custom_build("zilliqa/zilliqa", "scripts/localdev.py build-lite $EXPECTED_REF", [ "docker/Dockerfile.lite" ], skips_local_docker=True)
custom_build("zilliqa", "scripts/localdev.py build-lite $EXPECTED_REF", [ "docker/Dockerfile.lite" ])

# TODO: Deploy nginx ingress with Tilt, so the user doesn't need to do it manually.
k8s_yaml("infra/k8s/nginx-ingress.yaml")

OBSERVABILITY = os.getenv('OBSERVABILITY', '')
if OBSERVABILITY:
   # Install the observability stack: prometheus, grafana, tempo
   k8s_yaml(kustomize("infra/k8s/monitoring"))
   k8s_yaml("infra/k8s/prometheus-metrics-reader.yaml")
   k8s_resource(objects=["prometheus-ingress:Ingress:monitoring"], new_name="prometheus-ingress")
   k8s_resource("prometheus-ingress", resource_deps=['ingress-nginx-controller'])
   k8s_resource(objects=["grafana-ingress:Ingress:monitoring"], new_name="grafana-ingress")
   k8s_resource("grafana-ingress", resource_deps=['ingress-nginx-controller'])
   k8s_resource("loki-loki-stack-test", resource_deps=['loki'])

k8s_yaml("infra/k8s/localstack.yaml")
k8s_resource("localstack", port_forwards="4566:4566")

k8s_yaml(kustomize("infra/k8s/base"))
#k8s_resource("devnet-origin", trigger_mode = TRIGGER_MODE_MANUAL)
#k8s_resource("devnet-dsguard", trigger_mode = TRIGGER_MODE_MANUAL)
#k8s_resource("devnet-lookup", trigger_mode = TRIGGER_MODE_MANUAL)
#k8s_resource("devnet-multiplier", trigger_mode = TRIGGER_MODE_MANUAL)
#k8s_resource("devnet-normal", trigger_mode = TRIGGER_MODE_MANUAL)
#k8s_resource("devnet-seedpub", trigger_mode = TRIGGER_MODE_MANUAL)
#k8s_resource("devnet-explorer", port_forwards="8110:80")
