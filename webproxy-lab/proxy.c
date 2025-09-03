// proxy.c — CS:APP 기반 프록시 스켈레톤
#include "csapp.h"

// 스레드 구조체
typedef struct {
    int connfd;
    struct sockaddr_storage addr;
    socklen_t addrlen;
} client_arg_t;

static void client_error(int fd, int status, const char *shortmsg, const char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];
    snprintf(body, sizeof(body),
             "<html><title>Proxy Error</title>"
             "<body>%d %s<br>%s</body></html>", status, shortmsg, longmsg);
    snprintf(buf, sizeof(buf), "HTTP/1.0 %d %s\r\n", status, shortmsg); Rio_writen(fd, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "Content-Type: text/html\r\n");           Rio_writen(fd, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "Content-Length: %zu\r\n\r\n", strlen(body)); Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

static int parse_uri(const char*, char*, size_t, char*, size_t, char*, size_t);
static void forward_request_headers(int, rio_t*, const char*, const char*, const char*, const char*);
static void relay_response(int, int);


static void handle_client(int connfd) {
    rio_t rio;
    char reqline[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];

    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, reqline, MAXLINE)) return;  // 빈 요청 처리
    printf(">>> %s", reqline);

    // 요청라인 파싱
    if (sscanf(reqline, "%s %s %s", method, uri, version) != 3) {
        client_error(connfd, 400, "Bad Request", "Malformed request line");
        return;
    }

    char host[MAXLINE], port[MAXLINE], path[MAXLINE];
    if (parse_uri(uri, host, sizeof(host), port, sizeof(port), path, sizeof(path)) < 0) {
        client_error(connfd, 400, "Bad Request", "Only http://host[:port]/path is supported");
        return;
    }
    
    // 1단계: 일단 GET만 허용(추후 CONNECT/POST 등 확장)
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
        client_error(connfd, 501, "Not Implemented", "Only GET/HEAD supported for now");
        return;
    }

    int serverfd = open_clientfd(host, port);
    if (serverfd < 0) {
        // -2: getaddrinfo 실패(호스트 이름/포트 이상)
        // -1: 그 외 소켓/연결 에러
        client_error(connfd, 502, "Bad Gateway", "Cannot connect to upstream");
        return;
    }

    // TODO: 여기서 헤더 재작성 후 serverfd로 전송 → 응답 복사
    forward_request_headers(serverfd, &rio, method, path, host, port);
    relay_response(serverfd, connfd);
    Close(serverfd);
}


int parse_uri(const char *uri,
              char *host, size_t hostsz,
              char *port, size_t portsz,
              char *path, size_t pathsz)
{
    if (strncasecmp(uri, "http://", 7) != 0) {
        return -1;
    }
    const char *p = uri + 7;

    const char *slash = strchr(p, '/');
    size_t hostport_len = 0;

    if (slash) {
        hostport_len = (size_t)(slash - p);
        snprintf(path, pathsz, "%s", slash);
    } else {
        hostport_len = strlen(p);
        snprintf(path, pathsz, "/");
    }

    char hostport[MAXLINE];
    if (hostport_len >= sizeof(hostport)) return -1;
    memcpy(hostport, p, hostport_len);
    hostport[hostport_len] = '\0';

    char *colon = strchr(hostport, ':');
    if (colon) {
        *colon = '\0';
        snprintf(host, hostsz, "%s", hostport);
        snprintf(port, portsz, "%s", colon + 1);
    } else {
        snprintf(host, hostsz, "%s", hostport);
        snprintf(port, portsz, "80");
    }

    if (host[0] == '\0' || path[0] == '\0') return -1;
    return 0;
}



// 헤더 읽기 ver 과제용
static const char *UA_HDR =
"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) "
"Gecko/20120101 Firefox/10.0.3\r\n";

static void forward_request_headers(int serverfd, rio_t *client_rio,
                            const char *method,
                            const char *path,
                            const char *host,
                            const char *port) {
    char buf[MAXLINE];
    int n;

    // 1) 요청라인: HTTP/1.0
    n = snprintf(buf, sizeof(buf), "%s %s HTTP/1.0\r\n", method, path);
    Rio_writen(serverfd, buf, n);

    // 2) 클라 헤더는 끝까지 읽어서 '그냥 버림'
    while ((n = Rio_readlineb(client_rio, buf, sizeof(buf))) > 0) {
        if (!strcmp(buf, "\r\n")) break; // 빈 줄이면 헤더 종료
    }

    // 3) 우리가 보낼 최소 헤더 세트만 추가
    if (!strcmp(port, "80"))
        n = snprintf(buf, sizeof(buf), "Host: %s\r\n", host);
    else
        n = snprintf(buf, sizeof(buf), "Host: %s:%s\r\n", host, port);
    Rio_writen(serverfd, buf, n);

    Rio_writen(serverfd, UA_HDR, strlen(UA_HDR));
    Rio_writen(serverfd, "Connection: close\r\n", strlen("Connection: close\r\n"));
    Rio_writen(serverfd, "Proxy-Connection: close\r\n", strlen("Proxy-Connection: close\r\n"));

    // (선택) 압축 피해서 디버깅/채점 안정화
    Rio_writen(serverfd, "Accept-Encoding: identity\r\n", strlen("Accept-Encoding: identity\r\n"));

    // 4) 헤더 종료
    Rio_writen(serverfd, "\r\n", 2);
}


