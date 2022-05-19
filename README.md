
Servers for the HTTP 1.0 + 1.1 protocols capable of handling GET method requests for files with the following extensions: .html .css .js .txt .jpg. 


## usage
"nc -C host port" adds a crlf on every enter

### compiling
./compile.sh

### compile the simple server
gcc -std=gnu99 -Wall -g SimpleServer.c Helpers.c -lm -o SimpleServer

### compile the persistent server
gcc -std=gnu99 -Wall -g PersistentServer.c Helpers.c -lm -o PersistentServer

### compile the pipelined server
gcc -std=gnu99 -pthread -Wall -g PipelinedServer.c Helpers.c -lm -o PipelinedServer

### running the servers
./SimpleServer 8010 ./
  
### in local directory 
```
nc -C localhost 8010

GET /square.js HTTP/1.0
```
### in 3 deep subdirectory 
```
nc - C localhost 8010

GET /dir1/dir2/dir3/square.js HTTP/1.0
```

### browser
localhost:portnumber/filepath

## Features

### Simple Server
The simple server is a C program that runs indefinitely, with a socket open and listening to connections. Whenever a connection is received a new process is forked, and that new child process handles reading, processing and responding to a single request. After it responds, that client socket is closed, and a new connection is created for every request from any client.  

### Persistence
The persistence server was built over top the simple server. However, what is different is first the client socket that is created has a timeout timer, so if nothing is recieved within the timeout, the client socket (connection) closes. Another difference is that the server keeps track of two variables *keep-alive* and *requests*. *Keep-alive* is a parameter set under the Connection header, set by the client. While the Connection header is set to *keep-alive* and number of requests recieved by the client is less than *MAX_REQUESTS* the connection remains open, otherwise it is closed.

### Pipelining
The pipelined server was built over top the Persistent server. Our aim in the pipelined server was to handle all the responses sent by a single client in parallel. So this means every request a single client sends is recieved, processed, and sent in parallel. To do this, after a client connection is recieved we create *MAX_REQUESTS* threads, each of which runs the *processRequestByThread* function concurrently. 

The *processRequestByThread* function contains everything from the persistence server in reading, processing, and responding to requests. the pipelined server also contains a global *keep_alive* variable accessible by all threads and one which all threads modify (if *keep_alive* is set to 0, the connection closes). As such we needed to create locks (pthread_mutex_t) to avoid synchronization bugs. 

The server has 3 locks: keep_alive_mutex, read_from_client_mutex, and write_to_client_mutex. The keep_alive_mutex is to avoid race conditions in reading and writing to the *keep_alive* variable, the read_from_client_mutex is so only one thread can read a request at a time, and write_to_client_mutex is so only one thread can write to the client at a time. In the *processRequestByThread* function, if the thread enters and sees that *keep_alive* is 0, it exits.

### Conditional Requests

#### **If-Modified-Since**: 
* The **If-Modified-Since** request HTTP header makes the request conditional: the server sends back the requested resource, with a 200 status, only if it has been last modified after the given date. 
* Supported by HTTP 1.0 and HTTP 1.1 so works for SimpleServer.c, PersistentServer.c and PipelinedServer.c
* **Testing**: Enter these commands into a seperate terminal once the server is running (Ex. Port Number: 8010):
    * curl --http1.1 --header 'If-Modified-Since: Sunday, 13-Jan-21 21:49:37 GMT' 0.0.0.0:8010/square.js
        * Response should be returned as the file was modified after the date in the header request
    * curl --http1.1 --header 'If-Modified-Since: Sunday, 13-Jan-23 21:49:37 GMT' 0.0.0.0:8010/square.js
        * Error should be returned as the file was last modified before the date in the header request

#### **If-Unmodified-Since**: 
* The **If-Unmodified-Since** request HTTP header makes the request conditional: the server sends back the requested resource, with a 200 status, only if the last modified date on the file is before given date
* Supported by HTTP 1.1 so works for PersistentServer and PipelinedServer
* **Testing**: Enter these commands into a seperate terminal once the server is running (Ex. Port Number: 8010):
    * curl --http1.1 --header 'If-Unmodified-Since: Sunday, 13-Jan-23 21:49:37 GMT' 0.0.0.0:8010/square.js
        * Response should be returned as the file was modified before the date in the header request
    * curl --http1.1 --header 'If-Unmodified-Since: Sunday, 13-Jan-21 21:49:37 GMT' 0.0.0.0:8010/square.js
        * Error should be returned as the file was last modified after the date in the header request

#### **If-Match**: 
* The **If-Match** request HTTP header makes the request conditional: the server sends back the requested resource, with a 200 status, only if the etag computed of the files matches the one in the request header. If the request header for if-match is *, should pass, or if there are multiple etags listed, if the one computed matches one of them, it should pass.
* Supported by HTTP 1.1 so works for PersistentServer and PipelinedServer
* etag of simple_webpage.html: 347658864-1644290873-115
    * These were the etags I tested on but they’ll probably be different on your computers because the etags are computed using inode number, date last modified and size in bytes, which will be vary from our example. 
    * To figure out what the etags on your computers would be, we left a print statement so that when you send a request, it'll print what the computed etag would be and you can replace that with the ones given in the tests below. 
