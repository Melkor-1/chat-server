#!/bin/bash

for N in {1..100}
do
	ruby client.rb & 
	echo done
done
wait
