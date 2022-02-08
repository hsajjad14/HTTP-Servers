#include "HTTPServer.h"
#include <pthread.h>

char webpage[] = "HTTP/1.0 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>SimpleServerWebpage</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1> Hello World!</h1><br>\r\n"
"</center></body></html>\r\n";

struct threadData {
    int *httpCode;
    char *buf;
    int bytes_read;
    char *http_root_path;
    int fd_client;
};

static int fd_server;
// static int request_line_size = 200;

// global accesible variables to all threads
int keep_alive = 1;
pthread_mutex_t keep_alive_mutex;
pthread_mutex_t write_to_client_mutex;
pthread_t thread[MAX_REQUESTS];

// ------------------------------ HELPER FUNCTIONS  --------------------------------

/*
Sets up a socket to listen for incoming connections and binds
it to the given port.

Idea from (CSC209) muffinman.c's bindandlisten.
*/
void bindAndListen(int port, struct sockaddr_in serv_addr) {

    // create a socket
    if ((fd_server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    int on = 1;
    // have OS release server's port as soon as server terminates
    if ((setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, & on, sizeof(int))) == -1) {
        perror("setsockopt");
    }

    // set up <serv_addr> for connecting
    memset( & serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // bind the socket to the port
    if (bind(fd_server, (struct sockaddr * ) & serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(fd_server);
        exit(1);
    }

    // listen; set up a queue to store pending connections
    if (listen(fd_server, 10) < 0) {
        perror("listen");
        close(fd_server);
        exit(1);
    }

}

// https://stackoverflow.com/questions/1726302/remove-spaces-from-a-string-in-c
// void remove_spaces(char* s) {
//     char* d = s;
//     do {
//         while (*d == ' ' || *d == '\n' || *d == '\r') {
//             ++d;
//         }
//     } while (*s++ = *d++);
// }

/*
Fills <buf> with the contents received from the socket <client_fd>.
Returns the number of bytes read into the buffer or -1 if the double CRLF
was not received.
*/
int get_msg(int fd_client, char * buf) {
    printf("\n\n---------------in get_msg\n");

    int bytes_read = 0;
    int not_done_reading = 1;

    char * currentLine = calloc(LINE_SIZE, sizeof(char));
    int emptyLines = 0;
    while (not_done_reading == 1 && bytes_read < MAX_BUF) {
        printf("not done reading = %d\n", not_done_reading);
        bytes_read += read(fd_client, currentLine, LINE_SIZE);
        strncat(buf, currentLine, strlen(currentLine));

        // check if line has \r\n\r\n
        printf("\t---line = \"%s\" - empty lines = %d\n", currentLine, emptyLines);
        if (strcmp(currentLine, "") == 0) {
            printf("empty line\n");
            // if (emptyLines == 1) {
            not_done_reading = 0;
            // }
            // emptyLines++;
        } else if (strstr(currentLine, CRLFCRLF) != NULL) {
            // found the termination of the request message
            printf("empty crlf crlf line\n");
            not_done_reading = 0;
        } else if (strcmp(CRLF, currentLine) == 0) {
            printf(" empty crlf line\n");
            emptyLines++;
            if (emptyLines == 1) {
                not_done_reading = 0;
                printf("emptylines %d\n", emptyLines);
            }


        } else if (strcmp("\n", currentLine) == 0) {
            printf("empty new line\n");
            not_done_reading = 0;
        } else {
            printf("not empty line\n");
            emptyLines = 0;
            not_done_reading = 1;
        }
        memset(currentLine, 0, LINE_SIZE);
    }

    if (not_done_reading == 1) {
        return -1;
    } else {
        return bytes_read;
    }
}

/*
Returns:
  0 if the file ends in .html
  1 if the file ends in .css
  2 if the file ends in .js
  3 if the file ends in .txt
  4 if the file ends in .jpg
Otherwise, returns -1
*/
int find_ext(char * fname) {
    char * dot_ext_ptr = strrchr(fname, '.');
    if (dot_ext_ptr == NULL) {
        return -1;
    } else if (strstr(dot_ext_ptr, ".html") != NULL) {
        return 0;
    } else if (strstr(dot_ext_ptr, ".css") != NULL) {
        return 1;
    } else if (strstr(dot_ext_ptr, ".js") != NULL) {
        return 2;
    } else if (strstr(dot_ext_ptr, ".txt") != NULL) {
        return 3;
    } else if (strstr(dot_ext_ptr, ".jpg") != NULL) {
        return 4;
    } else {
        return -1;
    }
}

/*
Returns 1 if and only if the HTTP method for <h> is GET (not case-sensitive).
*/
int is_get_req(struct http_request * h) {
    if (strncmp(h -> method, "GET", 3) != 0) {
        return 0;
    } else {
        return 1;
    }
}

/*
Returns the file size of the file with the given <filename> path.
Resources used:
https://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c
*/
off_t fsize(const char * filename, struct stat * st) {
    if (stat(filename, st) == 0) {
        return st -> st_size;
    }
    return -1;
}

int parseRequestInitial(char * initial, struct http_request * h) {
    // set up pointers to the components of the initial line
    char * resource_loc_ptr = strchr(initial, ' ') + 1;
    char * ver_ptr = strchr(resource_loc_ptr, ' ') + 1;

    strncpy(h -> method, initial, 3); // assuming this server only handles GET requests, only
    // the first 3 characters matter; this is for sanity test
    strncpy(h -> uri, resource_loc_ptr, ver_ptr - resource_loc_ptr - 1);
    //h->uri[ver_ptr - resource_loc_ptr - 2] = '\0';
    //char *endptr;
    //h->version = strtol(ver_ptr + strlen("HTTP/1."), &endptr, 10);
    strncpy(h -> version, ver_ptr, 8); // assuming this will be of the form "HTTP/1.*"

    return 0;
}

/*
Parses the client request <req> and populates the HTTP request structure
with the parsed request data, header values.
*/
int parseRequest(char * req, struct http_request * h) {
    char * ptr = NULL;
    ptr = strtok(req, CRLF);

    char * colon_pointer = NULL;

    int line_counter = 0;
    int colon_pointer_index = 0;
    char * header = NULL;
    char * header_body = NULL;

    while (ptr != NULL) {
        printf("AA\n");
        if (line_counter > 0) {
            colon_pointer = strstr(ptr, ": ");
            colon_pointer_index = (int)(colon_pointer - ptr);
            header = (char * ) malloc(sizeof(char) * (colon_pointer_index + 1));
            header_body = (char * ) malloc(sizeof(char) * strlen(ptr));
            printf("here?3\n");
            // get header
            for (int i = 0; i < colon_pointer_index; i++) {
                header[i] = ptr[i];
            }

            // get body
            for (int i = colon_pointer_index + 2; i < strlen(ptr); i++) {
                header_body[i - colon_pointer_index - 2] = ptr[i];
            }
            printf("here?\n");
            if (strcmp("If-Modified-Since", header) == 0) {
                // Parse 'If-Modified-Since' header value
                strncpy(h -> if_modified_since, header_body, strlen(header_body));
                printf("\"%s\": \"%s\"\n", header, h -> if_modified_since);
            }
            printf("here?4\n");
            if (strncmp("Connection", header, 10) == 0) {
                // Parse 'Connection' header value
                if (strncmp("keep-alive", header_body, 10) == 0 || strncmp("Keep-Alive", header_body, 10) == 0) {
                    h -> keep_alive = 1;
                } else {
                    // body is close
                    h -> keep_alive = 0;
                }
                printf("\"%s\": \"%d\"\n", header, h -> keep_alive);
            }
            printf("here?5\n");

            // printf("colon at index = %ld, length of line = %ld\n", colon_pointer -
            // ptr, strlen(ptr)); printf("header is \"%s\"\n", header); printf("header
            // body is \"%s\"\n", header_body);

            free(header);
            free(header_body);

        } else {
            printf("here?2\n");
            // first line - parse the initial line
            parseRequestInitial(ptr, h);
        }

        printf("request line = \"%s\"\n\n", ptr);
        ptr = strtok(NULL, "\r\n");
        line_counter++;
    }

    return line_counter;
}

/*
Constructs an HTTP response to the client based on the HTTP request, current status,
and the <clientFile> requested from the client.
*/
void makeServerResponse(struct file * clientFile, char * bufferToSendClient,
    int * currentStatusCode, struct http_request * request) {
    FILE * filePtr;

    if ( * currentStatusCode != 200) {
        // there is a prior error
        memset(bufferToSendClient, 0, MAX_BUF);

        // on errors don't add any headers
        if ( * currentStatusCode == 301) {
            char statusLine[] = "HTTP/1.0 301 Moved Permanently\r\n";
            strncpy(bufferToSendClient, statusLine, strlen(statusLine));
            return;

        } else if ( * currentStatusCode == 400) {
            char statusLine[] = "HTTP/1.0 400 Bad Request\r\n";
            strncpy(bufferToSendClient, statusLine, strlen(statusLine));
            return;

        } else if ( * currentStatusCode == 505) {
            char statusLine[] = "HTTP/1.0 505 HTTP Version Not Supported\r\n";
            strncpy(bufferToSendClient, statusLine, strlen(statusLine));
            return;

        } else {
            char statusLine[] = "HTTP/1.0 400 Bad Request\r\n";
            strncpy(bufferToSendClient, statusLine, strlen(statusLine));
            return;
        }
    }

    filePtr = fopen(clientFile -> filePath, "r");

    // check if file exists:
    if (filePtr == NULL) {
        memset(bufferToSendClient, 0, MAX_BUF);
        char statusLine[] = "HTTP/1.0 404 Not Found\r\n";
        strncpy(bufferToSendClient, statusLine, strlen(statusLine));
        * currentStatusCode = 404;
        return;
    } else { // file found
        // read it
        memset(bufferToSendClient, 0, MAX_BUF);
        char statusLine[] = "HTTP/1.0 200 OK\r\n";
        strncpy(bufferToSendClient, statusLine, strlen(statusLine));

        // headers

        /* ==================================== Handling Conditional Requests ==================================== */
        // If-Modified-Since: <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT format
        // need to create a format for this
        // http://www.qnx.com/developers/docs/qnxcar2/index.jsp?topic=%2Fcom.qnx.doc.neutrino.lib_ref%2Ftopic%2Fs%2Fstrptime.html
        char * date_format = "%a, %d %b %Y %T GMT";
        // collect file meta data in order to find when the file was modified last
        // from https://stackoverflow.com/questions/10446526/get-last-modified-time-of-file-in-linux
        struct stat file_metadata;
        stat(clientFile -> filePath, & file_metadata);
        //fstat(filePtr, &file_metadata);
        //printf("File path: %s \n", clientFile->filePath);
        //printf("Last modified time: %s", ctime(&file_metadata.st_mtime));
        //printf("If modified since time %s \n", clientFile->date_modified_from_header_request);

        // Now we that have the last modified date from the file and the if modified since date from the request header, we can compare the dates.
        int works = 2;
        if (request -> if_modified_since != NULL) {
            struct tm time;
            if (strptime(request -> if_modified_since, date_format, & time) != NULL || strptime(request -> if_modified_since, "%A, %d-%b-%y %T GMT", & time) != NULL || strptime(request -> if_modified_since, "%c", & time) != NULL) {
                //difftime returns difference in seconds between two times.
                //    last modified file            if modified since time
                if (difftime(file_metadata.st_mtime, mktime( & time)) >= 0) {
                    // if we enter here, the file has been modified so we're good
                    works = 1;
                } else {
                    works = 0;
                }
            }
        }
        if (works == 0) {
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
            char header_error[] = "HTTP/1.0 304 Not Modified Since\r\n\r\n";
            strncat(bufferToSendClient, header_error, strlen(header_error));
            return;
        }

        //printf("%s\n", date_format);
        char curr_date[LINE_SIZE/2];
        char header1[LINE_SIZE]; // make line size 200 so curr_date can fit in it.
        time_t sec = time(NULL);
        strftime(curr_date, LINE_SIZE/2, date_format, localtime( & (sec)));
        snprintf(header1, sizeof(header1), "Date: %s \r\n", curr_date);
        //printf("%s \n", header1);
        strncat(bufferToSendClient, header1, strlen(header1));

        // from
        // https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
        int size = (int)((ceil(log10(clientFile -> fileSize)) + 1) * sizeof(char));
        char sizeToStr[size];
        sprintf(sizeToStr, "%d", clientFile -> fileSize);
        char header2_p1[] = "Content-Length: ";
        strncat(bufferToSendClient, header2_p1, strlen(header2_p1));
        // -1 to not write the null terminator as sprintf null terminates
        strncat(bufferToSendClient, sizeToStr, size - 1);

        char carriageReturn[] = "\r\n";
        strncat(bufferToSendClient, carriageReturn, strlen(carriageReturn));


        // adding keep_alive and connection header
        // connection header
        char connectionHeader_p1[] = "Connection: ";
        strncat(bufferToSendClient, connectionHeader_p1, strlen(connectionHeader_p1));
        if (request->keep_alive == 1) {
           char connectionHeader_p2[] = "Keep-Alive";
           strncat(bufferToSendClient, connectionHeader_p2, strlen(connectionHeader_p2));
        } else {
           char connectionHeader_p2[] = "close";
           strncat(bufferToSendClient, connectionHeader_p2, strlen(connectionHeader_p2));
        }
        strncat(bufferToSendClient, carriageReturn, strlen(carriageReturn));

        // keep-alive header
        if (request->keep_alive == 1) {
         char keepAliveHeader[] = "Keep-Alive: timeout=5, max=100\r\n";
         strncat(bufferToSendClient, keepAliveHeader, strlen(keepAliveHeader));
        }


        if (clientFile -> fileType == 0) {
            // html file
            char header3[] = "Content-Type: text/html; charset=UTF-8\r\n\r\n";
            strncat(bufferToSendClient, header3, strlen(header3));

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));
            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                strncat(bufferToSendClient, cToStr, 1);
            }

        } else if (clientFile -> fileType == 1) {
            // css file

            // from
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/css; charset=UTF-8\r\n\r\n";
            strncat(bufferToSendClient, header3, strlen(header3));

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));

            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                strncat(bufferToSendClient, cToStr, 1);
            }

        } else if (clientFile -> fileType == 2) {
            // js file

            // from
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/javascript; charset=UTF-8\r\n\r\n";
            strncat(bufferToSendClient, header3, strlen(header3));

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));

            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                strncat(bufferToSendClient, cToStr, 1);
            }

        } else if (clientFile -> fileType == 3) {
            // txt file
            // from
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
            strncat(bufferToSendClient, header3, strlen(header3));

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));

            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                strncat(bufferToSendClient, cToStr, 1);
            }
        } else if (clientFile -> fileType == 4) {
            // jpg file

            // from
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: image/jpeg\r\n\r\n";
            strncat(bufferToSendClient, header3, strlen(header3));

            printf("in jpg section\n");

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));

            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                strncat(bufferToSendClient, cToStr, 1);
            }

        } else {
            // invalid file type
            memset(bufferToSendClient, 0, MAX_BUF);
            char errorStatusLine[] = "HTTP/1.0 400 Bad Request\r\n";
            strncpy(bufferToSendClient, errorStatusLine, strlen(errorStatusLine));
            * currentStatusCode = 400;
        }
    }
}

