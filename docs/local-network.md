# Running a local network

1. Install [kind](https://kind.sigs.k8s.io/#installation-and-usage).
1. Install [tilt](https://docs.tilt.dev/install.html).
1. Note that previous versions of this guide used a cluster called
   `kind` rather than `zqdev` - if you've used those you will have a
   `kind` cluster called `kind`.  You can either delete it, or just
   change `zqdev` to `kind` in what follows. Do not attempt to run two
   copies of Zilliqa on one machine - they will fight over their port
   assignments.
1. Delete any previous dev cluster, just in case:

    ```sh
   kind delete cluster zqdev
    ```

1. Create and switch to a kind cluster.

    ```
    kind create cluster --name zqdev --config infra/kind-cluster.yaml
    kubectl config use-context kind-zqdev
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
    export KIND_IP_ADDR=$(docker container inspect zqdev-control-plane --format '{{ .NetworkSettings.Networks.kind.IPAddress }}')
    ```

1. Set up a reverse proxy which will rewrite the `Host` header for requests to the cluster.
(This example uses https://mitmproxy.org/)

    ```
    mitmweb --mode reverse:http://$KIND_IP_ADDR --modify-headers '/~q/Host/l2api.local.z7a.xyz'
    ```

Note that `mitmweb` invokes your browser on launch. If you're running
`brave`, `brave` won't go into the background when this happens, so
`mitmweb` won't start. To "fix" this, manually quit brave, and `mitmweb` should start.
You can then navigate manually to `localhost:8081` to see the mitm UI.


1. Check on the state of your cluster in the usual ways (`kubectl`, `k9s`, etc.).
Note that it may take a while to download the container image and start up.

1. Make sure devex is configured to talk to `http://localhost:8080`.

# Updating the network

Sadly our networks are not very resilient, so we can't rely on Kubernetes' automatic rollouts to restart nodes with a new image.
Instead, the easiest thing to do at the moment is to tear down the network and re-create it by running `tilt down`, waiting for all the pods to be deleted and then `tilt up` again.
