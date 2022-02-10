#define _XOPEN_SOURCE
#define _GNU_SOURCE

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
    int * httpCode;
    char * buf;
    char * http_root_path;
    int fd_client;
};

// static int request_line_size = 200;

// global accesible variables to all threads
int keep_alive = 1;
pthread_mutex_t keep_alive_mutex;
pthread_mutex_t read_from_client_mutex;
pthread_mutex_t write_to_client_mutex;
pthread_t thread[MAX_REQUESTS];

// ------------------------------ HELPER FUNCTIONS  --------------------------------
// Error webpages (for requests from a browser)
char * errorWebpage301Pipelined =
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>HTTP Error Message</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1>301: Moved Permanently</h1><br><p>The requested file has moved to a new location.</p>\r\n"
"</center></body></html>\r\n";

char * errorWebpage304Pipelined =
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>HTTP Error Message</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1>304: Not Modified</h1><br><p>The URL has not been modified since the specified date.</p>\r\n"
"</center></body></html>\r\n";

char * errorWebpage400Pipelined =
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>HTTP Error Message</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1>400: Bad Request</h1><br><p>The server did not understand the request.</p>\r\n"
"</center></body></html>\r\n";

char * errorWebpage404Pipelined =
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>HTTP Error Message</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1>404: Not Found</h1><br><p>The server could not find the requested file.</p>\r\n"
"</center></body></html>\r\n";

char * errorWebpage414Pipelined =
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>HTTP Error Message</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1>414: Request URL Too Long</h1><br><p>The URL in the request is too long.</p>\r\n"
"</center></body></html>\r\n";

char * errorWebpage501Pipelined =
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>HTTP Error Message</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1>501: Not Implemented</h1><br><p>The server does not support the requested functionality.</p>\r\n"
"</center></body></html>\r\n";

char * errorWebpage505Pipelined =
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html>\r\n"
"<html><head><title>HTTP Error Message</title>\r\n"
"<style>body {background-color: #FFFF00}</style></head>\r\n"
"<body><center><h1>505: Version Not Supported</h1><br><p>The server only supports HTTP/1.0 and HTTP/1.1</p>\r\n"
"</center></body></html>\r\n";

