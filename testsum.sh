#!/bin/bash

#########################################
#	Progetto SOL AA 2018/2019   	#
#	Alessio Galatolo            	#
#########################################

if ! [ -e  testout.log ] ; then
	echo "File 'testout.log' not found"
	exit 1
fi
echo "Elaborating test results"
raw_log=$(cut -d: -f2 testout.log) ;
tests=0 #number of clients launched
success=0 #number of successes
fails=0 #number of fails
client_failed=(0) #array of the clients who failed the test
c_index=0 #index of the array
test_failed=(0) #array containing the types of test failed
t_index=0 #index of the array
test_test=0 #stores the last test_type done
i=0
for number in $raw_log; do
    #every 4 numbers we get <test_type, #tests, #successes, #fails>
    case $i in
        (0) test_type=$number;;
        (1) tests=$(($tests+1));;
        (2) success=$(($success+$number));;
        (3) if [ "$number" -ne "0" ]; then
                client_failed[$c_index]=$tests
                c_index=$(($c_index+1))
                test_failed[$t_index]=$test_type
                t_index=$(($t_index+1))
                fails=$(($fails+$number))
            fi;;
    esac
    i=$((($i+1)%4));
done
echo Number of tests: $tests
echo Number of successes: $success
echo Number of fails: $fails

#if there are any failures, print #n client and #test
if [ "$fails" -ne "0" ]; then
    i=0
    echo "Clients who failed the test with the number of the test failed: "
    for client in ${client_failed[*]}; do
        echo "client " $client " failed test n:" ${test_failed[$i]}
        i=$(($i+1));
    done
fi

killall -SIGUSR1 object_store
killall -SIGINT object_store