/*

// 과제에서 제공한 UA를 그대로 쓰거나 동일 문자열을 사용
static const char *UA_HDR =
"User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) "
"Gecko/20120305 Firefox/10.0.3\r\n";

static void forward_request_headers(int serverfd, rio_t *client_rio,
                                    const char *method,
                                    const char *path,
                                    const char *host,
                                    const char *port) {
    char buf[MAXLINE];
    int n;

    // 1) 요청라인: HTTP/1.0로 다운그레이드
    n = snprintf(buf, sizeof(buf), "%s %s HTTP/1.0\r\n", method, path);
    Rio_writen(serverfd, buf, n);

    // 2) 클라 헤더를 읽어 필터링하며 전달
    int saw_host = 0;
    while ((n = Rio_readlineb(client_rio, buf, sizeof(buf))) > 0) {
        if (!strcmp(buf, "\r\n")) break;                 // 헤더 끝
        if (!strncasecmp(buf, "Host:", 5)) {             // Host는 한 번만
            saw_host = 1;
            Rio_writen(serverfd, buf, strlen(buf));
            continue;
        }
        if (!strncasecmp(buf, "Connection:", 11))        continue; // hop-by-hop 제거
        if (!strncasecmp(buf, "Proxy-Connection:", 17))  continue;
        if (!strncasecmp(buf, "Keep-Alive:", 11))        continue;
        if (!strncasecmp(buf, "Transfer-Encoding:", 18)) continue; // 보수적
        // 그 외 일반 헤더는 그대로 전달
        Rio_writen(serverfd, buf, strlen(buf));
    }

    // 3) 필수 헤더 보장 삽입
    if (!saw_host) {
        if (!strcmp(port, "80"))
            n = snprintf(buf, sizeof(buf), "Host: %s\r\n", host);
        else
            n = snprintf(buf, sizeof(buf), "Host: %s:%s\r\n", host, port);
        Rio_writen(serverfd, buf, n);
    }
    Rio_writen(serverfd, UA_HDR, strlen(UA_HDR));
    Rio_writen(serverfd, "Connection: close\r\n", 19);
    Rio_writen(serverfd, "Proxy-Connection: close\r\n", 25); // (채점 호환)

    // 4) 공백줄로 헤더 종료
    Rio_writen(serverfd, "\r\n", 2);
}

*/

static void relay_response(int serverfd, int clientfd) {
    rio_t srio; char buf[MAXBUF];
    Rio_readinitb(&srio, serverfd);
    ssize_t n;
    while ((n = Rio_readnb(&srio, buf, sizeof(buf))) > 0) {
        Rio_writen(clientfd, buf, n);
    }
}

static void thread_main(void* vargp){
  Pthread_detach(pthread_self());        // join 필요 없게
  client_arg_t *arg = (client_arg_t*)vargp;
  int connfd = arg->connfd;
  free(arg);

  handle_client(connfd);
  Close(connfd);
  return(NULL);
}

// 캐시
// 캐시 초기화              캐시크기       캐시 저장 오브젝트 크기
static void cache_init(size_t max_bytes, size_t max_object_bytes) {
  
} 


int main(int argc, char **argv) {
    Signal(SIGPIPE, SIG_IGN);
    if (argc != 2) { fprintf(stderr, "usage: %s <port>\n", argv[0]); exit(1); }

    int listenfd = Open_listenfd(argv[1]);
    while (1) {
        struct sockaddr_storage clientaddr;
        socklen_t len = sizeof(clientaddr);
        int connfd = Accept(listenfd, (SA*)&clientaddr, &len);

        client_arg_t *arg = malloc(sizeof(*arg));
        arg->connfd = connfd;
        arg->addr = clientaddr;
        arg->addrlen = len;
  
        pthread_t tid;
        Pthread_create(&tid, NULL, thread_main, arg);
    }
    return 0;
}