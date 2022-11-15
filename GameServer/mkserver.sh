#!/bin/bash
if [ $# -lt 3 ]
then
        echo "<Usage:  $0  App  Server  Servant>"
        exit 0
fi

mkdir src
cp -av /root/src/TarsCpp/servant/script/cmake_demo/src/* src/
cd src

APP=$1
SERVER=$2
SERVANT=$3

SRC_FILE="DemoServer.h DemoServer.cpp DemoServantImp.h DemoServantImp.cpp DemoServant.tars CMakeLists.txt"

for FILE in $SRC_FILE
do
        cat $FILE | sed "s/DemoServer/$SERVER/g" > $FILE.tmp
        mv $FILE.tmp $FILE

        cat $FILE | sed "s/DemoApp/$APP/g" > $FILE.tmp
        mv $FILE.tmp $FILE

        cat $FILE | sed "s/DemoServant/$SERVANT/g" > $FILE.tmp
        mv $FILE.tmp $FILE
done

mv DemoServer.h ${SERVER}.h
mv DemoServer.cpp ${SERVER}.cpp
mv DemoServantImp.h ${SERVANT}Imp.h
mv DemoServantImp.cpp ${SERVANT}Imp.cpp
mv DemoServant.tars ${SERVANT}.tars

cd -