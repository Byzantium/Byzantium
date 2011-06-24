#!/bin/bash
for lib in lib/*.jar
do
	CLASSPATH=`pwd`/$lib:$CLASSPATH
done
export CLASSPATH

