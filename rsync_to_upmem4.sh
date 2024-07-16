#!/bin/bash

while read line
do
	rsync -av ./ upmemcloud4:~/upmem_sdk_light/
done

wait