* **Testing**: Enter these commands into a seperate terminal once the server is running (Ex. Port Number: 8010):
    * curl --http1.1 --header 'If-Match: 347658864-1644290873-115' 0.0.0.0:8010/simple_webpage.html
        * Response should be returned because the etag in the header request matches the computed one of simpe_webpage.html
    * curl --http1.1 --header 'If-Match: 34763358864-1644290873-115' 0.0.0.0:8010/simple_webpage.html
        * Error should be returned because the etag in the header request does not match the computed one of simpe_webpage.html
    * curl --http1.1 --header 'If-Match: *' 0.0.0.0:8010/simple_webpage.html
        * Response should be returned because * matches all etags
    * curl --http1.1 --header 'If-Match: 34763358864-1644290873-115, 347658865-1644290873-1429, 347658864-1644290873-115' 0.0.0.0:8010/simple_webpage.html
        * Response should be returned because one of the etag in the header request (last one) matches the computed one of simpe_webpage.html
    * curl --http1.1 --header 'If-Match: 34763358864-1644290873-115, 347658865-1644290873-1429, 34765883464-1644290873-115' 0.0.0.0:8010/simple_webpage.html
        * Error should be returned because none of the etags in the header request match the computed one of simpe_webpage.html

#### **If-None-Match**
* The **If-Match** request HTTP header makes the request conditional: the server sends back the requested resource, with a 200 status, only if the etag computed of the files doesn’t match the one in the request header. If the request header for if-none-match is *, should fail, or if there are multiple etags listed, if the none of them match the computed etag, it should pass.
* Supported by HTTP 1.1 so works for PersistentServer and PipelinedServer
* **Testing**: Enter these commands into a seperate terminal once the server is running (Ex. Port Number: 8010):
    * curl --http1.1 --header 'If-None-Match: 347658864-1644290873-115' 0.0.0.0:8010/simple_webpage.html
        * Error should be returned because the etag in the header request matchs the computed one of simpe_webpage.html
    * curl --http1.1 --header 'If-None-Match: 34734658864-1644290873-115' 0.0.0.0:8010/simple_webpage.html
        * Reponse should be returned because etags don't match
    * curl --http1.1 --header 'If-None-Match: 3476335348864-1644290873-115, 347658865-1644290873-1429, 34765348864-1644290873-115' 0.0.0.0:8010/simple_webpage.html
        * Response should be returned because none of the etags in the header request match the computed etag
    * curl --http1.1 --header 'If-None-Match: 34763358864-1644290873-115, 347658865-1644290873-1429, 347658864-1644290873-115' 0.0.0.0:8010/simple_webpage.html
        * Error should be returned because the last etag matches the computed etag

## Documentation
### Function Defintion: 
* void **bindAndListen**(int **port**, struct sockaddr_in **serv_addr**, int ***fd_server**){}
    * Sets up a socket to listen for incoming connections and binds it to the given port.
    * Inspired by idea from (CSC209) muffinman.c's bindandlisten.
    * Found in Helpers.c

* int **get_msg**(int **fd_client**, char ***buf**){}
    * Fills <buf> with the contents received from the socket <fd_client>.
    * Returns the number of bytes read into the buffer or -1 if the double CRLF was not received.
    * Found in Helpers.c

* int **find_ext**(char ***fname**){}
    * Given a filename, returns an integer based on the filename extension, returns -1 otherwise. 
    * Found in Helpers.c

* int **is_get_req**(struct http_request ***h**){}
    * Returns 1 if and only if the HTTP method for <h> is GET (not case-sensitive).
    * Found in Helpers.c

* off_t **fsize**(const char ***filename**, struct stat *••st••){}
    * Returns the file size of the file with the given <filename> path.
    * Resources used: https://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c

* int **parseRequestInitial**(char ***initial**, struct http_request ••*h••){}
    * Parses the first line of the GET request
    * Returns an integer 0 on success
    * Found in Helpers.c 

* int **parseRequest**(char ***req**, struct http_request ***h**){} 
    * Parses the client request <req> and populates the HTTP request structure with the parsed request data, header values.
    * Found in Helpers.c

* void **makeServerResponse**(struct file ***clientFile**, int ***currentStatusCode**, struct http_request ***request**, int **fd_client**, int **is_simple**){}
    * Constructs, writes and sends an HTTP response to the client based on the HTTP request, current status, and the <clientFile> requested from the client.
    * Handles conditional requests for HTTP 1.0 and 1.1 Protocols
    * Found in Helpers.c

### Etags: 
-  **Etag = inode number - time of last modification - total size in bytes**
- Used the hint on Piazza for inspiration of how to compute our etags. 
    - For reference, here is Apache's implementation of ETags:
        - http://httpd.apache.org/docs/2.2/mod/core.html#FileETag 
