#!/bin/bash
cd fs
for i in {1..20}
do 
    touch "./$i.txt"; 
    mkdir $i
    cd $i
    for j in {1..200}
    do 
        touch "./$j.txt"; 
    done;
    cd ..
done;
