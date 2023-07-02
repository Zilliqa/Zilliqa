# Running localdev on a cloud ubuntu machine.

DANGER WILL ROBINSON! Currently, this script will only work within
Zilliqa, since `testnet` is not yet a public repository. We're working
on it ..

## Create a machine

This will depend on your setup, but if you're using GCP (you'll need different magic for AWS):

```
gcloud compute instances create <machine-name> \
--image-family ubuntu-2204-lts \
--image-project ubuntu-os-cloud \
--boot-disk-size=200GB \
--metadata enable-oslogin=TRUE \
--project <my-project> \
--zone europe-west1-c \
--machine-type e2-standard-4 \
--subnet west1 \
--no-address \
--no-service-account --no-scopes
```

## Setup

Copy the files in this directory into your home directory on the remote:

```
gcloud compute --project <my-project> scp *.sh <my-machine>:~
```

Now ssh in with `gcloud compute ssh <machine> --tunnel-through-iap`

Run:

```sh
./installme.sh
```

This will configure and install required packages.

It will also print out an ssh key (which, if you need a replay, can be found in `~/.ssh/id_rsa.pub`) and check out zilliqa in
`~/work/zilliqa/{zilliqa,scilla,testnet}` .

Feel free to run `installme.sh` as many times as you like - it will skip work it has already done.

## Setenv and source

Source `~/setenv.sh` before working with the source - it sets up paths
and other environment variables required by the build. You don't need
to source `setenv.sh` when running `installme.sh` - `installme.sh`
makes its own arrangements.

Source code for zilliqa is placed in `TARGET_DIR`, set by default at
`~/src/zilliqa` (you can edit `installme.sh` to change it if you
like).

`installme.sh` creates an ssh key and asks you to install it in github
so that it can clone repositories.  If you have your own ssh key you'd
prefer to use, put it in `~/.ssh/id_rsa.pub` and `installme.sh` won't
generate another.

## Start isolated server

You can build and run an isolated server using:

```sh
source ./setenv.sh
cd ~/src/zilliqa/zilliqa
./scripts/localdev.py isolated
```

## Start localdev

Now, log out and log back in (you'll need to be in the docker group) and...

```sh
source ./setenv.sh
cd ~/src/zilliqa/zilliqa
./scripts/localdev.py setup
./scripts/localdev.py up
```

Do:

```sh
minikube ip
```

and list the resulting IP in `/etc/hosts` like this:

```txt
192.168.49.2 localdev-api.localdomain
192.168.49.2 localdev-explorer.localdomain
192.168.49.2 localdev-l2api.localdomain
192.168.49.2 localdev-newapi.localdomain
192.168.49.2 localdev-origin.localdomain
192.168.49.2 localdev-origin-internal.localdomain
```

You should find that:

```sh
kubectl get pod
```

shows that your pods are up.

You can try doing this automatically with


## Accessing localdev from your desktop

Do:

```sh
gcloud compute ssh <machine> --project <project> --tunnel-through-iap -- -D 9000 -L9001:192.168.49.2:80
```

The `-D 9000` starts a SOCKS proxy at `socks5://localhost:9000` which proxies into your cloud host.

Now:

```sh
mkdir /tmp/mydir
google-chrome --user-data-dir=/tmp/p --proxy-server='socks5://localhost:9000' 
```

And in the chrome window that opens, navigate to
`http://localdev-explorer.localdomain` .  You should see DevEx. Add a
network, call it what you like, and make the URL:
`http://localdev-l2api.localdomain`. 

You should now have DevEx running. If you want to make calls:

```sh
mitmweb --mode 'reverse:http://localhost:9001' --modify-headers "/~q/Host/localdev-l2api.localdomain" --no-web-open-browser --listen-port 9100 --web-port 9101
```

Will open a port on `localhost:9100` you can use to talk to localdev, and monitor on port 9101.


## Known bugs

 * If you have problems setting up minikube, `minikube delete` and try again.


