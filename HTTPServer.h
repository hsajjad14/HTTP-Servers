#ifndef HTTPSERVER
#define HTTPSERVER

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
#define TIMEOUT 5 //seconds for timeout
#define MAX_REQUESTS 100 // For testing, set to 3. Normally is 100.

#define REQ_LINE_SIZE 200

extern const char errorWebpage301[];
extern const char errorWebpage304[];
extern const char errorWebpage400[];
extern const char errorWebpage404[];
extern const char errorWebpage414[];
extern const char errorWebpage501[];
extern const char errorWebpage505[];


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

extern void bindAndListen(int port, struct sockaddr_in serv_addr, int * fd_server);
extern int get_msg(int fd_client, char * buf);
extern int find_ext(char * fname);
extern int is_get_req(struct http_request * h);
extern off_t fsize(const char * filename, struct stat * st);
extern int parseRequestInitial(char * initial, struct http_request * h);
extern int parseRequest(char * req, struct http_request * h);
extern void makeServerResponse(struct file * clientFile, int * currentStatusCode, struct http_request * request, int fd_client, int is_simple);

#endif
