# Ex4 - Threaded TCP Echo Server

## Overview
This project implements a multithreaded TCP Echo Server and a multithreaded Client load simulator.
The server converts received lowercase letters to UPPERCASE and echoes the processed data back.

## Build
```bash
make

Run Server
./server 5555



Run Client (at least 5 threads)
./client 5555 5