/*
Constructs an HTTP response to the client based on the HTTP request, current status,
and the <clientFile> requested from the client.
*/
void makeServerResponsePipelinedVesion(struct file * clientFile, char * bufferToSendClient,
    int * currentStatusCode, struct http_request * request, int fd_client, int is_simple) {
    FILE * filePtr;

    memset(bufferToSendClient, 0, MAX_BUF);
    if ((strcasecmp("HTTP/1.1", request->version) == 0) && (is_simple == 0)) {
        strncpy(bufferToSendClient, "HTTP/1.1 ", strlen("HTTP/1.1 "));
    } else {
        // Default version of this server is HTTP/1.0
        strncpy(bufferToSendClient, "HTTP/1.0 ", strlen("HTTP/1.0 "));
    }

    // Handle erroroneous requests first; on error, don't add extra headers.
    if ( * currentStatusCode != 200) {
        if ( * currentStatusCode == 301) {
            char statusLine[] = "301 Moved Permanently\r\n";
			         strncat(bufferToSendClient, statusLine, strlen(statusLine));
            strncat(bufferToSendClient, errorWebpage301Pipelined, strlen(errorWebpage301Pipelined));
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);
            return;
        } else if ( * currentStatusCode == 400) {
            char statusLine[] = "400 Bad Request\r\n";
			         strncat(bufferToSendClient, statusLine, strlen(statusLine));
            strncat(bufferToSendClient, errorWebpage400Pipelined, strlen(errorWebpage400Pipelined));
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);
            return;
        } else if ( * currentStatusCode == 404) {
            char statusLine[] = "404 Not Found\r\n";
			         strncat(bufferToSendClient, statusLine, strlen(statusLine));
            strncat(bufferToSendClient, errorWebpage404Pipelined, strlen(errorWebpage404Pipelined));
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);
            return;
        } else if ( * currentStatusCode == 505) {
            char statusLine[] = "501 Not Implemented\r\n";
			         strncat(bufferToSendClient, statusLine, strlen(statusLine));
            strncat(bufferToSendClient, errorWebpage501Pipelined, strlen(errorWebpage501Pipelined));
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);
            return;
        } else if ( * currentStatusCode == 501) {
            char statusLine[] = "505 HTTP Version Not Supported\r\n";
			         strncat(bufferToSendClient, statusLine, strlen(statusLine));
            strncat(bufferToSendClient, errorWebpage505Pipelined, strlen(errorWebpage505Pipelined));
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);
            return;
        } else {
            char statusLine[] = "400 Bad Request\r\n";
			         strncat(bufferToSendClient, statusLine, strlen(statusLine));
            strncat(bufferToSendClient, errorWebpage400Pipelined, strlen(errorWebpage400Pipelined));
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);
            return;
        }
    }

    filePtr = fopen(clientFile -> filePath, "r");

    // Check if file exists
    if (filePtr == NULL) {
        char statusLine[] = "404 Not Found\r\n";
		      strncat(bufferToSendClient, statusLine, strlen(statusLine));
        strncat(bufferToSendClient, errorWebpage404Pipelined, strlen(errorWebpage404Pipelined));
        pthread_mutex_lock( & write_to_client_mutex);
        write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
        pthread_mutex_unlock( & write_to_client_mutex);
        * currentStatusCode = 404;
        return;
    } else {
        // Add headers, then add file contents

        /* ==================================== Handling Conditional Requests ==================================== */
        // If-Modified-Since: <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT format
        // need to create a format for this
        // http://www.qnx.com/developers/docs/qnxcar2/index.jsp?topic=%2Fcom.qnx.doc.neutrino.lib_ref%2Ftopic%2Fs%2Fstrptime.html
        char * date_format = "%a, %d %b %Y %T GMT";
        // collect file meta data in order to find when the file was modified last
        // from https://stackoverflow.com/questions/10446526/get-last-modified-time-of-file-in-linux
        struct stat file_metadata;
        stat(clientFile -> filePath, & file_metadata);

        // Create etag that will be used in later conditional checks (based on Piazza post recommendation)
        // For reference, here is Apache's implementation of ETags:
        // http://httpd.apache.org/docs/2.2/mod/core.html#FileETag
        // etag construction: inode number - time of last modification - total size, in bytes
        char *etag;
        asprintf(&etag, "%ld-%ld-%lld", (long)file_metadata.st_ino, (long)file_metadata.st_mtime, (long long)file_metadata.st_size);

        if (strcasecmp("HTTP/1.1", request->version) == 0) {
            /* ==================================== if-modified-since ==================================== */
            int works = 2;
            if (request -> if_modified_since != NULL) {
                struct tm time;
                // Converting the request time from the header request to correct format and storing it in time
                if (strptime(request -> if_modified_since, date_format, & time) != NULL || strptime(request -> if_modified_since, "%A, %d-%b-%y %T GMT", & time) != NULL || strptime(request -> if_modified_since, "%c", & time) != NULL) {
                    // Now we that have the last modified date from the file and the if modified since date from the request header, we can compare the dates.
                    // difftime returns difference in seconds between two times.
                    //    last modified file            if modified since time
                    if (difftime(file_metadata.st_mtime, mktime( & time)) >= 0) {
                        // If we enter here, the file has been modified so we're good
                        works = 1;
                    } else {
                        works = 0;
                    }
                }
            }
            if (works == 0) {
                // Did not pass the If-Modified-Since condition. Send error message to client.
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "304 Not Modified Since\r\n\r\n";
                strncat(bufferToSendClient, header_error, strlen(header_error));
                pthread_mutex_lock( & write_to_client_mutex);
                write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
                pthread_mutex_unlock( & write_to_client_mutex);
                return;
            }
            /* ==================================== if-unmodified-since ==================================== */
            if (request->if_modified_since != NULL) {
                struct tm time;
                // Converting the request time from the header request to correct format and storing it in time
                if (strptime(request->if_unmodified_since, date_format, &time) != NULL || strptime(request->if_unmodified_since, "%A, %d-%b-%y %T GMT", &time) != NULL || strptime(request->if_unmodified_since, "%c", &time) != NULL) {
                    // Now we that have the last modified date from the file and the if modified since date from the request header, we can compare the dates.
                    // difftime returns difference in seconds between two times.
                    //     last modified file            if modified since time
                    if (difftime(file_metadata.st_mtime, mktime(&time)) >= 0) {
                        // If we enter here, the file has been modified after the date in the header so it will fail
                        works = 0;
                    }
                    else {
                        works = 1;
                        //printf("passed if-unmodified \n");
                    }
                }
            }
            if (works == 0) {
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "412 Precondition Failed\r\n\r\n";
                //write(fd_client, header_error, strlen(header_error));
                strncat(bufferToSendClient, header_error, strlen(header_error));
                pthread_mutex_lock( & write_to_client_mutex);
                write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
                pthread_mutex_unlock( & write_to_client_mutex);
                return;
            }
            /* ==================================== if-none-match ==================================== */
            works = 3;
            if (request->if_none_match != NULL) {
                if (strcmp(request->if_none_match, "*") == 0)
                {
                    // etags match, it's a fail ** ask professor about this **
                    works = 0;
                }
                char *index;
                index = strtok(request->if_none_match, ", ");
                //printf("index: %s \n", index);
                while (index != NULL) {
                    // checking if the etag starts with W/ and if it does, increment the index and perform a strong
                    // validation on the rest of the string
                    if (strncmp(index, "W/", 2) == 0) {
                        index = index + 2;
                    }
                    if (strcmp(index, etag) == 0) {
                        // etags match, it's a fail
                        works = 0;
                    }
                    index = strtok(NULL, ", ");
                }
                // etags dont match

                if (works == 3) {
                    // etags don't match, it's a pass
                    works = 1;
                };
            }
            if (works == 0) {
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "412 Precondition Failed\r\n\r\n";
                strncat(bufferToSendClient, header_error, strlen(header_error));
                pthread_mutex_lock( & write_to_client_mutex);
                write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
                pthread_mutex_unlock( & write_to_client_mutex);
                return;
            }

            /* ==================================== if-match ==================================== */
            if (request->if_match != NULL && works != 1) {
                if (strcmp(request->if_match, "*") == 0) {
                    // etag matches
                    works = 1;
                }
                char *index;
                index = strtok(request->if_match, ", ");

                while (index != NULL) {
                    // Checking if the etag starts with W/ and if it does, increment the index and perform a strong
                    // validation on the rest of the string
                    if (strncmp(index, "W/", 2) == 0) {
                        index = index + 2;
                    }
                    if (strcmp(index, etag) == 0) {
                        // etag matches
                        works = 1;
                    }
                    index = strtok(NULL, ", ");
                }
                // etags dont match
                if (works == 3) {
                    works = 0;
                }
            }
            if (works == 0) {
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "412 Precondition Failed\r\n\r\n";
                strncat(bufferToSendClient, header_error, strlen(header_error));
                pthread_mutex_lock( & write_to_client_mutex);
                write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
                pthread_mutex_unlock( & write_to_client_mutex);
                return;
            }
        } else if (strcasecmp("HTTP/1.0", request->version) == 0) {
            // Only check for If-Modified-Since
            int works = 2;
            if (request -> if_modified_since != NULL) {
                struct tm time;
                if (strptime(request -> if_modified_since, date_format, & time) != NULL || strptime(request -> if_modified_since, "%A, %d-%b-%y %T GMT", & time) != NULL || strptime(request -> if_modified_since, "%c", & time) != NULL) {
                    // Now we that have the last modified date from the file and the if modified since date from the request header, we can compare the dates.
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
                char header_error[] = "304 Not Modified Since\r\n\r\n";
                strncat(bufferToSendClient, header_error, strlen(header_error));
                pthread_mutex_lock( & write_to_client_mutex);
                write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
                pthread_mutex_unlock( & write_to_client_mutex);
                return;
            }
        }

        char statusLine[] = "200 OK\r\n";
		      strncat(bufferToSendClient, statusLine, strlen(statusLine));

        char curr_date[REQ_LINE_SIZE/2];
        char header1[200]; // Make line size 200 so curr_date can fit in it.
        time_t sec = time(NULL);
        strftime(curr_date, REQ_LINE_SIZE, date_format, localtime( & (sec)));
        snprintf(header1, sizeof(header1), "Date: %s \r\n", curr_date);
		      strncat(bufferToSendClient, header1, strlen(header1));

        // From https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
        int size = (int)((ceil(log10(clientFile -> fileSize)) + 1) * sizeof(char));
        char sizeToStr[size];
        sprintf(sizeToStr, "%d", clientFile -> fileSize);
        char header2_p1[] = "Content-Length: ";
		      strncat(bufferToSendClient, header2_p1, strlen(header2_p1));
        // -1 to not write the null terminator as sprintf null terminates
		      strncat(bufferToSendClient, sizeToStr, size - 1);

        char carriageReturn[] = "\r\n";
		      strncat(bufferToSendClient, carriageReturn, strlen(carriageReturn));

        // Add Connection header
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

        // Add keep-alive header
        if (request->keep_alive == 1) {
         char keepAliveHeader[] = "Keep-Alive: timeout=5, max=100\r\n";
         strncat(bufferToSendClient, keepAliveHeader, strlen(keepAliveHeader));
        }


        if (clientFile -> fileType == 0) {
            // html file
            char header3[] = "Content-Type: text/html; charset=UTF-8\r\n\r\n";
			         strncat(bufferToSendClient, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
				            strncat(bufferToSendClient, cToStr, 1);
            }

        } else if (clientFile -> fileType == 1) {
            // css file

            // From https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/css; charset=UTF-8\r\n\r\n";
			         strncat(bufferToSendClient, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
				            strncat(bufferToSendClient, cToStr, 1);
            }
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);

        } else if (clientFile -> fileType == 2) {
            // js file

            // From https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/javascript; charset=UTF-8\r\n\r\n";
			         strncat(bufferToSendClient, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
				            strncat(bufferToSendClient, cToStr, 1);
            }
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);


        } else if (clientFile -> fileType == 3) {
            // txt file

            // From https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
			         strncat(bufferToSendClient, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = getc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
				            strncat(bufferToSendClient, cToStr, 1);
            }
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);

        } else if (clientFile -> fileType == 4) {
            // jpg file
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));

            // From https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: image/jpeg\r\n\r\n";
            write(fd_client, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = fgetc(filePtr)) != EOF) {
                char cToStr[1];
                cToStr[0] = c;
                write(fd_client, cToStr, 1);
            }
            
            pthread_mutex_unlock( & write_to_client_mutex);

        } else {
            // invalid file type
            char errorStatusLine[] = "400 Bad Request\r\n";
            strncat(bufferToSendClient, errorStatusLine, strlen(errorStatusLine));
            strncat(bufferToSendClient, errorWebpage400Pipelined, strlen(errorWebpage400Pipelined));
            pthread_mutex_lock( & write_to_client_mutex);
            write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
            pthread_mutex_unlock( & write_to_client_mutex);
			         * currentStatusCode = 400;
        }
    }
}

