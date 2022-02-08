#!/bin/bash

gcc -std=gnu99 -Wall -g SimpleServer.c Helpers.c -lm -o SimpleServer
gcc -std=gnu99 -Wall -g PersistentServer.c Helpers.c -lm -o PersistentServer