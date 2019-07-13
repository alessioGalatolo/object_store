#!/bin/bash

#########################################
#	Progetto SOL AA 2018/2019   	#
#	Alessio Galatolo            	#
#########################################

echo "Test 1 started"
rm -f testout.log
for ((i=1; i<=50; i++)); do
	./client_test "name$i" 1 &>> testout.log &
done
echo "Waiting for every client 1 to finish"
wait
echo "Test 2 started"
for ((i=1; i<=30; i++)); do
	./client_test "name$i" 2 &>> testout.log &
done
echo "Test 3 started"
for ((i=31; i<=50; i++)); do
	./client_test "name$i" 3 &>> testout.log &
done
echo "Waiting for every client 2 and 3 to finish"
wait
echo "Test done"
