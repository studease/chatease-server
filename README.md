# chatease-server

> [[domain] http://studease.cn](http://studease.cn)

> [[source] https://github.com/studease/chatease-server](https://github.com/studease/chatease-server)


## Build
--------

### Lua

[Download lua](http://www.lua.org/download.html)

```
curl -R -O http://www.lua.org/ftp/lua-5.3.3.tar.gz
tar zxf lua-5.3.3.tar.gz
cd lua-5.3.3
make linux test
```

You may need readline-devel:

```
yum install readline-devel
```

### Lua CJSON

If you want to run with the sample script applications/main.lua, CJSON is needed.

```
wget https://www.kyne.com.au/~mark/software/download/lua-cjson-2.1.0.tar.gz
tar zxf lua-cjson-2.1.0.tar.gz
cd lua-cjson-2.1.0
make & make install
```

### CMake

To build chatease-server, you need CMake 3.5 and or above. Then run:

```
mkdir build 
cd build
cmake ..
```

## Run
------

```
sudo ./chatease-server
```

## Interface
------------

* **onAppStart**
* **onAppStop**

* **onConnect**
* **onDisconnect**

* **onMessage**

For more details, please check the main.lua and c sources.

## Software License
-------------------

Apache License 2.0
