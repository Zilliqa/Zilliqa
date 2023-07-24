#!/usr/bin/bash

echo 1 > /proc/sys/net/ipv4/ip_forward || exit 1

bridge_name="zil-br0"
ip link del "${bridge_name}" > /dev/null 2>&1

for i in $(seq 10 50); do
  ns="zil-ns-${i}"
  ip netns del "${ns}" > /dev/null 2>&1
done

ip link add "${bridge_name}" type bridge || exit 1
ip addr add 10.1.1.1/24 dev "${bridge_name}"
ip link set dev "${bridge_name}" up || exit 1

for i in $(seq 10 50); do
  ns="zil-ns-${i}"
  ip netns add "${ns}"
  ip netns exec "${ns}" ip link set dev lo up || exit 1

  veth_name="en0s${i}"
  veth_peer_name="zil-veth-${i}"
  ip link del "${veth_peer_name}" > /dev/null 2>&1
  ip link add "${veth_name}" type veth peer name "${veth_peer_name}" || exit 1

  ip link set "${veth_name}" netns "${ns}"
  ip netns exec "${ns}" ip addr add 10.1.1.${i}/24 dev "${veth_name}"
  ip netns exec "${ns}" ip link set "${veth_name}" up

  ip link set "${veth_peer_name}" master "${bridge_name}" || exit 1
  ip link set "${veth_peer_name}" up
done