void * processRequestByThread(void * t) {

    printf("\t\tWhere are we C\n");

    pthread_mutex_lock( & keep_alive_mutex);
    if (keep_alive == 0) {
        return NULL;
    }
    pthread_mutex_unlock( & keep_alive_mutex);

    struct threadData * thread_data = (struct threadData * ) t;

    pthread_mutex_lock( & read_from_client_mutex);
    pthread_mutex_lock( & keep_alive_mutex);
    if (keep_alive == 0) {
        pthread_mutex_unlock( & read_from_client_mutex);
        return NULL;
    }
    pthread_mutex_unlock( & keep_alive_mutex);
    int bytes_read = get_msg(thread_data -> fd_client, thread_data -> buf);
    pthread_mutex_unlock( & read_from_client_mutex);

    // unsure about this
    if (bytes_read < 0) {
        return NULL;
    }

    *(thread_data -> httpCode) = 200; // default value

    // read in the request from the client

    //int req_bytes_read = get_msg(fd_client, buf);

    if (bytes_read < 0) {
        printf("bytes read%d\n", bytes_read);

        *(thread_data -> httpCode) = 400; // something went wrong with the request reading (tentative)
    }

    printf("%s\n", thread_data -> buf);

    struct file * clientFile = (struct file * ) malloc(sizeof(struct file));

    struct http_request * request =
        (struct http_request * ) malloc(sizeof(struct http_request));
    request -> version = calloc(9, sizeof(char));
    request -> method = calloc(4, sizeof(char));
    request -> uri = calloc(REQ_LINE_SIZE, sizeof(char));
    request -> keep_alive = 1; // default for HTTP/1.1
    request -> accept = calloc(REQ_LINE_SIZE, sizeof(char));
    request -> if_match = calloc(REQ_LINE_SIZE, sizeof(char));
    request -> if_none_match = calloc(REQ_LINE_SIZE, sizeof(char));
    request -> if_modified_since = calloc(REQ_LINE_SIZE, sizeof(char));
    request -> if_unmodified_since = calloc(REQ_LINE_SIZE, sizeof(char));

    // parse the client request
    parseRequest(thread_data -> buf, request);

    // check if connection closed from clients end, if its 0 don't nee to check
    pthread_mutex_lock( & keep_alive_mutex);
    if (keep_alive == 1) {
        keep_alive = request -> keep_alive;
    }
    pthread_mutex_unlock( & keep_alive_mutex);

    //TODO: remove
    printf("method parsed out: %s\n", request -> method);
    printf("uri parsed out: %s\n", request -> uri);
    printf("http ver parsed out: %s\n", request -> version);

    // set error codes if request file type, method or versions are invalid
    clientFile -> fileType = find_ext(request -> uri);
    if (clientFile -> fileType < 0 || is_get_req(request) == 0) {
        printf("AAAAAAAAAAAHHHHHHHHH\n");
        *(thread_data -> httpCode) = 400; // bad request
    }
    // Technically this server follows the HTTP/1.0 protocol, but requests from browser
    // may be an HTTP/1.1 request
    if (strncmp(request -> version, "HTTP/1.0", 8) != 0 && strncmp(request -> version, "HTTP/1.1", 8) != 0) {
        *(thread_data -> httpCode) = 505; // HTTP version not supported
    }

    // get full path of requested file
    clientFile -> fileName = malloc(sizeof(char) * strlen(request -> uri));
    strncpy(clientFile -> fileName, request -> uri, strlen(request -> uri));
    int path_len = strlen(thread_data -> http_root_path) + strlen(request -> uri) + 1;
    clientFile -> filePath = calloc(path_len, sizeof(char));
    strncpy(clientFile -> filePath, thread_data -> http_root_path, strlen(thread_data -> http_root_path));
    strncpy(clientFile -> filePath + strlen(thread_data -> http_root_path), request -> uri, strlen(request -> uri));
    clientFile -> filePath[strlen(thread_data -> http_root_path) + strlen(request -> uri) + 1] = '\0';
    printf("A file in clientFile = \"%s\"\n", clientFile -> filePath);

    printf("err code A = %d\n", *(thread_data -> httpCode));
    // clientFile->fileType = 2; // test file types
    // clientFile->fileSize = 2000;

    struct stat * st = (struct stat * ) malloc(sizeof(struct stat));
    clientFile -> fileSize = fsize(clientFile -> filePath, st);
    if (clientFile -> fileSize == -1) {
        printf("BHHHHHHHHHHHH path = %s\n", clientFile -> filePath);
        *(thread_data -> httpCode) = 400;
    }
    free(st);

    printf("err code B = %d\n", *(thread_data -> httpCode));

    // construct and send the HTTP response
    printf("file in clientFile = \"%s\"\n", clientFile -> filePath);
    char * bufferToSendClient = (char * ) malloc(sizeof(char) * MAX_BUF); // max http request message len
    makeServerResponsePipelinedVesion(clientFile, bufferToSendClient, thread_data -> httpCode, request, thread_data -> fd_client, 0);
    printf("buff to send to client = ---------------------\n%s\n", bufferToSendClient);

    // pthread_mutex_lock( & write_to_client_mutex);
    // write(thread_data -> fd_client, bufferToSendClient, strlen(bufferToSendClient));
    // pthread_mutex_unlock( & write_to_client_mutex);

    if (request -> keep_alive == 0) {
        // "timeout" because the client does not expect persistent connection
        *(thread_data -> httpCode) = 408; //408 Request Timeout
    }
    return NULL;
}

