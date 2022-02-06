#include "HTTPServer.h"


char webpage[] = "HTTP/1.0 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>SimpleServerWebpage</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1> Hello World!</h1><br>\r\n"
"</center></body></html>\r\n";

static int fd_server;
static int request_line_size = 200;

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

/*
Fills <buf> with the contents received from the socket <client_fd>.
Returns the number of bytes read into the buffer or -1 if the double CRLF
was not received.
*/
int get_msg(int fd_client, char * buf) {

    int bytes_read = 0;
    int not_done_reading = 1;

    char * currentLine = calloc(request_line_size, sizeof(char));
    int emptyLines = 0;
    while (not_done_reading == 1 && bytes_read < MAX_BUF) {
        bytes_read += read(fd_client, currentLine, request_line_size);
        strncat(buf, currentLine, strlen(currentLine));

        // check if line has \r\n\r\n
        printf("\t---line = \"%s\" - empty lines = %d\n", currentLine, emptyLines);
        if (strstr(currentLine, "") != NULL) {
            // printf(" something\n");
        }

        if (strstr(currentLine, CRLFCRLF) != NULL) {
            // found the termination of the request message
            not_done_reading = 0;
        } else if (strcmp(CRLF, currentLine) == 0) {
            emptyLines++;
            if (emptyLines == 1) {
                not_done_reading = 0;
                printf("emptylines %d\n", emptyLines);
            }
        } else {
            emptyLines = 0;
            not_done_reading = 1;
        }
        memset(currentLine, 0, request_line_size);
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
        if (line_counter > 0) {
            colon_pointer = strstr(ptr, ": ");
            colon_pointer_index = (int)(colon_pointer - ptr);
            header = (char * ) malloc(sizeof(char) * (colon_pointer_index + 1));
            header_body = (char * ) malloc(sizeof(char) * strlen(ptr));

            // get header
            for (int i = 0; i < colon_pointer_index; i++) {
                header[i] = ptr[i];
            }

            // get body
            for (int i = colon_pointer_index + 2; i < strlen(ptr); i++) {
                header_body[i - colon_pointer_index - 2] = ptr[i];
            }

            // header we are tracking for SimpleServer
            // If-Modified-Since
            if (strcmp("If-Modified-Since", header) == 0) {
                strncpy(h -> if_modified_since, header_body, strlen(header_body));
                printf("\"%s\": \"%s\"\n", header, h -> if_modified_since);
            }

            // printf("colon at index = %ld, length of line = %ld\n", colon_pointer -
            // ptr, strlen(ptr)); printf("header is \"%s\"\n", header); printf("header
            // body is \"%s\"\n", header_body);

            free(header);
            free(header_body);

        } else {
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
    int * currentStatusCode, struct http_request * request, int fd_client) {
    FILE * filePtr;

    if ( * currentStatusCode != 200) {
        // there is a prior error
        memset(bufferToSendClient, 0, MAX_BUF);

        // on errors don't add any headers
        if ( * currentStatusCode == 301) {
            char statusLine[] = "HTTP/1.0 301 Moved Permanently\r\n";
			write(fd_client, statusLine, strlen(statusLine));
            //strncpy(bufferToSendClient, statusLine, strlen(statusLine));
            return;

        } else if ( * currentStatusCode == 400) {
            char statusLine[] = "HTTP/1.0 400 Bad Request\r\n";
			write(fd_client, statusLine, strlen(statusLine));
            //strncpy(bufferToSendClient, statusLine, strlen(statusLine));
            return;

        } else if ( * currentStatusCode == 505) {
            char statusLine[] = "HTTP/1.0 505 HTTP Version Not Supported\r\n";
			write(fd_client, statusLine, strlen(statusLine));
            //strncpy(bufferToSendClient, statusLine, strlen(statusLine));
            return;

        } else {
            char statusLine[] = "HTTP/1.0 400 Bad Request\r\n";
			write(fd_client, statusLine, strlen(statusLine));
            //strncpy(bufferToSendClient, statusLine, strlen(statusLine));
            return;
        }
    }

    filePtr = fopen(clientFile -> filePath, "r");

    // check if file exists:
    if (filePtr == NULL) {
        memset(bufferToSendClient, 0, MAX_BUF);
        char statusLine[] = "HTTP/1.0 404 Not Found\r\n";
		write(fd_client, statusLine, strlen(statusLine));
        //strncpy(bufferToSendClient, statusLine, strlen(statusLine));
        * currentStatusCode = 404;
        return;
    } else { // file found
        // read it
        memset(bufferToSendClient, 0, MAX_BUF);

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
			write(fd_client, header_error, strlen(header_error));
            //strncat(bufferToSendClient, header_error, strlen(header_error));
            return;
        }
        char statusLine[] = "HTTP/1.0 200 OK\r\n";
        //strncpy(bufferToSendClient, statusLine, strlen(statusLine));
		write(fd_client, statusLine, strlen(statusLine));
        //printf("%s\n", date_format);
        char curr_date[100];
        char header1[200]; // make line size 200 so curr_date can fit in it.
        time_t sec = time(NULL);
        strftime(curr_date, 100, date_format, localtime( & (sec)));
        snprintf(header1, sizeof(header1), "Date: %s \r\n", curr_date);
        //printf("%s \n", header1);
        //strncat(bufferToSendClient, header1, strlen(header1));
		write(fd_client, header1, strlen(header1));

        // from
        // https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
        int size = (int)((ceil(log10(clientFile -> fileSize)) + 1) * sizeof(char));
        char sizeToStr[size];
        sprintf(sizeToStr, "%d", clientFile -> fileSize);
        char header2_p1[] = "Content-Length: ";
        //strncat(bufferToSendClient, header2_p1, strlen(header2_p1));
		write(fd_client, header2_p1, strlen(header2_p1));
        // -1 to not write the null terminator as sprintf null terminates
        //strncat(bufferToSendClient, sizeToStr, size - 1);
		write(fd_client, sizeToStr, size - 1);

        char carriageReturn[] = "\r\n";
		write(fd_client, carriageReturn, strlen(carriageReturn));
        //strncat(bufferToSendClient, carriageReturn, strlen(carriageReturn));

        if (clientFile -> fileType == 0) {
            // html file
            char header3[] = "Content-Type: text/html; charset=UTF-8\r\n\r\n";
            //strncat(bufferToSendClient, header3, strlen(header3));
			write(fd_client, header3, strlen(header3));

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));
            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                //strncat(bufferToSendClient, cToStr, 1);
				write(fd_client, cToStr, 1);
            }

        } else if (clientFile -> fileType == 1) {
            // css file

            // from
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/css; charset=UTF-8\r\n\r\n";
            //strncat(bufferToSendClient, header3, strlen(header3));
			write(fd_client, header3, strlen(header3));

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));

            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                //strncat(bufferToSendClient, cToStr, 1);
				write(fd_client, cToStr, 1);
            }

        } else if (clientFile -> fileType == 2) {
            // js file

            // from
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/javascript; charset=UTF-8\r\n\r\n";
            //strncat(bufferToSendClient, header3, strlen(header3));
			write(fd_client, header3, strlen(header3));

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));

            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                //strncat(bufferToSendClient, cToStr, 1);
				write(fd_client, cToStr, 1);
            }

        } else if (clientFile -> fileType == 3) {
            // txt file
            // from
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
            //strncat(bufferToSendClient, header3, strlen(header3));
			write(fd_client, header3, strlen(header3));
            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));

            int c;
            // read from file
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                //strncat(bufferToSendClient, cToStr, 1);
				write(fd_client, cToStr, 1);
            }
        } else if (clientFile -> fileType == 4) {
            // jpg file

            // from
            // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: image/jpeg\r\n\r\n";
            //strncat(bufferToSendClient, header3, strlen(header3));
			write(fd_client, header3, strlen(header3));

            printf("in jpg section\n");

            // copy file body
            // strncat(bufferToSendClient, tempHtmlFile, strlen(tempHtmlFile));

            int c;
            // read from file
            while ((c = fgetc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                //strncat(bufferToSendClient, cToStr, 1);
				write(fd_client, cToStr, 1);
            }

        } else {
            // invalid file type
            memset(bufferToSendClient, 0, MAX_BUF);
            char errorStatusLine[] = "HTTP/1.0 400 Bad Request\r\n";
            //strncpy(bufferToSendClient, errorStatusLine, strlen(errorStatusLine));
            write(fd_client, errorStatusLine, strlen(errorStatusLine));
			* currentStatusCode = 400;
        }
    }
}

