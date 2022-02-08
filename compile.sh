#!/bin/bash

gcc -Wall -g SimpleServer.c Helpers.c -lm -o SimpleServer
gcc -Wall -g PersistentServer.c Helpers.c -lm -o PersistentServer