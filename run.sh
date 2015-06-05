#!/bin/sh

set -x

for var in $(ps -u $(basename $HOME) | grep server)
do
	pid=$(echo $var | cut -c1-5)
	pname=$(echo $var | cut -c25-)

	if kill -2 $pid
	then
		echo "$pname stop"
	else
		echo "$pname cannot stop"
	fi
done

cond=$(ps -u $(basename $HOME) | grep server | wc -l)
while [ $cond -gt 0 ]
do
	sleep 1
	cond=$(ps -u $(basename $HOME) | grep server | wc -l)
	echo "server remain:$cond"
done

bin/hubserver -d
sleep 1
bin/logonserver -d
bin/keeperserver -d
sleep 1
bin/forwardserver -d
