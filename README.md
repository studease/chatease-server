# chatease-server

> [[domain] http://studease.cn](http://studease.cn/chatease-server.html)

> [[source] https://github.com/studease/chatease-server](https://github.com/studease/chatease-server)

> [[中文] http://blog.csdn.net/icysky1989/article/details/52138527](http://blog.csdn.net/icysky1989/article/details/52138527)

> 公众号：STUDEASE

> QQ群：528109813

This is a websocket chat server.


## Note
-------

This server was renamed as [kiwichatd](http://studease.cn/kiwichatd.html) at version 2.0.


## Description
--------------

For the Enterprise Edition, supply an interface to return a JSON object formatted as /data/userinfo.json.

While a client connecting to upgrade protocol, it sends an identify upstream request, carrying channel and token params, 
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


## Client
-------------------

[chatease.js https://github.com/studease/chatease](https://github.com/studease/chatease)


## License
----------

Apache License 2.0
