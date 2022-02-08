#define _XOPEN_SOURCE // for https://man7.org/linux/man-pages/man3/strptime.3.html

#include <arpa/inet.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>


#define CRLF "\r\n"
#define CRLFCRLF "\r\n\r\n"
#define MAX_BUF 1000000
#define TIMEOUT 10 //seconds for timeout
#define MAX_REQUESTS 3 // for testing set to 3, normally is 100
#define LINE_SIZE 200
// Status codes
#define OK 200
#define MOVED_PERMANENTLY 301
#define BAD_REQUEST 400
#define HTTP_VER_NOT_SUPPORTED 505

struct file {
    int fileType;
    // 0 = .html
    // 1 = .css
    // 2 = .js
    // 3 = .txt
    // 4 = .jpg
	// -1 = invalid or unknown extension
    int fileSize; // bytes in the file
    char *fileName; // file name
    char *filePath; // file path
};

struct http_request {
	char *method;
	char *uri;
	char *version;
	// header values
	char *accept;
	int keep_alive;	// 1 if true, 0 if false (i.e. Connection: close)
	char *if_match;
	char *if_none_match;
	char *if_modified_since;
	char *if_unmodified_since;
};
