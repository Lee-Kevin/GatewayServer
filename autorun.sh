#!/bin/sh

chmod +x /apclient
while true
do
	 
	procnum=`ps |grep apclient |grep -v grep|wc -l`
	echo The procnum is $procnum
	if [ $procnum -eq 0 ]; then
		/apclient
	fi

	sleep 60
done
