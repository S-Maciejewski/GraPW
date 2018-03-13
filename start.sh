#!/bin/bash

gcc serwer.c -Wall -o serwer
gcc klient.c -lncurses -Wall -o klient

./clear.sh

gnome-terminal --geometry=101x29+0+0 -e 'bash -c "./serwer"'
gnome-terminal --geometry=101x29-0+0 -e 'bash -c "./klient 1"'
gnome-terminal --geometry=101x29+0-0 -e 'bash -c "./klient 2"'
gnome-terminal --geometry=101x29-0-0 -e 'bash -c "./klient 3"'

exit 0
