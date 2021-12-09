# USB-DUXfast TCP Server in C

## Compilation
build the duxToFile or duxToTcpServer like this
````
gcc -o duxToFile duxToFile.o -lcomedi -lm
````

## System Setup

duxToTcpServer.c provides a TCP Server that runs on a linux machine 

if the raw data is required on a windows machine, the attached JavaDUX.m class and the MyUSBDUX.jar can be used in MATLAB
to interface with the TCP Server