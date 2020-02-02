#!/bin/bash

echo "run-quick-start.sh"

WORKDIR=$(pwd)

echo ${WORKDIR}

killall -9 QuickStartDemo ProxyServer

echo "start server: ${WORKDIR}/../bin/QuickStartDemo --config=${WORKDIR}/../../examples/QuickStartDemo/HelloServer/Server/config.conf &"

${WORKDIR}/../bin/QuickStartDemo --config=${WORKDIR}/../../examples/QuickStartDemo/HelloServer/Server/config.conf &
${WORKDIR}/../bin/ProxyServer --config=${WORKDIR}/../../examples/QuickStartDemo/ProxyServer/Server/config.conf &

sleep 3

echo "client: ${WORKDIR}/../bin/QuickStartDemoClient"

${WORKDIR}/../bin/QuickStartDemoClient --count=100000 --call=sync --thread=2 --buffersize=100 --netthread=2

${WORKDIR}/../bin/QuickStartDemoClient --count=100000 --call=async --thread=2 --buffersize=100 --netthread=2

${WORKDIR}/../bin/QuickStartDemoClient --count=100000 --call=synctup --thread=2 --buffersize=100 --netthread=2

${WORKDIR}/../bin/QuickStartDemoClient --count=100000 --call=asynctup --thread=2 --buffersize=100 --netthread=2

echo "client: ${WORKDIR}/../bin/ProxyServerClient"

${WORKDIR}/../bin/ProxyServerClient

sleep 2

killall -9 ProxyServer QuickStartDemo


