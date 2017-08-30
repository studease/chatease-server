#!/bin/bash

pids=$(ps x | grep ./check.sh | grep -v grep | awk '{print $1}')
for pid in $pids; do
	echo "Killing check process ${pid}..."
	kill -9 $pid
done