void *processRequestByThread(void *t) {
    struct threadData *thread_data = (struct threadData *)t;
    printf("\t\tWhere are we C\n");
    *(thread_data->httpCode) = 200; // default value

    // read in the request from the client

    //int req_bytes_read = get_msg(fd_client, buf);

    if (thread_data->bytes_read < 0) {
        *(thread_data->httpCode) = 400; // something went wrong with the request reading (tentative)
    }

    printf("%s\n", thread_data->buf);

    struct file * clientFile = (struct file * ) malloc(sizeof(struct file));

    struct http_request * request =
        (struct http_request * ) malloc(sizeof(struct http_request));
    request -> version = calloc(9, sizeof(char));
    request -> method = calloc(4, sizeof(char));
    request -> uri = calloc(LINE_SIZE, sizeof(char));
    request -> keep_alive = 1; // default for HTTP/1.1
    request -> accept = calloc(LINE_SIZE, sizeof(char));
    request -> if_match = calloc(LINE_SIZE, sizeof(char));
    request -> if_none_match = calloc(LINE_SIZE, sizeof(char));
    request -> if_modified_since = calloc(LINE_SIZE, sizeof(char));
    request -> if_unmodified_since = calloc(LINE_SIZE, sizeof(char));

    // parse the client request
    parseRequest(thread_data->buf, request);


    // check if connection closed from clients end, if its 0 don't nee to check
    if (keep_alive == 1) {
       pthread_mutex_lock(&keep_alive_mutex);
       keep_alive = request->keep_alive;
       pthread_mutex_unlock(&keep_alive_mutex);
    }

    //TODO: remove
    printf("method parsed out: %s\n", request -> method);
    printf("uri parsed out: %s\n", request -> uri);
    printf("http ver parsed out: %s\n", request -> version);

    // set error codes if request file type, method or versions are invalid
    clientFile -> fileType = find_ext(request -> uri);
    if (clientFile -> fileType < 0 || is_get_req(request) == 0) {
        *(thread_data->httpCode) = 400; // bad request
    }
    // Technically this server follows the HTTP/1.0 protocol, but requests from browser
    // may be an HTTP/1.1 request
    if (strncmp(request -> version, "HTTP/1.0", 8) != 0 && strncmp(request -> version, "HTTP/1.1", 8) != 0) {
        *(thread_data->httpCode) = 505; // HTTP version not supported
    }

    // get full path of requested file
    clientFile -> fileName = malloc(sizeof(char) * strlen(request -> uri));
    strncpy(clientFile -> fileName, request -> uri, strlen(request -> uri));
    int path_len = strlen(thread_data->http_root_path) + strlen(request -> uri) + 1;
    clientFile -> filePath = calloc(path_len, sizeof(char));
    strncpy(clientFile -> filePath, thread_data->http_root_path, strlen(thread_data->http_root_path));
    strncpy(clientFile -> filePath + strlen(thread_data->http_root_path), request -> uri, strlen(request -> uri));
    clientFile -> filePath[strlen(thread_data->http_root_path) + strlen(request -> uri) + 1] = '\0';
    printf("A file in clientFile = \"%s\"\n", clientFile -> filePath);

    printf("err code A = %d\n", *(thread_data->httpCode));
    // clientFile->fileType = 2; // test file types
    // clientFile->fileSize = 2000;

    struct stat * st = (struct stat * ) malloc(sizeof(struct stat));
    clientFile -> fileSize = fsize(clientFile -> filePath, st);
    if (clientFile -> fileSize == -1) {
        *(thread_data->httpCode) = 400;
    }
    free(st);

    printf("err code B = %d\n", *(thread_data->httpCode));

    // construct and send the HTTP response
    printf("file in clientFile = \"%s\"\n", clientFile -> filePath);
    char * bufferToSendClient = (char * ) malloc(sizeof(char) * MAX_BUF); // max http request message len
    makeServerResponse(clientFile, bufferToSendClient, thread_data->httpCode, request);
    printf("buff to send to client = ---------------------\n%s\n", bufferToSendClient);

    pthread_mutex_lock(&write_to_client_mutex);
    write(thread_data->fd_client, bufferToSendClient, strlen(bufferToSendClient));
    pthread_mutex_unlock(&write_to_client_mutex);

    if (request -> keep_alive == 0) {
        // "timeout" because the client does not expect persistent connection
        *(thread_data->httpCode) = 408; //408 Request Timeout
    }
    return NULL;
}

