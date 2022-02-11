#define _XOPEN_SOURCE
#define _GNU_SOURCE

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

#include "HTTPServer.h"

// Error webpages (for requests from a browser)
char *errorWebpage301 =
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n"
    "<html><head><title>HTTP Error Message</title>\r\n"
    "<style>body {background-color: #FFFF00}</style></head>\r\n"
    "<body><center><h1>301: Moved Permanently</h1><br><p>The requested file has moved to a new location.</p>\r\n"
    "</center></body></html>\r\n";

char *errorWebpage304 =
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n"
    "<html><head><title>HTTP Error Message</title>\r\n"
    "<style>body {background-color: #FFFF00}</style></head>\r\n"
    "<body><center><h1>304: Not Modified</h1><br><p>The URL has not been modified since the specified date.</p>\r\n"
    "</center></body></html>\r\n";

char *errorWebpage400 =
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n"
    "<html><head><title>HTTP Error Message</title>\r\n"
    "<style>body {background-color: #FFFF00}</style></head>\r\n"
    "<body><center><h1>400: Bad Request</h1><br><p>The server did not understand the request.</p>\r\n"
    "</center></body></html>\r\n";

char *errorWebpage404 =
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n"
    "<html><head><title>HTTP Error Message</title>\r\n"
    "<style>body {background-color: #FFFF00}</style></head>\r\n"
    "<body><center><h1>404: Not Found</h1><br><p>The server could not find the requested file.</p>\r\n"
    "</center></body></html>\r\n";

char *errorWebpage414 =
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n"
    "<html><head><title>HTTP Error Message</title>\r\n"
    "<style>body {background-color: #FFFF00}</style></head>\r\n"
    "<body><center><h1>414: Request URL Too Long</h1><br><p>The URL in the request is too long.</p>\r\n"
    "</center></body></html>\r\n";

char *errorWebpage501 =
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n"
    "<html><head><title>HTTP Error Message</title>\r\n"
    "<style>body {background-color: #FFFF00}</style></head>\r\n"
    "<body><center><h1>501: Not Implemented</h1><br><p>The server does not support the requested functionality.</p>\r\n"
    "</center></body></html>\r\n";

char *errorWebpage505 =
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>\r\n"
    "<html><head><title>HTTP Error Message</title>\r\n"
    "<style>body {background-color: #FFFF00}</style></head>\r\n"
    "<body><center><h1>505: Version Not Supported</h1><br><p>The server only supports HTTP/1.0 and HTTP/1.1</p>\r\n"
    "</center></body></html>\r\n";