int main(int argc, char ** argv) {

    // parse in command line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: ./simpleserver port_num http_root_path\n");
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

    while (1) {
        fd_client = accept(fd_server, (struct sockaddr * ) & client_addr, & sin_len);

        if (fd_client == -1) {
            perror("connection failed --\n");
            continue;
        }

        printf("got client connection ---\n");

        if (!fork()) {

            // child processes returns 0
            close(fd_server);

            int * httpCode = (int * ) malloc(sizeof(int));
            * httpCode = 200; // default value

            // read in the request from the client
            memset(buf, 0, MAX_BUF);
            int req_bytes_read = get_msg(fd_client, buf);

            if (req_bytes_read < 0) {
                * httpCode = 400; // something went wrong with the request reading (tentative)
            }

            printf("%s\n", buf);

            struct file * clientFile = (struct file * ) malloc(sizeof(struct file));

            struct http_request * request =
                (struct http_request * ) malloc(sizeof(struct http_request));
            request -> version = calloc(9, sizeof(char));
            request -> method = calloc(4, sizeof(char));
            request -> uri = calloc(request_line_size, sizeof(char));
            request -> accept = calloc(request_line_size, sizeof(char));
            request -> keep_alive = 0; // assume a closed connection for HTTP/1.0 SimpleServer
            request -> if_match = calloc(request_line_size, sizeof(char));
            request -> if_none_match = calloc(request_line_size, sizeof(char));
            request -> if_modified_since = calloc(request_line_size, sizeof(char));
            request -> if_unmodified_since = calloc(request_line_size, sizeof(char));

            // parse the client request
            parseRequest(buf, request);
            //TODO: remove
            printf("method parsed out: %s\n", request -> method);
            printf("uri parsed out: %s\n", request -> uri);
            printf("http ver parsed out: %s\n", request -> version);

            // set error codes if request file type, method or versions are invalid
            clientFile -> fileType = find_ext(request -> uri);
            if (clientFile -> fileType < 0 || is_get_req(request) == 0) {
                * httpCode = 400; // bad request
            }
            // Technically this server follows the HTTP/1.0 protocol, but requests from browser
            // may be an HTTP/1.1 request
            if (strncmp(request -> version, "HTTP/1.0", 8) != 0 && strncmp(request -> version, "HTTP/1.1", 8) != 0) {
				//printf("Got here: %d %d\n", strncmp(request -> version, "HTTP/1.0", 8), strncmp(request -> version, "HTTP/1.1", 8));
				* httpCode = 505; // HTTP version not supported
            }

            // get full path of requested file
            clientFile -> fileName = malloc(sizeof(char) * strlen(request -> uri));
            strncpy(clientFile -> fileName, request -> uri, strlen(request -> uri));
            int path_len = strlen(http_root_path) + strlen(request -> uri) + 1;
            clientFile -> filePath = calloc(path_len, sizeof(char));
            strncpy(clientFile -> filePath, http_root_path, strlen(http_root_path));
            strncpy(clientFile -> filePath + strlen(http_root_path), request -> uri, strlen(request -> uri));
            clientFile -> filePath[strlen(http_root_path) + strlen(request -> uri) + 1] = '\0';
            printf("A file in clientFile = \"%s\"\n", clientFile -> filePath);

            printf("err code A = %d\n", * httpCode);
            // clientFile->fileType = 2; // test file types
            // clientFile->fileSize = 2000;

            struct stat * st = (struct stat * ) malloc(sizeof(struct stat));
            clientFile -> fileSize = fsize(clientFile -> filePath, st);
            if (clientFile -> fileSize == -1) {
                * httpCode = 400;
            }
            free(st);

            printf("err code B = %d\n", * httpCode);

            // construct and send the HTTP response
            printf("file in clientFile = \"%s\"\n", clientFile -> filePath);
            char * bufferToSendClient = (char * ) malloc(sizeof(char) * MAX_BUF); // max http request message len
            makeServerResponse(clientFile, bufferToSendClient, httpCode, request, fd_client);
            printf("buff to send to client = ---------------------\n%s\n", bufferToSendClient);

            //write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            close(fd_client);
            printf("closing client connection--\n");

            exit(0);
        }

        // parent process
        close(fd_client);
    }

    return 0;

}
