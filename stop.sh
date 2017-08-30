#!/bin/bash

pids=$(ps x | grep chatease-server | grep -v grep | awk '{print $1}')
for pid in $pids; do
	echo "Killing chatease-server process ${pid}..."
	kill -9 $pid
done

