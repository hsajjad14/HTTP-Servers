#include "HTTPServer.h"

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