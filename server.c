// server.c
// Threaded TCP Echo Server (lowercase -> UPPERCASE) with mutex-protected connected-clients counter
// Loopback only: 127.0.0.1
// Buffer size: 4096
// Uses system calls (socket/bind/listen/accept/recv/send/close) + pthread + mutex

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 4096
#define DEFAULT_PORT 5555
#define BACKLOG 64

static pthread_mutex_t g_clients_mtx = PTHREAD_MUTEX_INITIALIZER;
static int g_connected_clients = 0;

static void die(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

static ssize_t send_all(int fd, const void *buf, size_t len) {
  const unsigned char *p = (const unsigned char *)buf;
  size_t total = 0;

  while (total < len) {
    ssize_t n = send(fd, p + total, len - total, 0);
    if (n < 0) {
      if (errno == EINTR) continue; 
    }
    if (n == 0) break; 
    total += (size_t)n;
  }
  return (ssize_t)total;
}

static void to_uppercase(unsigned char *buf, size_t n) {
  for (size_t i = 0; i < n; i++) {
    buf[i] = (unsigned char)toupper((unsigned char)buf[i]);
  }
}

typedef struct {
  int client_fd;
  struct sockaddr_in client_addr;
} client_ctx_t;

static void inc_clients(void) {
  pthread_mutex_lock(&g_clients_mtx);
  g_connected_clients++;
  int now = g_connected_clients;
  pthread_mutex_unlock(&g_clients_mtx);

  fprintf(stderr, "[server] connected clients = %d\n", now);
}

static void dec_clients(void) {
  pthread_mutex_lock(&g_clients_mtx);
  g_connected_clients--;
  int now = g_connected_clients;
  pthread_mutex_unlock(&g_clients_mtx);

  fprintf(stderr, "[server] connected clients = %d\n", now);
}

static void *client_thread(void *arg) {
  client_ctx_t *ctx = (client_ctx_t *)arg;
  int fd = ctx->client_fd;

  free(ctx);

  inc_clients();

  unsigned char buf[BUF_SIZE];

  while (1) {
    ssize_t r = recv(fd, buf, sizeof(buf), 0);
    if (r < 0) {
      if (errno == EINTR) continue; 
      perror("[server] recv");
      break;
    }
    if (r == 0) {
      break;
    }

    to_uppercase(buf, (size_t)r);

    if (send_all(fd, buf, (size_t)r) < 0) {
      perror("[server] send");
      break;
    }
  }

  close(fd);
  dec_clients();
  return NULL;
}

int main(int argc, char **argv) {
  signal(SIGPIPE, SIG_IGN);

  int port = DEFAULT_PORT;
  if (argc >= 2) {
    port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
      fprintf(stderr, "Usage: %s [port]\n", argv[0]);
      return EXIT_FAILURE;
    }
  }

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) die("socket");

  int opt = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    die("setsockopt(SO_REUSEADDR)");
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) die("bind");

  if (listen(listen_fd, BACKLOG) < 0) die("listen");

  fprintf(stderr, "[server] listening on 127.0.0.1:%d\n", port);

  while (1) {
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&caddr, &clen);
    if (client_fd < 0) {
      if (errno == EINTR) continue;
      perror("accept");
      continue; // keep server alive
    }

    client_ctx_t *ctx = (client_ctx_t *)malloc(sizeof(client_ctx_t));
    if (!ctx) {
      fprintf(stderr, "[server] malloc failed\n");
      close(client_fd);
      continue;
    }
    ctx->client_fd = client_fd;
    ctx->client_addr = caddr;

    pthread_t tid;
    if (pthread_create(&tid, NULL, client_thread, ctx) != 0) {
      perror("[server] pthread_create");
      close(client_fd);
      free(ctx);
      continue;
    }

    pthread_detach(tid);
  }

  close(listen_fd);
  return 0;
}
