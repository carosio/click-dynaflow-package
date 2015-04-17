# Get started with DynaFlow click development
## Prerequisites

### Required packages
 * Ubuntu >= 14.10 Utopic
 * Make sure you use universe repo

```
sudo apt-get update
sudo apt-get install git make g++ autoconf erlang-base erlang-parsetools erlang-tools erlang-dev erlang-lager libboost-dev ethtool
```

### tetrapak

```
git clone https://github.com/travelping/tetrapak
cd tetrapak
make
sudo make install
```


### Click


You *must* compile with C++11 support

```
git clone git://github.com/kohler/click.git
cd click
CXXFLAGS="-std=gnu++11" ./configure --disable-linuxmodule --enable-user-multithread --enable-all-elements
make
sudo make install
```


## Build / Install

### click-dynaflow-package

Clone from https://github.com/carosio/click-dynaflow-package, use the ts-master branch.

```
git clone git@github.com/carosio/click-dynaflow-package.git
cd click-dynaflow-package
autoconf
./configure
make
sudo make install
```


### click_dp

```
git clone git@github.com/carosio/click-dp.git
cd click_dp
tetrapak build
```

## Run / Test

### Start test setup

The run.sh script creates a test setup composed of three separated network namespaces (intern, extern, click) with appropriate network configuration and starts both epmd and click within the click namespace using a dynaflow sample config.


```
cd click-dynaflow-package
sudo sample/run.sh
```



#### Start DP API wrapper

```
cd click_dp
sudo ip netns exec click /usr/lib/erlang/bin/tetrapak start -sname click -setcookie secret
```


#### Network configuration within the namespaces

 * click
   * interfaces: `intern`, `extern`
 * intern
   * interface `intern`
     * inet 192.168.110.16/24
     * default via 192.168.110.1
 * extern
   * interface `extern`
     * inet 192.168.100.16/24
     * default via 192.168.100.1


### Ping
Ping from intern to extern

```
sudo ip netns exec intern ping 192.168.100.16
```
