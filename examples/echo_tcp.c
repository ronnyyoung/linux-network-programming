#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>  // basename
#include <netdb.h>   // gethostbyname

#include <stdio.h>
#include <stdlib.h>  // atoi
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void do_client(int sock_fd, const struct sockaddr_in *addr) {
  int ret = connect(sock_fd, (struct sockaddr *)addr, sizeof(*addr));
  if (ret != 0) {
    printf("[%d]: %s\n", errno, strerror(errno));
    return;
  }
  printf("Success to connect to server!\n");

  size_t buf_size = 1024;
  char *buf = malloc(buf_size);

  while (1) {
    // 从标准输入中读取一行
    // getline会在换行符'\n'(10)后面，再补一个'\0'(0)
    // 所以不用担心buf里有脏数据导致strlen计算错误的问题
    // 返回的n是算上换行符
    // 如果一行文本的内容大于buf_size，那么getline会realloc
    // 同时更新buf和buf_size的值
    ssize_t n = getline(&buf, &buf_size, stdin);
    buf[n - 1] = '\0';  // 去掉行尾的换行符

    // 设置client的退出机制
    if (strcmp("quit", buf) == 0) {
      break;
    }
    send(sock_fd, buf, n, 0);

    // 以下代码为了测试，一次性服务器写入大量数据，测试send函数的阻塞问题
    // int32_t large_size = 10240000;
    // char *large_buf = malloc(large_size);
    // for (int i = 0; i < large_size; i++) {
    //   large_buf[i] = i % 256;
    // }
    // send(sock_fd, large_buf, large_size, 0);
    // printf("sent large size data to server!\n");

    // 从服务端接收数据
    const uint32_t recv_buf_size = 1024;
    char recv_buf[recv_buf_size];
    memset(recv_buf, '\0', recv_buf_size);
    ssize_t n_recv = recv(sock_fd, recv_buf, recv_buf_size, 0);
    if (n_recv == 0) {
      printf("lose connection from server!\n");
      break;
    }
    printf("Recv: [%ld] %s\n", n_recv, recv_buf);
  }
  free(buf);
}

void handle_client_connection(int conn) {
  const uint32_t buf_size = 1024;
  char recv_buf[buf_size];
  while (1) {
    int n = recv(conn, recv_buf, buf_size, 0);
    if (n == 0) {
      break;
    }
    printf("recv: [%d] %s\n", n, recv_buf);
    send(conn, recv_buf, n, 0);
  }
}

void do_server(int sock_fd, const struct sockaddr_in *addr) {
  int on = 1;
  setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  int ret = bind(sock_fd, (struct sockaddr *)(addr), sizeof(*addr));
  if (ret != 0) {
    printf("[%d] %s\n", errno, strerror(errno));
    return;
  }

  // 监听socket
  ret = listen(sock_fd, 5);
  if (ret != 0) {
    printf("[%d] %s\n", errno, strerror(errno));
    return;
  }
  while (1) {
    struct sockaddr_in client_addr;
    socklen_t addr_len;
    int conn = accept(sock_fd, (struct sockaddr *)(&client_addr), &addr_len);
    if (conn == -1) {
      printf("[%d] %s\n", errno, strerror(errno));
      return;
    }
    char client_ip[INET_ADDRSTRLEN];
    memset(client_ip, '\0', INET_ADDRSTRLEN);
    printf("Connection from %s:%d\n",
           inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN),
           ntohs(client_addr.sin_port));
    handle_client_connection(conn);
    printf("Connection closed!\n");
    close(conn);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <role>(server/client) <ip/hostname> <port>\n", basename(argv[0]));
    return -1;
  }

  // 根据程序参数来生成socket的地址
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(argv[3]));  // 端口号也需要转换为网络字节序
  // 根据hostname（点分十进制/域名）获取ip地址（网络字节序）
  struct hostent *he = gethostbyname(argv[2]);
  memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

  // 创建socket
  int sock_fd = socket(PF_INET, SOCK_STREAM, 0);

  if (strcmp(argv[1], "server") == 0) {
    do_server(sock_fd, &addr);
  } else if (strcmp(argv[1], "client") == 0) {
    do_client(sock_fd, &addr);
  } else {
    printf("Usage: %s <role>(server/client) <ip/hostname> <port>\n", basename(argv[0]));
    return -1;
  }

  close(sock_fd);

  return 0;
}