/*
Sets up a socket to listen for incoming connections and binds
it to the given port.

Idea from (CSC209) muffinman.c's bindandlisten.
*/
void bindAndListen(int port, struct sockaddr_in serv_addr, int *fd_server)
{

    // create a socket
    if ((*fd_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    int on = 1;
    // have OS release server's port as soon as server terminates
    if ((setsockopt(*fd_server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int))) == -1)
    {
        perror("setsockopt");
    }

    // set up <serv_addr> for connecting
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // bind the socket to the port
    if (bind(*fd_server, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        close(*fd_server);
        free(fd_server);
        exit(1);
    }

    // listen; set up a queue to store pending connections
    if (listen(*fd_server, 10) < 0)
    {
        perror("listen");
        close(*fd_server);
        free(fd_server);
        exit(1);
    }
}

/*
Fills <buf> with the contents received from the socket <fd_client>.
Returns the number of bytes read into the buffer or -1 if the double CRLF
was not received.
*/
int get_msg(int fd_client, char *buf)
{

    int bytes_read = 0;
    int not_done_reading = 1;

    char *currentLine = calloc(REQ_LINE_SIZE, sizeof(char));
    int emptyLines = 0; // count of CRLF received
    // Read until double CRFL is received or until buffer is full
    while (not_done_reading == 1 && bytes_read < MAX_BUF)
    {
        bytes_read += read(fd_client, currentLine, REQ_LINE_SIZE);
        strncat(buf, currentLine, strlen(currentLine));

        // Check if line has \r\n\r\n
        if (strcmp(currentLine, "") == 0)
        {
            // Received an empty line
            not_done_reading = 0;
        }
        else if (strstr(currentLine, CRLFCRLF) != NULL)
        {
            // Found the termination of the request message
            not_done_reading = 0;
        }
        else if (strcmp(CRLF, currentLine) == 0)
        {
            if (emptyLines == 1)
            {
                not_done_reading = 0;
            }
            emptyLines++;
        }
        else
        {
            emptyLines = 0;
            not_done_reading = 1;
        }
        memset(currentLine, 0, REQ_LINE_SIZE);
    }

    if (not_done_reading == 1)
    {
        return -1;
    }
    else
    {
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
int find_ext(char *fname)
{
    char *dot_ext_ptr = strrchr(fname, '.');
    if (dot_ext_ptr == NULL)
    {
        return -1;
    }
    else if (strstr(dot_ext_ptr, ".html") != NULL)
    {
        return 0;
    }
    else if (strstr(dot_ext_ptr, ".css") != NULL)
    {
        return 1;
    }
    else if (strstr(dot_ext_ptr, ".js") != NULL)
    {
        return 2;
    }
    else if (strstr(dot_ext_ptr, ".txt") != NULL)
    {
        return 3;
    }
    else if (strstr(dot_ext_ptr, ".jpg") != NULL)
    {
        return 4;
    }
    else
    {
        return -1;
    }
}

/*
Returns 1 if and only if the HTTP method for <h> is GET (not case-sensitive).
*/
int is_get_req(struct http_request *h)
{
    if (strncmp(h->method, "GET", 3) != 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/*
Returns the file size of the file with the given <filename> path.
Resources used:
https://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c
*/
off_t fsize(const char *filename, struct stat *st)
{
    if (stat(filename, st) == 0)
    {
        return st->st_size;
    }
    return -1;
}

int parseRequestInitial(char *initial, struct http_request *h)
{
    // set up pointers to the components of the initial line
    char *resource_loc_ptr = strchr(initial, ' ') + 1;
    char *ver_ptr = strchr(resource_loc_ptr, ' ') + 1;

    strncpy(h->method, initial, 3); // Assuming this server only handles GET requests, only
                                    // the first 3 characters matter.
    strncpy(h->uri, resource_loc_ptr, ver_ptr - resource_loc_ptr - 1);
    strncpy(h->version, ver_ptr, 8); // assuming this will be of the form "HTTP/1.*"

    return 0;
}

/*
Parses the client request <req> and populates the HTTP request structure
with the parsed request data, header values.
*/
int parseRequest(char *req, struct http_request *h)
{
    char *ptr = NULL;
    ptr = strtok(req, CRLF);

    char *colon_pointer = NULL;

    int line_counter = 0;
    int colon_pointer_index = 0;
    char *header = NULL;
    char *header_body = NULL;

    while (ptr != NULL)
    {
        if (line_counter > 0)
        {
            colon_pointer = strstr(ptr, ": ");
            colon_pointer_index = (int)(colon_pointer - ptr);
            header = (char *)malloc(sizeof(char) * (colon_pointer_index + 1));
            header_body = (char *)malloc(sizeof(char) * strlen(ptr));

            // get header
            for (int i = 0; i < colon_pointer_index; i++)
            {
                header[i] = ptr[i];
            }

            // get body
            for (int i = colon_pointer_index + 2; i < strlen(ptr); i++)
            {
                header_body[i - colon_pointer_index - 2] = ptr[i];
            }

            if (strcmp("If-Modified-Since", header) == 0)
            {
                // Parse 'If-Modified-Since' header value
                strncpy(h->if_modified_since, header_body, strlen(header_body));
            }

            if (strcmp("If-Unmodified-Since", header) == 0)
            {
                // Parse 'If-Unmodified-Since' header value
                strncpy(h->if_unmodified_since, header_body, strlen(header_body));
            }

            if (strcmp("If-Match", header) == 0)
            {
                // Parse 'If-Unmodified-Since' header value
                strncpy(h->if_match, header_body, strlen(header_body));
            }

            if (strcmp("If-None-Match", header) == 0)
            {
                // Parse 'If-Unmodified-Since' header value
                strncpy(h->if_none_match, header_body, strlen(header_body));
            }

            if (strncmp("Connection", header, 10) == 0)
            {
                // Parse 'Connection' header value
                if (strncmp("keep-alive", header_body, 10) == 0 || strncmp("Keep-Alive", header_body, 10) == 0)
                {
                    h->keep_alive = 1;
                }
                else
                {
                    // Body is 'close'
                    h->keep_alive = 0;
                }
            }

            free(header);
            free(header_body);
        }
        else
        {
            // first line - parse the initial line
            parseRequestInitial(ptr, h);
        }

        ptr = strtok(NULL, "\r\n");
        line_counter++;
    }

    return line_counter;
}

/*
Constructs and sends an HTTP response to the client based on the HTTP request, current status,
and the <clientFile> requested from the client.
*/
void makeServerResponse(struct file *clientFile,
                        int *currentStatusCode, struct http_request *request, int fd_client, int is_simple)
{
    FILE *filePtr;

    if ((strcasecmp("HTTP/1.1", request->version) == 0) && (is_simple == 0))
    {
        write(fd_client, "HTTP/1.1 ", strlen("HTTP/1.1 "));
    }
    else
    {
        // Default version of this server is HTTP/1.0
        write(fd_client, "HTTP/1.0 ", strlen("HTTP/1.0 "));
    }

    // Handle erroroneous requests first; on error, don't add extra headers.
    if (*currentStatusCode != 200)
    {
        if (*currentStatusCode == 301)
        {
            char statusLine[] = "301 Moved Permanently\r\n";
            write(fd_client, statusLine, strlen(statusLine));
            write(fd_client, errorWebpage301, strlen(errorWebpage301));
            return;
        }
        else if (*currentStatusCode == 400)
        {
            char statusLine[] = "400 Bad Request\r\n";
            write(fd_client, statusLine, strlen(statusLine));
            write(fd_client, errorWebpage400, strlen(errorWebpage400));
            return;
        }
        else if (*currentStatusCode == 404)
        {
            char statusLine[] = "404 Not Found\r\n";
            write(fd_client, statusLine, strlen(statusLine));
            write(fd_client, errorWebpage404, strlen(errorWebpage404));
            return;
        }
        else if (*currentStatusCode == 505)
        {
            char statusLine[] = "501 Not Implemented\r\n";
            write(fd_client, statusLine, strlen(statusLine));
            write(fd_client, errorWebpage501, strlen(errorWebpage501));
            return;
        }
        else if (*currentStatusCode == 501)
        {
            char statusLine[] = "505 HTTP Version Not Supported\r\n";
            write(fd_client, statusLine, strlen(statusLine));
            write(fd_client, errorWebpage505, strlen(errorWebpage505));
            return;
        }
        else
        {
            char statusLine[] = "400 Bad Request\r\n";
            write(fd_client, statusLine, strlen(statusLine));
            write(fd_client, errorWebpage400, strlen(errorWebpage400));
            return;
        }
    }

    filePtr = fopen(clientFile->filePath, "r");

    // Check if file exists
    if (filePtr == NULL)
    {
        char statusLine[] = "404 Not Found\r\n";
        write(fd_client, statusLine, strlen(statusLine));
        write(fd_client, errorWebpage404, strlen(errorWebpage404));
        *currentStatusCode = 404;
        return;
    }
    else
    {
        // Add headers, then add file contents

        /* ==================================== Handling Conditional Requests ==================================== */
        // If-Modified-Since: <day-name>, <day> <month> <year> <hour>:<minute>:<second> GMT format
        // need to create a format for this
        // http://www.qnx.com/developers/docs/qnxcar2/index.jsp?topic=%2Fcom.qnx.doc.neutrino.lib_ref%2Ftopic%2Fs%2Fstrptime.html
        char *date_format = "%a, %d %b %Y %T GMT";
        // collect file meta data in order to find when the file was modified last
        // from https://stackoverflow.com/questions/10446526/get-last-modified-time-of-file-in-linux
        struct stat file_metadata;
        stat(clientFile->filePath, &file_metadata);

        // Create etag that will be used in later conditional checks (based on Piazza post recommendation)
        // For reference, here is Apache's implementation of ETags:
        // http://httpd.apache.org/docs/2.2/mod/core.html#FileETag
        // etag construction: inode number - time of last modification - total size, in bytes
        char *etag;
        asprintf(&etag, "%ld-%ld-%lld", (long)file_metadata.st_ino, (long)file_metadata.st_mtime, (long long)file_metadata.st_size);
        printf("Computed Etag: %s \n", etag);

        if (strcasecmp("HTTP/1.1", request->version) == 0)
        {
            /* ==================================== if-modified-since ==================================== */
            int works = 2;
            if (request->if_modified_since != NULL)
            {
                struct tm time;
                // Converting the request time from the header request to correct format and storing it in time
                if (strptime(request->if_modified_since, date_format, &time) != NULL || strptime(request->if_modified_since, "%A, %d-%b-%y %T GMT", &time) != NULL || strptime(request->if_modified_since, "%c", &time) != NULL)
                {
                    // Now we that have the last modified date from the file and the if modified since date from the request header, we can compare the dates.
                    // difftime returns difference in seconds between two times.
                    //    last modified file            if modified since time
                    if (difftime(file_metadata.st_mtime, mktime(&time)) >= 0)
                    {
                        // If we enter here, the file has been modified so we're good
                        works = 1;
                    }
                    else
                    {
                        works = 0;
                    }
                }
            }
            if (works == 0)
            {
                // Did not pass the If-Modified-Since condition. Send error message to client.
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "304 Not Modified Since\r\n\r\n";
                write(fd_client, header_error, strlen(header_error));
                return;
            }
            /* ==================================== if-unmodified-since ==================================== */
            if (request->if_modified_since != NULL)
            {
                struct tm time;
                // Converting the request time from the header request to correct format and storing it in time
                if (strptime(request->if_unmodified_since, date_format, &time) != NULL || strptime(request->if_unmodified_since, "%A, %d-%b-%y %T GMT", &time) != NULL || strptime(request->if_unmodified_since, "%c", &time) != NULL)
                {
                    // Now we that have the last modified date from the file and the if modified since date from the request header, we can compare the dates.
                    // difftime returns difference in seconds between two times.
                    //     last modified file            if modified since time
                    if (difftime(file_metadata.st_mtime, mktime(&time)) >= 0)
                    {
                        // If we enter here, the file has been modified after the date in the header so it will fail
                        works = 0;
                    }
                    else
                    {
                        works = 1;
                    }
                }
            }
            if (works == 0)
            {
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "412 Precondition Failed\r\n\r\n";
                // write(fd_client, header_error, strlen(header_error));
                write(fd_client, header_error, strlen(header_error));
                return;
            }
            /* ==================================== if-none-match ==================================== */
            works = 3;
            if (request->if_none_match != NULL)
            {
                if (strcmp(request->if_none_match, "*") == 0)
                {
                    // etags match, it's a fail ** ask professor about this **
                    works = 0;
                }
                char *index;
                index = strtok(request->if_none_match, ", ");
                while (index != NULL)
                {
                    // checking if the etag starts with W/ and if it does, increment the index and perform a strong
                    // validation on the rest of the string
                    if (strncmp(index, "W/", 2) == 0)
                    {
                        index = index + 2;
                    }
                    if (strcmp(index, etag) == 0)
                    {
                        // etags match, it's a fail
                        works = 0;
                    }
                    index = strtok(NULL, ", ");
                }
                // etags dont match

                if (works == 3)
                {
                    // etags don't match, it's a pass
                    works = 1;
                };
            }
            if (works == 0)
            {
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "412 Precondition Failed\r\n\r\n";
                write(fd_client, header_error, strlen(header_error));
                return;
            }

            /* ==================================== if-match ==================================== */
            if (request->if_match != NULL && works != 1)
            {
                if (strcmp(request->if_match, "*") == 0)
                {
                    // etag matches
                    works = 1;
                }
                char *index;
                index = strtok(request->if_match, ", ");

                while (index != NULL)
                {
                    // Checking if the etag starts with W/ and if it does, increment the index and perform a strong
                    // validation on the rest of the string
                    if (strncmp(index, "W/", 2) == 0)
                    {
                        index = index + 2;
                    }
                    if (strcmp(index, etag) == 0)
                    {
                        // etag matches
                        works = 1;
                    }
                    index = strtok(NULL, ", ");
                }
                // etags dont match
                if (works == 3)
                {
                    works = 0;
                }
            }
            if (works == 0)
            {
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "412 Precondition Failed\r\n\r\n";
                write(fd_client, header_error, strlen(header_error));
                return;
            }
        }
        else if (strcasecmp("HTTP/1.0", request->version) == 0)
        {
            // Only check for If-Modified-Since
            int works = 2;
            if (request->if_modified_since != NULL)
            {
                struct tm time;
                if (strptime(request->if_modified_since, date_format, &time) != NULL || strptime(request->if_modified_since, "%A, %d-%b-%y %T GMT", &time) != NULL || strptime(request->if_modified_since, "%c", &time) != NULL)
                {
                    // Now we that have the last modified date from the file and the if modified since date from the request header, we can compare the dates.
                    // difftime returns difference in seconds between two times.
                    //    last modified file            if modified since time
                    if (difftime(file_metadata.st_mtime, mktime(&time)) >= 0)
                    {
                        // if we enter here, the file has been modified so we're good
                        works = 1;
                    }
                    else
                    {
                        works = 0;
                    }
                }
            }
            if (works == 0)
            {
                // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since
                char header_error[] = "304 Not Modified Since\r\n\r\n";
                write(fd_client, header_error, strlen(header_error));
                return;
            }
        }

        char statusLine[] = "200 OK\r\n";
        write(fd_client, statusLine, strlen(statusLine));

        char curr_date[REQ_LINE_SIZE / 2];
        char header1[200]; // Make line size 200 so curr_date can fit in it.
        time_t sec = time(NULL);
        strftime(curr_date, REQ_LINE_SIZE, date_format, localtime(&(sec)));
        snprintf(header1, sizeof(header1), "Date: %s \r\n", curr_date);
        write(fd_client, header1, strlen(header1));

        // From https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
        int size = (int)((ceil(log10(clientFile->fileSize)) + 1) * sizeof(char));
        char sizeToStr[size];
        sprintf(sizeToStr, "%d", clientFile->fileSize);
        char header2_p1[] = "Content-Length: ";
        write(fd_client, header2_p1, strlen(header2_p1));
        // -1 to not write the null terminator as sprintf null terminates
        write(fd_client, sizeToStr, size - 1);

        write(fd_client, CRLF, strlen(CRLF));

        // Add Connection header
        char connectionHeader_p1[] = "Connection: ";
        write(fd_client, connectionHeader_p1, strlen(connectionHeader_p1));
        if (request->keep_alive == 1)
        {
            char connectionHeader_p2[] = "Keep-Alive";
            write(fd_client, connectionHeader_p2, strlen(connectionHeader_p2));
        }
        else
        {
            char connectionHeader_p2[] = "close";
            write(fd_client, connectionHeader_p2, strlen(connectionHeader_p2));
        }
        write(fd_client, CRLF, strlen(CRLF));

        // Add keep-alive header
        if (request->keep_alive == 1)
        {
            char keepAliveHeader[] = "Keep-Alive: timeout=5, max=100\r\n";
            write(fd_client, keepAliveHeader, strlen(keepAliveHeader));
        }

        if (clientFile->fileType == 0)
        {
            // html file
            char header3[] = "Content-Type: text/html; charset=UTF-8\r\n\r\n";
            write(fd_client, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = getc(filePtr)) != EOF)
            {
                char cToStr[1];
                cToStr[0] = c;
                write(fd_client, cToStr, 1);
            }
        }
        else if (clientFile->fileType == 1)
        {
            // css file

            // From https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/css; charset=UTF-8\r\n\r\n";
            write(fd_client, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = getc(filePtr)) != EOF)
            {
                char cToStr[1];
                cToStr[0] = c;
                write(fd_client, cToStr, 1);
            }
        }
        else if (clientFile->fileType == 2)
        {
            // js file

            // From https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/javascript; charset=UTF-8\r\n\r\n";
            write(fd_client, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = getc(filePtr)) != EOF)
            {
                char cToStr[1];
                cToStr[0] = c;
                write(fd_client, cToStr, 1);
            }
        }
        else if (clientFile->fileType == 3)
        {
            // txt file

            // From https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
            write(fd_client, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = getc(filePtr)) != EOF)
            {
                char cToStr[1];
                cToStr[0] = c;
                write(fd_client, cToStr, 1);
            }
        }
        else if (clientFile->fileType == 4)
        {
            // jpg file

            // From https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
            char header3[] = "Content-Type: image/jpeg\r\n\r\n";
            write(fd_client, header3, strlen(header3));

            int c;
            // Read and write the file one byte at a time
            while ((c = fgetc(filePtr)) != EOF)
            {
                char cToStr[1];
                cToStr[0] = c;
                write(fd_client, cToStr, 1);
            }
        }
        else
        {
            // invalid file type
            char errorStatusLine[] = "400 Bad Request\r\n";
            write(fd_client, errorStatusLine, strlen(errorStatusLine));
            write(fd_client, errorWebpage400, strlen(errorWebpage400));
            *currentStatusCode = 400;
        }
    }
}