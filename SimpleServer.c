#include "HTTPServer.h"


int main(int argc, char ** argv) {

    // Parse in command line arguments
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
    int * fd_server = (int *) malloc(sizeof(int));
    int fd_client;

    char buf[MAX_BUF];

    bindAndListen(port_num, server_addr, fd_server);

    while (1) {
        fd_client = accept(* fd_server, (struct sockaddr * ) & client_addr, & sin_len);

        if (fd_client == -1) {
            perror("connection failed --\n");
            continue;
        }

        if (!fork()) {

            // Child processes returns 0
            close(* fd_server);
            free(fd_server);

            int * httpCode = (int * ) malloc(sizeof(int));
            * httpCode = 200; // default value

            // Read in the request from the client
            memset(buf, 0, MAX_BUF);
            int req_bytes_read = get_msg(fd_client, buf);

            if (req_bytes_read < 0) {
                * httpCode = 400; // Something went wrong with the request reading (tentative)
            }

            struct file * clientFile = (struct file * ) malloc(sizeof(struct file));

            struct http_request * request =
                (struct http_request * ) malloc(sizeof(struct http_request));
            request -> version = calloc(9, sizeof(char));
            request -> method = calloc(4, sizeof(char));
            request -> uri = calloc(REQ_LINE_SIZE, sizeof(char));
            request -> accept = calloc(REQ_LINE_SIZE, sizeof(char));
            request -> keep_alive = 0; // assume a closed connection for HTTP/1.0 SimpleServer
            request -> if_match = calloc(REQ_LINE_SIZE, sizeof(char));
            request -> if_none_match = calloc(REQ_LINE_SIZE, sizeof(char));
            request -> if_modified_since = calloc(REQ_LINE_SIZE, sizeof(char));
            request -> if_unmodified_since = calloc(REQ_LINE_SIZE, sizeof(char));

            // Parse the client request
            parseRequest(buf, request);
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

            // get full path of requested file
            clientFile -> fileName = malloc(sizeof(char) * strlen(request -> uri));
            strncpy(clientFile -> fileName, request -> uri, strlen(request -> uri));
            int path_len = strlen(http_root_path) + strlen(request -> uri) + 1;
            clientFile -> filePath = calloc(path_len, sizeof(char));
            strncpy(clientFile -> filePath, http_root_path, strlen(http_root_path));
            strncpy(clientFile -> filePath + strlen(http_root_path), request -> uri, strlen(request -> uri));
            clientFile -> filePath[strlen(http_root_path) + strlen(request -> uri) + 1] = '\0';
            struct stat * st = (struct stat * ) malloc(sizeof(struct stat));
            clientFile -> fileSize = fsize(clientFile -> filePath, st);
            if (clientFile -> fileSize == -1) {
                * httpCode = 400;
            }
            free(st);

            // Construct and send the HTTP response
            makeServerResponse(clientFile, httpCode, request, fd_client, 1);

            free(request -> version);
            free(request -> method);
            free(request -> uri);
            free(request -> accept);
            free(request -> if_match);
            free(request -> if_none_match);
            free(request -> if_modified_since);
            free(request -> if_unmodified_since);
            free(request);

            free(clientFile -> fileName);
            free(clientFile -> filePath);
            free(clientFile);

            close(fd_client);

            exit(0);
        }

        // Parent process
        close(fd_client);
    }

    return 0;

}
