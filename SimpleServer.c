
#include "HTTPServer.h"

char webpage[] = "HTTP/1.0 200 OK\r\n"
                 "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                 "<!DOCTYPE html>\r\n"
                 "<html><head><title>SimpleServerWebpage</title>\r\n"
                 "<style>body {background-color: #FFFF00}</style></head>\r\n"
                 "<body><center><h1> Hello World!</h1><br>\r\n"
                 "</center></body></html>\r\n";

static int fd_server;

void bindAndListen(int port, struct sockaddr_in serv_addr) {

  // create a socket
  if ((fd_server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  int on = 1;
  // have OS release server's port as soon as server terminates
  if ((setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int))) ==
      -1) {
    perror("setsockopt");
  }

  // set up <serv_addr> for connecting
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  // bind the socket to the port
  if (bind(fd_server, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
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
int get_msg(int client_fd, char *buf) {
  int bytes_recvd, tot_recvd = 0;
  char *buf_ptr;
  int buf_size = strlen(buf);
  int crlf_flag = 0; // 0 = none of it, 1 = got "\r", 2 = got "\n"
  while (((bytes_recvd = recv(client_fd, buf, 1, 0)) > 0) &&
         (tot_recvd < buf_size)) {
    if (*buf_ptr == CRLF[crlf_flag]) {
      crlf_flag++;
      if (crlf_flag == 3) {
        *(buf_ptr - 3) = '\0'; // null terminate buffer, truncating "\r\n\r\n"
        return strlen(buf);
      }
    } else {
      crlf_flag = 0; // reset the "progress" flag
    }
    buf_ptr++;
    tot_recvd++;
  }
  return -1;
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
int find_ext(char *fname) {
  char *dot_ext_ptr = strrchr(fname, '.');
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

// get file size
// https://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c
off_t fsize(const char *filename, struct stat *st) {
  // printf("file path = \"%s\"\n",filename);
  // char *some = "square.js";
  if (stat(filename, st) == 0) {
    // printf("here 1\n");
    return st->st_size;
  }
  return -1;
}

int parseRequestInitial(char *initial, struct http_request *h) {

  // char *request = calloc(4, sizeof(char)); // Assuming this server only
  // handles GET requests
  // char *resource_loc = calloc(strlen(initial), sizeof(char));
  // char *ver = calloc(2, sizeof(char));

  char *resource_loc_ptr = strchr(initial, ' ') + 1;
  char *ver_ptr = strchr(resource_loc_ptr, ' ') + 1;
  printf("initial line = \"%s\"\n", initial);
  printf("resource_loc_ptr = \"%s\"\n", resource_loc_ptr);
  printf("ver_ptr = \"%s\"\n", ver_ptr);

  strncpy(h->method, initial, 3);
  strncpy(h->uri, resource_loc_ptr+1, ver_ptr - resource_loc_ptr - 2);
  h->uri[ver_ptr - resource_loc_ptr - 2] = '\0';
  printf("huri a = \"%s\"\n",h->uri );

  char *endptr;
  h->version = strtol(ver_ptr + strlen("HTTP/1."), &endptr, 10);

  return 0;
}

int parseRequest(char *req, struct http_request *h) {
  // printf("in parseRequest\n");
  char *ptr = NULL;
  ptr = strtok(req, "\r\n");

  // parse initial line of HTTP request
  // int init_size = strlen(ptr) + 1;
  // char *initial = calloc(init_size, sizeof(char));
  // strncpy(initial, ptr, init_size - 1);
  // parseRequestInitial(initial, h);

  char *colon_pointer = NULL;

  int line_counter = 0;
  int colon_pointer_index = 0;
  char *header = NULL;
  char *header_body = NULL;

  while (ptr != NULL) {
    if (line_counter > 0) {
      colon_pointer = strstr(ptr, ": ");
      colon_pointer_index = (int)(colon_pointer - ptr);
      header = (char *)malloc(sizeof(char) * (colon_pointer_index + 1));
      header_body = (char *)malloc(sizeof(char) * strlen(ptr));

      // get header
      for (int i = 0; i < colon_pointer_index; i++) {
        header[i] = ptr[i];
      }

      // get body
      for (int i = colon_pointer_index + 2; i < strlen(ptr); i++) {
        // printf("\tHmmmm %d\n",i);
        header_body[i - colon_pointer_index - 2] = ptr[i];
      }

      // printf("colon at index = %ld, length of line = %ld\n", colon_pointer -
      // ptr, strlen(ptr)); printf("header is \"%s\"\n", header); printf("header
      // body is \"%s\"\n", header_body);

      free(header);
      free(header_body);
    } else {
      // first line
      parseRequestInitial(ptr, h);
    }
    printf("request line = \"%s\"\n\n", ptr);
    ptr = strtok(NULL, "\r\n");
    line_counter++;
  }
  return 0;
}

void makeServerResponse(struct file *clientFile, char *bufferToSendClient,
                        int *currentStatusCode, struct http_request *request) {
  FILE *filePtr;

  if (*currentStatusCode != 200) {
    // use 2048 as max buffer size for now
    memset(bufferToSendClient, 0, MAX_BUF);
    // there is a prior error
    // 301 Moved Permanently
    // 400 Bad Request
    // 404 Not Found
    // 505 HTTP Version Not Supported

    // on errors don't add any headers
    if (*currentStatusCode == 301) {
      char statusLine[] = "HTTP/1.0 301 Moved Permanently\r\n";
      strncpy(bufferToSendClient, statusLine, strlen(statusLine));
      return;

    } else if (*currentStatusCode == 400) {
      char statusLine[] = "HTTP/1.0 400 Bad Request\r\n";
      strncpy(bufferToSendClient, statusLine, strlen(statusLine));
      return;

    } else if (*currentStatusCode == 505) {
      char statusLine[] = "HTTP/1.0 505 HTTP Version Not Supported\r\n";
      strncpy(bufferToSendClient, statusLine, strlen(statusLine));
      return;

    } else {
      char statusLine[] = "HTTP/1.0 400 Bad Request\r\n";
      strncpy(bufferToSendClient, statusLine, strlen(statusLine));
      return;
    }
  }

  // filePtr = fopen(clientFile->filePath, "r");
  filePtr = fopen(request->uri, "r");

  // check if file exists:
  if (filePtr == NULL) { // actual condition
    // if (0) { // condition for testing because we haven't implemented file io
    // use 2048 as max buffer size for now
    printf("do we go here?\n");
    memset(bufferToSendClient, 0, MAX_BUF);
    char statusLine[] = "HTTP/1.0 404 Not Found\r\n";
    strncpy(bufferToSendClient, statusLine, strlen(statusLine));
    *currentStatusCode = 404;
    return;

  } else {
    // file found

    // read it
    // for now use this:
    // char tempHtmlFile[] = "<!DOCTYPE html>\r\n"
    //          "<html><head><title>SimpleServerWebpage</title>\r\n"
    //          "<style>body {backgrou-color: #FFFF00}</style></head>\r\n"
    //          "<body><center><h1> Hello World!</h1><br>\r\n"
    //          "</center></body></html>\r\n";

    memset(bufferToSendClient, 0, MAX_BUF);
    char statusLine[] = "HTTP/1.0 200 OK\r\n";
    strncpy(bufferToSendClient, statusLine, strlen(statusLine));

    // headers
    char header1[] = "Date: get date somehow\r\n";
    strncat(bufferToSendClient, header1, strlen(header1));

    // from
    // https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
    int size = (int)((ceil(log10(clientFile->fileSize)) + 1) * sizeof(char));
    char sizeToStr[size];
    sprintf(sizeToStr, "%d", clientFile->fileSize);
    char header2_p1[] = "Content-Length: ";
    strncat(bufferToSendClient, header2_p1, strlen(header2_p1));
    // -1 to not write the null terminator as sprintf null terminates
    strncat(bufferToSendClient, sizeToStr, size - 1);

    char carriageReturn[] = "\r\n";
    strncat(bufferToSendClient, carriageReturn, strlen(carriageReturn));

    if (clientFile->fileType == 0) {
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

    } else if (clientFile->fileType == 1) {
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

    } else if (clientFile->fileType == 2) {
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

    } else if (clientFile->fileType == 3) {
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
    } else if (clientFile->fileType == 4) {
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
      *currentStatusCode = 400;
    }
  }
}

int main(int argc, char **argv) {

  // parse in command line arguments
  if (argc != 3) {
    fprintf(stderr, "Usage: ./simpleserver port_num http_root_path\n");
    exit(-1);
  }

  char *strtol_buf;
  int port_num = strtol(argv[1], &strtol_buf, 10);
  if (port_num < 0 || port_num > 65536) {
    fprintf(stderr, "Port number out of range.\n");
    exit(-1);
  }
  char *http_root_path = argv[2];
  // this is to move one / up so / => root directory
  http_root_path = http_root_path+1;

  struct sockaddr_in server_addr, client_addr;
  socklen_t sin_len = sizeof(client_addr);
  // int fd_server;
  int fd_client;

  char buf[MAX_BUF];

  bindAndListen(port_num, server_addr);

  while (1) {
    fd_client = accept(fd_server, (struct sockaddr *)&client_addr, &sin_len);

    if (fd_client == -1) {
      perror("connection failed --\n");
      continue;
    }
    int request_line_size = 100;
    printf("got client connection ---\n");

    if (!fork()) {

      // child processes returns 0
      close(fd_server);

      memset(buf, 0, MAX_BUF);

      read(fd_client, buf, MAX_BUF - 1);
      // char *req_buf = calloc(MAX_BUF, sizeof(char *));
      // get_msg(fd_client, req_buf);

      printf("%s\n", buf);

      struct file *clientFile = (struct file *)malloc(sizeof(struct file));
      int *httpCode = (int *)malloc(sizeof(int)); // why is this a ptr?
      *httpCode = 200;                            // default

      struct http_request *request =
          (struct http_request *)malloc(sizeof(struct http_request));
      request->version = -1;
      request->method = calloc(request_line_size, sizeof(char));
      request->uri = calloc(request_line_size, sizeof(char));
      request->accept = calloc(request_line_size, sizeof(char));
      request->if_match = calloc(request_line_size, sizeof(char));
      request->if_none_match = calloc(request_line_size, sizeof(char));
      request->if_modified_since = calloc(request_line_size, sizeof(char));
      request->if_unmodified_since = calloc(request_line_size, sizeof(char));

      // read and parse buffer
      parseRequest(buf, request);

      printf("method parsed out: %s\n", request->method);
      printf("uri parsed out: %s\n", request->uri);
      printf("http ver parsed out: %d\n", request->version);
      clientFile->fileType = find_ext(request->uri);
      if (clientFile->fileType < 0) {
        *httpCode = 400;
      }

      // get full path of requested file
      clientFile->fileName = malloc(sizeof(char) * strlen(request->uri));
      strncpy(clientFile->fileName, request->uri, strlen(request->uri));
      int path_len = strlen(http_root_path) + strlen(request->uri) + 1;
      clientFile->filePath = calloc(path_len, sizeof(char));
      strncpy(clientFile->filePath, http_root_path, strlen(http_root_path));
      strncpy(clientFile->filePath + strlen(http_root_path), request->uri, strlen(request->uri));
      clientFile->filePath[strlen(request->uri)] = '\0';
      printf("A file in clientFile = \"%s\"\n", clientFile->filePath);

      // char someFileName[] = "square.js";
      //
      // strncpy(request->uri, someFileName, strlen(someFileName));

      printf("err code A = %d\n", *httpCode);
      // clientFile->fileType = 2; // test file types
      // clientFile->fileSize = 2000;

      struct stat *st = (struct stat *)malloc(sizeof(struct stat));
      clientFile->fileSize = fsize(clientFile->filePath, st);
      if (clientFile->fileSize == -1) {
        *httpCode = 400;
      }
      free(st);

      printf("err code B = %d\n", *httpCode);

      // char someFileName[] = "simpleServer_webpage.html";
      // char someFileName[] = "square.js";

      // clientFile->fileName = malloc(sizeof(char)*strlen(someFileName));

      // char someFilePath[] = "simpleServer_webpage.html";
      // char someFilePath[] = "square.js";

      // clientFile->filePath = malloc(sizeof(char)*(strlen(someFilePath)));
      // clientFile->filePath = malloc(sizeof(char)*(strlen(request->uri)));
      // strncpy(clientFile->filePath, request->uri, strlen(request->uri));
      // clientFile->filePath[strlen(request->uri)] = '\0';

      // strncat(clientFile->filePath, someFileName, strlen(someFileName));
      printf("file in clientFile = \"%s\"\n", clientFile->filePath);
      char *bufferToSendClient = (char *)malloc(
          sizeof(char) * MAX_BUF); // max http request message len
      makeServerResponse(clientFile, bufferToSendClient, httpCode, request);
      printf("buff to send to client = ---------------------\n%s\n", bufferToSendClient);

      write(fd_client, bufferToSendClient, strlen(bufferToSendClient));
      close(fd_client);
      printf("closing client connection--\n");

      exit(0);
    }
    // parent process
    close(fd_client);
  }

  return 0;
  // hello
}
