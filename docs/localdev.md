# Using localdev

The instructions obtained by running 

```sh
scripts/localdev.py
```

without arguments should get you going. There are a few things you might find it useful to know though.

## Proxies

Each incoming port on the ingress is mirrored through an instance of mitmweb; the ports themselves start at 5300, and the mitmweb interfaces start at 8300. 

You can find out which port is which via `localdev.py show-proxy`

## Ingress restart

Sometimes the ingress sticks and connections to it hang; this hangs the explorer and other ports. You can find out if this has happened with:

```sh
curl http://localdev-origin.localdomain/
```

If that hangs, then `localdev.py restart-ingress` will boot it for you.

## localdev adds isolated server accounts

So you can use the accounts from `isolated-server-accounts.json` to test your transactions.

## Logging

`localdev` can capture logs for you, so if the cluster gets into an odd state:

```sh
scripts/localdev.py normal 90 'HelpMe'
```

will tell you which normal nodes emitted a log containing `HelpMe` in the last 90s.

You can also do things like sending a txn, then 90s later, saying:

```sh
scripts/localdev.py log-snapshot 120
```

Which generates a directory in `/tmp` where it puts the logs of all pods from the last 120s, allowing you to easily see how the transaction flowed through the system.

## Troubleshooting

### Linux attempts to use a rootless docker context


If `localdev setup` fails with:

```
📌  Using rootless Docker driver

❌  Exiting due to MK_USAGE: --container-runtime must be set to "containerd" or "cri-o" for rootless

```

You have probably configured docker to be rootless; you can fix this with:

```
docker context create rootful --docker host=unix:///var/run/docker.sock
docker context use rootful
```

### Linux packages to install

Helm: See https://helm.sh/docs/intro/install/
