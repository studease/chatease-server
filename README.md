# chatease-server

> [[domain] http://studease.cn](http://studease.cn)

> [[source] https://github.com/studease/chatease-server](https://github.com/studease/chatease-server)

This is a websocket chat server.


## Note
-------

This server was renamed as [kiwichatd](http://studease.cn/kiwichatd.html) at version 2.0, and closed source.


## Prepare
----------

For the Enterprise Edition, supply an interface to return a JSON object formatted as /data/userinfo.json.

While a client is connecting to upgrade protocol, it sends an identify upstream request, carrying channel and token params, 
to get the user info, which will decide whether the operation will be satisfied.

The Preview Edition is more like a stand-alone server. The user info, includes name, icon, role, and channel state could be
present in params. However, this is not safe.


## Build
--------

To build chatease-server, you need CMake 3.5 and or above, and OpenSSL installed. Then run:

```
cmake .
make
```


## Run
------

```
sudo ./start.sh
```


## Websocket Client
-------------------

[chatease.js https://github.com/studease/chatease](https://github.com/studease/chatease)


## Software License
-------------------

Apache License 2.0
