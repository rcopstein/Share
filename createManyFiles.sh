#!/bin/bash

for i in {0001..125}
do
	mkdir ${i}

	for j in ` seq 1 3 150 `
	do
                echo ${j}
		touch ${i}/${j}.txt
	done
done
