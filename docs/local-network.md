# Running a local network

1. Install [kind](https://kind.sigs.k8s.io/#installation-and-usage).
1. Create and switch to a kind cluster.

    ```
    kind create cluster --config infra/kind-cluster.yaml
    kubectl config use-context kind-kind
    ```

1. (Optionally, if you want to use an image from AWS) create a Kubernetes secret containing the docker registry credentials.
You may need to authenticate to AWS first.

    ```
    kubectl create secret docker-registry ecr-creds --docker-server 648273915458.dkr.ecr.us-west-2.amazonaws.com --docker-username AWS --docker-password $(aws ecr get-login-password)
    ```

1. Deploy the nginx ingress controller and wait for it to start up.

    ```
    kubectl apply -f https://raw.githubusercontent.com/kubernetes/ingress-nginx/main/deploy/static/provider/kind/deploy.yaml
    kubectl wait --namespace ingress-nginx --for=condition=ready pod --selector=app.kubernetes.io/component=controller --timeout=300s
    ```

1. (Optionally) change the Zilliqa container image in `infra/k8s/base/kustomization.yaml`.
1. Deploy the network.

    ```
    kubectl apply -k infra/k8s/base
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