void *doNothing(void *t) {
 return NULL;
}

int main(int argc, char ** argv) {

    // parse in command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: ./persistent port_num http_root_path\n");
        exit(-1);
    }

    char * strtol_buf;
    int port_num = strtol(argv[1], & strtol_buf, 10);
    if (port_num < 0 || port_num > 65536) {
        fprintf(stderr, "Port number out of range.\n");
        exit(-1);
    }
    char * http_root_path = argv[2];

    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_len = sizeof(client_addr);
    int fd_client;

    char buf[MAX_BUF];

    bindAndListen(port_num, server_addr);

    struct timeval * t_val = calloc(1, sizeof(struct timeval));
    t_val -> tv_sec = TIMEOUT;
    t_val -> tv_usec = 0;

    while (1) {
        fd_client = accept(fd_server, (struct sockaddr * ) & client_addr, & sin_len);

        if (fd_client == -1) {
            perror("connection failed --\n");
            continue;
        }

        printf("got client connection ---\n");

        if (!fork()) {
            printf("\t\tWhere are we B\n");
            // child processes returns 0
            close(fd_server);

            // Use SO_RECVTIMEO flag to specify a timeout value for input from socket
            // Sources: https://stackoverflow.com/questions/4181784/how-to-set-socket-timeout-in-c-when-making-multiple-connections
            if (setsockopt(fd_client, SOL_SOCKET, SO_RCVTIMEO, (const char * ) t_val, sizeof(struct timeval)) == -1) {
                perror("setsockopt rcv timeout");
            }
            if (setsockopt(fd_client, SOL_SOCKET, SO_SNDTIMEO, (const char * ) t_val, sizeof(struct timeval)) == -1) {
                perror("setsockopt send timout");
            }

            printf("\t\tWhere are we A\n");
            int * httpCode = (int * ) malloc(sizeof(int));
            * httpCode = 200; // default value

            // stuff for threads
            pthread_attr_t attr;
            int rc;
            void *status;

            pthread_mutex_init(&keep_alive_mutex, NULL);
            pthread_mutex_init(&write_to_client_mutex, NULL);

            /* Initialize and set thread detached attribute */
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

            // Keep looping as long as the client has something to send and
            // the last request was OK
            int bytes_read;
            int requests = 0;
            while (keep_alive == 1 && *httpCode == 200 && (bytes_read = get_msg(fd_client, buf)) > 0 && requests < MAX_REQUESTS) {

                // make threads to do everything inside here
                struct threadData *my_data = (struct threadData *)malloc(sizeof(struct threadData));
                my_data->httpCode = (int*)malloc(sizeof(int));
                my_data->buf = (char*)malloc(sizeof(char)*MAX_BUF);
                my_data->bytes_read = bytes_read;
                my_data->fd_client = fd_client;
                my_data->http_root_path = (char*)malloc(sizeof(char)*strlen(http_root_path));
                strncpy(my_data->http_root_path, http_root_path, strlen(http_root_path));
                strncpy(my_data->buf, buf, MAX_BUF); // how much to copy from buf?

                rc = pthread_create(&thread[requests], &attr, processRequestByThread, (void *)my_data);
                if (rc) {
                    printf("ERROR; return code from pthread_create() is %d\n", rc);
                    exit(-1);
                }

                // make keep_alive a shared variable

                requests++;

            }
            // make the rest
            for (long t = requests; t < MAX_REQUESTS; t++) {
               rc = pthread_create(&thread[t], &attr, doNothing, (void *)t);
            }

            // wait for all threads to end before closing the socket
            for (long t = 0; t < MAX_REQUESTS; t++) {
               rc = pthread_join(thread[t], &status);
               if (rc) {
                   printf("ERROR; return code from pthread_join() is %d\n", rc);
                   exit(-1);
               }
            }

            pthread_mutex_destroy(&keep_alive_mutex);
            pthread_mutex_destroy(&write_to_client_mutex);

            printf("\t\tWhere are we D\n");
            close(fd_client);
            exit(0);
        }

        printf("did we get here\n");
        // parent process
        close(fd_client);
    }

    return 0;

}
