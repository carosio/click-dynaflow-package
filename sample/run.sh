#!/bin/bash
#
# This wrapper creates three network namespaces in total.
#
# A root namespace called 'click' that is used to run click itself (and the
# required epmd instance). Any control instance also needs to run in this
# namespace.
#
# The two other namespaces are called extern and intern. They correspond
# to the click interfaces of the same name.

which click > /dev/null 2>&1 || {
    echo "click userlevel driver not found, make sure click is installed and in your PATH"
    exit 1
}

if [ "$(id -u)" != '0' ]; then
    echo "$0 must be run as root"
    exit 1
fi

function finish {
    # clean up
    ip netns exec click epmd -kill > /dev/null 2>&1
    ip netns delete intern > /dev/null 2>&1
    ip netns delete extern > /dev/null 2>&1
    ip netns delete click > /dev/null 2>&1
}
trap finish EXIT

function click_if {
    ip link add name $1 netns $1 type veth peer name $1 netns click address $2

    ip netns exec $1 ethtool -K $1 rx off tx off
    ip netns exec $1 ip addr add $3/$5 dev $1
    ip netns exec $1 ip link set $1 up
    ip netns exec $1 ip route add default via $4 dev $1

    ip netns exec click ethtool -K $1 rx off tx off
    # ip netns exec click ip addr add $4/$5 dev $1
    ip netns exec click ip link set $1 up
}

function init_ns {
    ip netns add $1
    ip netns exec $1 ip link set lo up
}

init_ns intern
init_ns extern
init_ns click

ip netns exec click epmd -daemon -relaxed_command_check

# this has to match click's AddressInfo
click_if intern 00:50:ba:85:84:a9 192.168.110.16 192.168.110.1 24
click_if extern 00:e0:98:09:ab:af 192.168.100.16 192.168.100.1 24

echo "EXTERN"
ip netns exec extern ip addr
ip netns exec extern ip route

echo "INTERN"
ip netns exec intern ip addr
ip netns exec intern ip route

echo "CLICK"
ip netns exec click ifconfig -a

export CLICKPATH=$PWD

ip netns exec click click -f sample/dynaflow.click $@
