// simple HTTP server for testing purposes

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

int main() {
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
    std::cerr << "WSAStartup failed" << std::endl;
    return 1;
  }

  SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_sock == INVALID_SOCKET) {
    std::cerr << "socket() failed: " << WSAGetLastError() << std::endl;
    WSACleanup();
    return 1;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

  sockaddr_in serv{};
  serv.sin_family = AF_INET;
  serv.sin_addr.s_addr = htonl(INADDR_ANY);
  serv.sin_port = htons(8080);

  if (bind(listen_sock, (sockaddr*)&serv, sizeof(serv)) == SOCKET_ERROR) {
    std::cerr << "bind() failed: " << WSAGetLastError() << std::endl;
    closesocket(listen_sock);
    WSACleanup();
    return 1;
  }

  if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
    std::cerr << "listen() failed: " << WSAGetLastError() << std::endl;
    closesocket(listen_sock);
    WSACleanup();
    return 1;
  }

  std::cout << "Simple HTTP server running on http://localhost:8080/ (CTRL+C to stop)" << std::endl;

  for (;;) {
    sockaddr_in client_addr{};
    int addrlen = sizeof(client_addr);
    SOCKET client = accept(listen_sock, (sockaddr*)&client_addr, &addrlen);
    if (client == INVALID_SOCKET) {
      std::cerr << "accept() failed: " << WSAGetLastError() << std::endl;
      break;
    }

    char buffer[4096];
    int received = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
      buffer[received] = '\0';
      // Minimal request logging (first line)
      std::string req(buffer);
      auto pos = req.find("\r\n");
      if (pos != std::string::npos) req = req.substr(0, pos);
      std::cout << "Request: " << req << std::endl;

      const char body[] = "Hello World!";
      std::string resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(sizeof(body)-1) + "\r\n"
        "Connection: close\r\n\r\n";
      resp.append(body, sizeof(body)-1);

      send(client, resp.c_str(), (int)resp.size(), 0);
    }

    closesocket(client);
  }

  closesocket(listen_sock);
  WSACleanup();
  return 0;
}
