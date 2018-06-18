#!/usr/bin/env bash

./daemon &
sleep 1
chmod 666 /tmp/demo.unix.socket

echo "Launching client in new PID and user namespace"
sudo unshare --fork --pid --user --map-root-user ./client

