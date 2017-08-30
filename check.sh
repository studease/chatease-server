#!/bin/bash

while true; do
HTTP_CODE=`curl -G -I -m 2 -o /dev/null -s -w %{http_code}"\n" -H "Connection:Upgrade" -H "Upgrade:websocket" -H "Sec-WebSocket-Key:t6QkrxoCFWCRezSZApB/5w==" http://127.0.0.1/stats?name=admin`;

echo "Status code: ${HTTP_CODE}."

if [ $HTTP_CODE != 101 ]; then
	echo "Unexpected status code: ${HTTP_CODE}."
	
	pids=$(ps x | grep chatease-server | grep -v grep | awk '{print $1}')
	for pid in $pids; do
		echo "Killing process ${pid}..."
		kill -9 $pid
	done
	
	./start.sh
	echo "Restarted chatease-server."
fi

sleep 5s

done

