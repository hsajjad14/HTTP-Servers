#include "HTTPServer.h"

int main(int argc, char ** argv) {

    // Parse in command line arguments
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
    int * fd_server = (int *) malloc(sizeof(int));
    int fd_client;

    char buf[MAX_BUF];

    bindAndListen(port_num, server_addr, fd_server);

    struct timeval * t_val = calloc(1, sizeof(struct timeval));
    t_val -> tv_sec = TIMEOUT;
    t_val -> tv_usec = 0;

    while (1) {
        fd_client = accept(* fd_server, (struct sockaddr * ) & client_addr, & sin_len);

        if (fd_client == -1) {
            perror("connection failed --\n");
            continue;
        }

        printf("got client connection ---\n");

        if (!fork()) {
            printf("\t\tWhere are we B\n");
            // Child processes returns 0
            close(* fd_server);
            free(fd_server);

            // Use SO_RECVTIMEO flag to specify a timeout value for input from socket
            // Sources: https://stackoverflow.com/questions/4181784/how-to-set-socket-timeout-in-c-when-making-multiple-connections
            if (setsockopt(fd_client, SOL_SOCKET, SO_RCVTIMEO, (const char * ) t_val, sizeof(struct timeval)) == -1) {
                perror("setsockopt rcv timeout");
            }
            if (setsockopt(fd_client, SOL_SOCKET, SO_SNDTIMEO, (const char * ) t_val, sizeof(struct timeval)) == -1) {
                perror("setsockopt send timout");
            }

            //printf("\t\tWhere are we A\n");
            int * httpCode = (int * ) malloc(sizeof(int));
            * httpCode = 200; // default value

            // Keep looping as long as the client has something to send and the last request was OK
            int bytes_read;
            int keep_alive = 1; // Assume 1 because this is a persistent server
            int requests = 0;
            while (keep_alive == 1 && * httpCode == 200 && ((bytes_read = get_msg(fd_client, buf)) > 0) && requests < MAX_REQUESTS) {
                //printf("\t\tWhere are we C\n");
                * httpCode = 200; // Default value

                if (bytes_read < 0) {
                    * httpCode = 400; // Something went wrong with the request reading
                }

                printf("buf: %s, bytes-read: %d\n", buf, bytes_read);

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

                // Parse the client request
                int num_request_lines = parseRequest(buf, request);
                // Loop again; server did not get a complete request (initial line).
                if (num_request_lines == 0) {
                    continue;
                }
                // Check if connection closed from clients end
                keep_alive = request->keep_alive;
                //TODO: remove
                printf("method parsed out: %s\n", request -> method);
                printf("uri parsed out: %s\n", request -> uri);
                printf("http ver parsed out: %s\n", request -> version);

                // Set error codes if request file type, method or versions are invalid
                clientFile -> fileType = find_ext(request -> uri);
                if (clientFile -> fileType < 0 || is_get_req(request) == 0) {
                    * httpCode = 501; // Feature (filetype or method) is not supported
                }
                // Technically this server follows the HTTP/1.0 protocol, but requests from browser
                // may be an HTTP/1.1 request
                if (strncmp(request -> version, "HTTP/1.0", 8) != 0 && strncmp(request -> version, "HTTP/1.1", 8) != 0) {
                    * httpCode = 505; // HTTP version not supported
                }

                // Get full path of requested file
                clientFile -> fileName = malloc(sizeof(char) * strlen(request -> uri));
                strncpy(clientFile -> fileName, request -> uri, strlen(request -> uri));
                int path_len = strlen(http_root_path) + strlen(request -> uri) + 1;
                clientFile -> filePath = calloc(path_len, sizeof(char));
                strncpy(clientFile -> filePath, http_root_path, strlen(http_root_path));
                strncpy(clientFile -> filePath + strlen(http_root_path), request -> uri, strlen(request -> uri));
                clientFile -> filePath[strlen(http_root_path) + strlen(request -> uri) + 1] = '\0';
                //printf("A file in clientFile = \"%s\"\n", clientFile -> filePath);

                printf("err code A = %d\n", * httpCode);

                struct stat * st = (struct stat * ) malloc(sizeof(struct stat));
                clientFile -> fileSize = fsize(clientFile -> filePath, st);
                if (clientFile -> fileSize == -1) {
                    * httpCode = 404;
                }
                free(st);

                printf("err code B = %d\n", * httpCode);

                // Construct and send the HTTP response
                printf("file in clientFile = \"%s\"\n", clientFile -> filePath);
                makeServerResponse(clientFile, httpCode, request, fd_client, 0);
               
                if (request -> keep_alive == 0) {
                    // "Timeout" because the client does not expect persistent connection
                    * httpCode = 408; // 408 Request Timeout
                }

                // Reset the buffer
                memset(buf, 0, MAX_BUF);
                requests++;

            }

            //printf("\t\tWhere are we D\n");
            close(fd_client);
            exit(0);
        }

        // parent process
        close(fd_client);
    }

    return 0;

}
