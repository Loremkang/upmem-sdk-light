#!/bin/bash

while read line
do
	rsync -av --exclude="build/" ./ upmemcloud9:~/upmem_sdk_light/ --delete
done

wait
