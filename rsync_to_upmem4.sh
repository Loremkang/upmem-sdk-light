#!/bin/bash

while read line
do
	rsync -av --exclude="build/" ./ upmemcloud4:~/upmem_sdk_light/ --delete
done

wait