void * doNothing(void * t) {
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
    int * fd_server = (int *) malloc(sizeof(int));

    // char buf[MAX_BUF];

    bindAndListen(port_num, server_addr, fd_server);

    struct timeval * t_val = calloc(1, sizeof(struct timeval));
    t_val -> tv_sec = TIMEOUT;
    t_val -> tv_usec = 0;

    while (1) {
        fd_client = accept(*fd_server, (struct sockaddr * ) & client_addr, & sin_len);

        if (fd_client == -1) {
            perror("connection failed --\n");
            continue;
        }

        printf("got client connection ---\n");

        if (!fork()) {
            printf("\t\tWhere are we B\n");
            // child processes returns 0
            close(*fd_server);

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
            void * status;
            long t;

            pthread_mutex_init( & keep_alive_mutex, NULL);
            pthread_mutex_init( & write_to_client_mutex, NULL);
            pthread_mutex_init( & read_from_client_mutex, NULL);

            /* Initialize and set thread detached attribute */
            pthread_attr_init( & attr);
            pthread_attr_setdetachstate( & attr, PTHREAD_CREATE_JOINABLE);

            for (t = 0; t < MAX_REQUESTS; t++) {

                struct threadData * my_data = (struct threadData * ) malloc(sizeof(struct threadData));
                my_data -> httpCode = (int * ) malloc(sizeof(int));
                my_data -> buf = (char * ) malloc(sizeof(char) * MAX_BUF);
                my_data -> fd_client = fd_client;
                my_data -> http_root_path = (char * ) malloc(sizeof(char) * strlen(http_root_path));
                strncpy(my_data -> http_root_path, http_root_path, strlen(http_root_path));
                rc = pthread_create( & thread[t], & attr, processRequestByThread, (void * ) my_data);

                if (rc) {
                    printf("ERROR; return code from pthread_create() is %d\n", rc);
                    // exit(-1);
                }
            }

            // Keep looping as long as the client has something to send and
            // the last request was OK
            // int bytes_read;
            // int requests = 0;
            // while (keep_alive == 1 && *httpCode == 200 && (bytes_read = get_msg(fd_client, buf)) > 0 && requests < MAX_REQUESTS) {
            //     printf("kjsadflk\n");
            //     // make threads to do everything inside here
            //     struct threadData *my_data = (struct threadData *)malloc(sizeof(struct threadData));
            //     my_data->httpCode = (int*)malloc(sizeof(int));
            //     my_data->buf = (char*)malloc(sizeof(char)*MAX_BUF);
            //     my_data->fd_client = fd_client;
            //     my_data->http_root_path = (char*)malloc(sizeof(char)*strlen(http_root_path));
            //     strncpy(my_data->http_root_path, http_root_path, strlen(http_root_path));
            //     strncpy(my_data->buf, buf, MAX_BUF); // how much to copy from buf?
            //
            //     rc = pthread_create(&thread[requests], &attr, processRequestByThread, (void *)my_data);
            //     if (rc) {
            //         printf("ERROR; return code from pthread_create() is %d\n", rc);
            //         // exit(-1);
            //     }
            //     // make keep_alive a shared variable
            //     requests++;
            // }
            // make the rest
            // for (long t = requests; t < MAX_REQUESTS; t++) {
            //    rc = pthread_create(&thread[t], &attr, doNothing, (void *)t);
            // }
            //
            // // wait for all threads to end before closing the socket
            // for (long t = 0; t < MAX_REQUESTS; t++) {
            //    rc = pthread_join(thread[t], &status);
            //    if (rc) {
            //        printf("ERROR; return code from pthread_join() is %d\n", rc);
            //        // exit(-1);
            //    }
            // }

            for (long t = 0; t < MAX_REQUESTS; t++) {
                rc = pthread_join(thread[t], & status);
                if (rc) {
                    printf("ERROR; return code from pthread_join() is %d\n", rc);
                    // exit(-1);
                }
            }

            pthread_mutex_destroy( & keep_alive_mutex);
            pthread_mutex_destroy( & write_to_client_mutex);
            pthread_mutex_destroy( & read_from_client_mutex);

            printf("\t-------\tWhere are we D\n");
            close(fd_client);
            exit(0);
        }

        printf("------------------\t---did we get here\n");
        // parent process
        close(fd_client);
    }

    return 0;

}
