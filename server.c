// server_win.c - basit broadcast chat server (Windows / Winsock)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib") // Winsock kütüphanesi

#define PORT 5000
#define MAX_CLIENTS  FD_SETSIZE
#define BUF_SIZE 1024

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        printf("WSAStartup başarısız\n");
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        printf("Socket oluşturulamadı: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(listenSock, (struct sockaddr*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
        printf("Bind hatası: %d\n", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, 10) == SOCKET_ERROR) {
        printf("Listen hatası: %d\n", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    SOCKET clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i] = INVALID_SOCKET;

    fd_set allset, rset;
    FD_ZERO(&allset);
    FD_SET(listenSock, &allset);
    int maxfd = (int)listenSock;

    printf("Server dinlemede: port %d\n", PORT);

    char buf[BUF_SIZE];

    while (1) {
        rset = allset;
        if (select(maxfd + 1, &rset, NULL, NULL, NULL) == SOCKET_ERROR) {
            printf("Select hatası: %d\n", WSAGetLastError());
            break;
        }

        // Yeni bağlantı
        if (FD_ISSET(listenSock, &rset)) {
            struct sockaddr_in cliaddr;
            int clilen = sizeof(cliaddr);
            SOCKET connSock = accept(listenSock, (struct sockaddr*)&cliaddr, &clilen);
            if (connSock == INVALID_SOCKET) {
                printf("Accept hatası: %d\n", WSAGetLastError());
                continue;
            }
            printf("Yeni bağlantı: %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

            // Boş yeri bul
            int added = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i] == INVALID_SOCKET) {
                    clients[i] = connSock;
                    added = 1;
                    break;
                }
            }
            if (!added) {
                printf("Çok fazla client\n");
                closesocket(connSock);
            } else {
                FD_SET(connSock, &allset);
                if ((int)connSock > maxfd) maxfd = (int)connSock;
            }
        }

        // Mevcut istemcilerden veri oku
        for (int i = 0; i < MAX_CLIENTS; i++) {
            SOCKET sockfd = clients[i];
            if (sockfd == INVALID_SOCKET) continue;

            if (FD_ISSET(sockfd, &rset)) {
                int n = recv(sockfd, buf, BUF_SIZE-1, 0);
                if (n <= 0) {
                    printf("Client kapandı (fd=%d)\n", (int)sockfd);
                    closesocket(sockfd);
                    FD_CLR(sockfd, &allset);
                    clients[i] = INVALID_SOCKET;
                } else {
                    buf[n] = '\0';
                    // Mesajı diğer clientlara gönder
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        SOCKET outfd = clients[j];
                        if (outfd != INVALID_SOCKET && outfd != sockfd) {
                            send(outfd, buf, n, 0);
                        }
                    }
                    printf("Mesaj (fd=%d): %s", (int)sockfd, buf);
                }
            }
        }
    }

    // Temizlik
    for (int i = 0; i < MAX_CLIENTS; i++) if (clients[i] != INVALID_SOCKET) closesocket(clients[i]);
    closesocket(listenSock);
    WSACleanup();
    return 0;
}
