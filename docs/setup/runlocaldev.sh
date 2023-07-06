#! /usr/bin/bash

source ~/setenv.sh
cd ~/src/zilliqa/zilliqa
./scripts/localdev.py setup
IP=`minikube ip`
cat <<EOF | sudo tee -a /etc/hosts
${IP} localdev-api.localdomain
${IP} localdev-explorer.localdomain
${IP} localdev-l2api.localdomain
${IP} localdev-newapi.localdomain
${IP} localdev-origin.localdomain
${IP} localdev-origin-internal.localdomain
EOF
./scripts/localdev.py up
