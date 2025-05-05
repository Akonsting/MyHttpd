#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

#define IS_SPACE(c)      isspace((int)(c))
#define SERVER_HEADER    "Server: myhttpd/1.0\r\n"

void handleClient(int sockfd);
int  readLine(int sockfd, char *buff, int maxLen);
void sendError(int sockfd, int code);
void serveStatic(int sockfd, const char *filePath);
void streamFile(int sockfd, FILE *fp);
void runCgi(int sockfd, const char *execPath, const char *method, const char *args);
int  setupListener(u_short *port);

int main() {
    u_short port = 0;
    int listenFd = setupListener(&port);
    printf("Server listening on port %d\n", port);

    while (1) {
        int clientFd = accept(listenFd, NULL, NULL);
        if (clientFd < 0) {
            perror("accept");
            continue;
        }
        handleClient(clientFd);
        close(clientFd);
    }

    close(listenFd);
    return 0;
}

int setupListener(u_short *port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(*port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (*port == 0) {
        socklen_t len = sizeof(addr);
        if (getsockname(fd, (struct sockaddr *)&addr, &len) < 0) {
            perror("getsockname");
            exit(1);
        }
        *port = ntohs(addr.sin_port);
    }

    if (listen(fd, 10) < 0) {
        perror("listen");
        exit(1);
    }
    return fd;
}

void handleClient(int sockfd) {
    char line[1024], method[16], url[256];
    int len = readLine(sockfd, line, sizeof(line));
    if (len <= 0) return;

    sscanf(line, "%s %s", method, url);

    int isCgi = 0;
    char *query = NULL;
    if (strcasecmp(method, "POST") == 0) {
        isCgi = 1;
    } else if (strcasecmp(method, "GET") == 0) {
        query = strchr(url, '?');
        if (query) {
            *query++ = '\0';
            isCgi = 1;
        }
    } else {
        sendError(sockfd, 501);
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "wwwroot%s", url);
    if (path[strlen(path)-1] == '/') {
        strcat(path, "index.html");
    }

    struct stat st;
    if (stat(path, &st) < 0) {
        sendError(sockfd, 404);
    } else if (!isCgi && !(st.st_mode & S_IXUSR)) {
        serveStatic(sockfd, path);
    } else {
        runCgi(sockfd, path, method, query);
    }
}

int readLine(int sockfd, char *buff, int maxLen) {
    int i = 0;
    char c = '\0';
    while (i < maxLen - 1 && c != '\n') {
        if (recv(sockfd, &c, 1, 0) <= 0) break;
        if (c == '\r') {
            recv(sockfd, &c, 1, MSG_PEEK);
            if (c == '\n') recv(sockfd, &c, 1, 0);
            else c = '\n';
        }
        buff[i++] = c;
    }
    buff[i] = '\0';
    return i;
}

void sendError(int sockfd, int code) {
    char buf[256];
    const char *msg = (code == 404) ? "Not Found"
                       : (code == 501) ? "Not Implemented"
                       : "Bad Request";
    snprintf(buf, sizeof(buf), "HTTP/1.0 %d %s\r\n", code, msg);
    send(sockfd, buf, strlen(buf), 0);
    send(sockfd, SERVER_HEADER, strlen(SERVER_HEADER), 0);
    send(sockfd, "Content-Type: text/html\r\n\r\n", 28, 0);
    snprintf(buf, sizeof(buf), "<h1>%d %s</h1>\n", code, msg);
    send(sockfd, buf, strlen(buf), 0);
}

void serveStatic(int sockfd, const char *filePath) {
    char buff[1024];
    // 丢弃所有请求头
    while (readLine(sockfd, buff, sizeof(buff)) > 0 && strcmp(buff, "\n"));
    FILE *fp = fopen(filePath, "r");
    if (!fp) { sendError(sockfd, 404); return; }
    // 发送响应头
    snprintf(buff, sizeof(buff), "HTTP/1.0 200 OK\r\n");
    send(sockfd, buff, strlen(buff), 0);
    send(sockfd, SERVER_HEADER, strlen(SERVER_HEADER), 0);
    send(sockfd, "Content-Type: text/html\r\n\r\n", 28, 0);
    // 发送文件内容
    streamFile(sockfd, fp);
    fclose(fp);
}

void streamFile(int sockfd, FILE *fp) {
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp)) {
        send(sockfd, buf, strlen(buf), 0);
    }
}

void runCgi(int sockfd, const char *execPath, const char *method, const char *args) {
    char buff[1024];
    int inPipe[2], outPipe[2];
    int content_length = -1;

    while (readLine(sockfd, buff, sizeof(buff)) > 0 && strcmp(buff, "\n")) {
        if (strncmp(buff, "Content-Length:", 15) == 0)
            content_length = atoi(buff + 15);
    }

    snprintf(buff, sizeof(buff), "HTTP/1.0 200 OK\r\n");
    send(sockfd, buff, strlen(buff), 0);
    send(sockfd, SERVER_HEADER, strlen(SERVER_HEADER), 0);

    if (pipe(inPipe) < 0 || pipe(outPipe) < 0) {
        sendError(sockfd, 500);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        sendError(sockfd, 500);
        return;
    }

    if (pid == 0) {
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(inPipe[0], STDIN_FILENO);
        close(outPipe[0]);
        close(inPipe[1]);

        setenv("REQUEST_METHOD", method, 1);
        if (strcasecmp(method, "GET") == 0 && args)
            setenv("QUERY_STRING", args, 1);
        else if (strcasecmp(method, "POST") == 0 && content_length > 0) {
            char len_str[32];
            snprintf(len_str, sizeof(len_str), "%d", content_length);
            setenv("CONTENT_LENGTH", len_str, 1);
        }

        execl(execPath, execPath, NULL);
        exit(1);
    } else {
        close(outPipe[1]);
        close(inPipe[0]);

        if (strcasecmp(method, "POST") == 0 && content_length > 0) {
            int i;
            for (i = 0; i < content_length; i++) {
                recv(sockfd, &buff[i], 1, 0);
            }
            write(inPipe[1], buff, content_length);
        }

        while (read(outPipe[0], &buff, 1) > 0) {
            send(sockfd, &buff, 1, 0);
        }
        close(outPipe[0]);
        close(inPipe[1]);

        waitpid(pid, NULL, 0);
    }
}
