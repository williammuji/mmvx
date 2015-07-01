#!/bin/sh

set -x

for var in $(ps -u $(basename $HOME) | grep multi_licenseplate)
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

cond=$(ps -u $(basename $HOME) | grep multi_licenseplate | wc -l)
while [ $cond -gt 0 ]
do
	sleep 1
	cond=$(ps -u $(basename $HOME) | grep multi_licenseplate | wc -l)
	echo "multi_licenseplate* remain:$cond"
done
