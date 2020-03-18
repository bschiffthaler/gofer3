#!/bin/bash

set -eux

CONF=$1

WAIT=30

echo "Starting gofer3 in $WAIT secs using config: $CONF"

sleep $WAIT

# echo "Updating DB"
/usr/local/bin/updateDB -e TokuDB $CONF

echo "Starting REST server"
/usr/local/bin/gofer3 $CONF
