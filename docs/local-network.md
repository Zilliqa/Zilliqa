# Running a local network

1. Install [kind](https://kind.sigs.k8s.io/#installation-and-usage).
1. Install [tilt](https://docs.tilt.dev/install.html).
1. Create and switch to a kind cluster.

    ```
    kind create cluster --config infra/kind-cluster.yaml
    kubectl config use-context kind-kind
    ```

1. Deploy the nginx ingress controller and wait for it to start up.

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

# Updating the network

Sadly our networks are not very resilient, so we can't rely on Kubernetes' automatic rollouts to restart nodes with a new image.
Instead, the easiest thing to do at the moment is to tear down the network and re-create it by running `tilt down`, waiting for all the pods to be deleted and then `tilt up` again.

# Localdev

There is a script, `scripts/localdev.py` which helps automate some of these tasks - run it with no arguments for details.

# Logging

You can log from nodes with command lines like:

```sh
kubectl logs --tail=100 -f -l type=normal | tee /tmp/logs.txt 2>&1
```
