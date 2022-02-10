# csc358-a1
## usage
"nc -C host port" adds a crlf on every enter

### run the server
./SimpleServer 8010 ./
  
### in local directory 
nc -C localhost 8010

GET /square.js HTTP/1.0

### in 3 deep subdirectory 
nc - C localhost 8010

GET /dir1/dir2/dir3/square.js HTTP/1.0

### browser
localhost:portnumber/filepath


### to run pipelined server
gcc -pthread -Wall -g PipelinedServer.c -lm -o PipelinedServer
