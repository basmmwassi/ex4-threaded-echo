// client.c
// Multi-threaded TCP client simulator: creates at least 5 threads.
// Each thread connects to 127.0.0.1, sends a string, receives processed response, prints it.
// Handles partial sends/receives with loops.

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 4096
#define DEFAULT_PORT 5555
#define DEFAULT_THREADS 5

typedef struct {
  int id;
  int port;
  const char *msg;
} worker_args_t;

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
      return -1;
    }
    if (n == 0) break;
    total += (size_t)n;
  }
  return (ssize_t)total;
}

// Receive exactly len bytes (advanced-style).
// For echo: we expect server returns same length as sent.
// If server closes early, return bytes received so far.
static ssize_t recv_exact(int fd, void *buf, size_t len) {
  unsigned char *p = (unsigned char *)buf;
  size_t total = 0;

  while (total < len) {
    ssize_t n = recv(fd, p + total, len - total, 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) break; // server closed
    total += (size_t)n;
  }
  return (ssize_t)total;
}

static void *worker(void *arg) {
  worker_args_t *a = (worker_args_t *)arg;

  // 1) socket
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("[client] socket");
    return NULL;
  }

  // 2) connect to 127.0.0.1:port
  struct sockaddr_in srv;
  memset(&srv, 0, sizeof(srv));
  srv.sin_family = AF_INET;
  srv.sin_port = htons((uint16_t)a->port);
  srv.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (connect(fd, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
    perror("[client] connect");
    close(fd);
    return NULL;
  }

  // 3) send message
  size_t len = strlen(a->msg);
  if (len == 0) {
    close(fd);
    return NULL;
  }
  if (len > BUF_SIZE) len = BUF_SIZE; // keep it safe

  if (send_all(fd, a->msg, len) < 0) {
    perror("[client] send");
    close(fd);
    return NULL;
  }

  // 4) recv response (same length)
  char resp[BUF_SIZE + 1];
  memset(resp, 0, sizeof(resp));

  ssize_t r = recv_exact(fd, resp, len);
  if (r < 0) {
    perror("[client] recv");
    close(fd);
    return NULL;
  }
  resp[r] = '\0';

  // 5) print
  printf("[client thread %d] sent: \"%.*s\" | got: \"%s\"\n",
         a->id, (int)len, a->msg, resp);

  // 6) close socket
  close(fd);
  return NULL;
}

int main(int argc, char **argv) {
  int port = DEFAULT_PORT;
  int threads = DEFAULT_THREADS;

  if (argc >= 2) port = atoi(argv[1]);
  if (argc >= 3) threads = atoi(argv[2]);

  if (port <= 0 || port > 65535 || threads < 5) {
    fprintf(stderr, "Usage: %s [port] [threads>=5]\n", argv[0]);
    return 1;
  }

  const char *messages[] = {
      "hello from thread!",
      "system programming is fun",
      "abcXYZ 123",
      "Shenkar test",
      "lowercase -> uppercase"
  };
  int msg_count = (int)(sizeof(messages) / sizeof(messages[0]));

  pthread_t *tids = (pthread_t *)calloc((size_t)threads, sizeof(pthread_t));
  worker_args_t *args = (worker_args_t *)calloc((size_t)threads, sizeof(worker_args_t));
  if (!tids || !args) die("calloc");

  for (int i = 0; i < threads; i++) {
    args[i].id = i + 1;
    args[i].port = port;
    args[i].msg = messages[i % msg_count];

    if (pthread_create(&tids[i], NULL, worker, &args[i]) != 0) {
      perror("[client] pthread_create");
      // continue creating others
    }
  }

  for (int i = 0; i < threads; i++) {
    pthread_join(tids[i], NULL);
  }

  free(tids);
  free(args);
  return 0;
}


