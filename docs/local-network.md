# Running a local network

1. Install [kind](https://kind.sigs.k8s.io/#installation-and-usage) or `brew`.
1. Install [tilt](https://docs.tilt.dev/install.html) or `brew`.
1. Create and switch to a kind cluster.

    ```
    kind create cluster --config infra/kind-cluster.yaml
    kubectl config use-context kind-kind
    ```

1. __[DEPRECATED] Deploy the nginx ingress controller and wait for it to start up.__

*Tilt is now deploying the nginx ingress controller*

    ```
    kubectl apply -f https://raw.githubusercontent.com/kubernetes/ingress-nginx/main/deploy/static/provider/kind/deploy.yaml
    kubectl wait --namespace ingress-nginx --for=condition=ready pod --selector=app.kubernetes.io/component=controller --timeout=300s
    ```

1. Run Tilt to deploy the network.

    ```
    tilt up
    ```

1. Obtain the IP address of the kind cluster.

    ```
    export KIND_IP_ADDR=$(docker container inspect kind-control-plane --format '{{ .NetworkSettings.Networks.kind.IPAddress }}')
    ```

1. Set up a reverse proxy which will rewrite the `Host` header for requests to the cluster.
(This example uses https://mitmproxy.org/)

    ```
    mitmweb --mode reverse:http://$KIND_IP_ADDR --modify-headers '/~q/Host/l2api.local.z7a.xyz'
    ```

1. Check on the state of your cluster in the usual ways (`kubectl`, `k9s`, etc.).
Note that it may take a while to download the container image and start up.
1. Make sure devex is configured to talk to `http://localhost:8080`.

# Access the observability framework

1. Set up a reverse proxy which will forward all the requests to the ingress nginx binded on `localhost:80`.
(This example uses https://mitmproxy.org/)

    ```
     mitmweb --mode reverse:http://localhost -p 8081 --set keep_host_header=true
    ```
## Browser access
1. Configure a proxy for your browser to forward all the requests for `*.local.z7a.xyz` to to mitmweb proxy
on `127.0.0.1:8081`
(This example uses Google Chrome Extension SwitchyOmega https://chrome.google.com/webstore/detail/proxy-switchyomega/)

## Grafana URL: http://grafana.local.z7a.xyz
Grafana login credentials:

```bash
    username: admin
    password: admin
```

## Prometheus URL: http://prometheus.local.z7a.xyz
## Tempo cluster endpoint: tempo.monitoring.svc.cluster.local

## Tempo service endpoints by supported protocol:
| endpoint | Description |
|-----------| ------------|
| tempo.monitoring.svc.cluster.local:3100 | Prometheus metrics |
| tempo.monitoring.svc.cluster.local:16687 | Jaeger metrics |
| tempo.monitoring.svc.cluster.local:16686| Tempo Query Jaeger ui |
| tempo.monitoring.svc.cluster.local:6831 | Jaeger thrift compact |
| tempo.monitoring.svc.cluster.local:6832 | Jaeger thrift binary |
| tempo.monitoring.svc.cluster.local:14268 | Jaeger thrift HTTP |
| tempo.monitoring.svc.cluster.local:14250 | GPRC Jaeger |
| tempo.monitoring.svc.cluster.local:9411 | Zipkin |
| tempo.monitoring.svc.cluster.local:55680 | OTLP legacy |
| tempo.monitoring.svc.cluster.local:55681 | OTLP HTTP legacy |
| tempo.monitoring.svc.cluster.local:4317 | GPRC OLTP |
| tempo.monitoring.svc.cluster.local:4318 | OLTP HTTP |
| tempo.monitoring.svc.cluster.local:55678 | Opencensus |



# Updating the network

Sadly our networks are not very resilient, so we can't rely on Kubernetes' automatic rollouts to restart nodes with a new image.
Instead, the easiest thing to do at the moment is to tear down the network and re-create it by running `tilt down`, waiting for all the pods to be deleted and then `tilt up` again.
