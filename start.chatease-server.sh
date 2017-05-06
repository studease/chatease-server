#!/bin/bash
docker run -it -d \
 --restart always \
 --name chatease-server \
 -p 8000:80 \
 chatease-server
