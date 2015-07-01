#!/bin/sh

set -x

find . -iname "core*" -exec gdb -c {} \; 

for var in $(ps -u $(basename $HOME) | grep multi_licenseplate)
do
	pid=$(echo $var | cut -c1-5)
	pname=$(echo $var | cut -c25-)

	if kill -9 $pid
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

bin/multi_licenseplate_server "127.0.0.1" 19830 9 60 2000 75200
sleep 1
bin/multi_licenseplate_gate "127.0.0.1" 19831 "127.0.0.1" 19830 3
bin/multi_licenseplate_gate "127.0.0.1" 19832 "127.0.0.1" 19830 3
bin/multi_licenseplate_gate "127.0.0.1" 19833 "127.0.0.1" 19830 3
sleep 1
bin/multi_licenseplate_client "127.0.0.1" 19831 3000 3
bin/multi_licenseplate_client "127.0.0.1" 19832 3000 3
bin/multi_licenseplate_client "127.0.0.1" 19833 3000 3

date
tail -f *.log
