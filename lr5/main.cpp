#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "main5headers.h"
#include <thread>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 1024

struct client {
    std::string name;
    SOCKET clientSockets;
    sockaddr_in clientAddrs;
};

std::vector<client> clients;

void RelayMessage(std::string client, Message* msg) {
    for (int i = 0; i < clients.size(); i++) {
        // std::cout <<clients[i].name <<'\n';
        if (clients[i].name == client) {
            std::cout << "Sending to " << clients[i].name << '\n';
            send(clients[i].clientSockets, (char*)msg, 120, 0);
            return;
        }
    }
    std::cout << "NOT FOUND\n";
}

void HandleClient(SOCKET socket, sockaddr_in adress) {
    char buffer[120];
    std::string name;
    //std::cout<<"waiting";
    bool init = 0;
    while (1) {
        memset(buffer, 0, 120);
        int recvSize = recv(socket, buffer, 120, 0);
        if (recvSize > 0) {

            Message* msg = (Message*)buffer;

            if (init == 0) {
                client cl;
                cl.clientAddrs = adress;
                cl.clientSockets = socket;
                cl.name = msg->sender;
                name = msg->sender;
                clients.push_back(cl);
                init = 1;
            }

            std::cout << "Client " << msg->sender << " |Receiver " << msg->receiver << " |Content " << msg->content << '\n';
            // Echo message back to client
            if (strcmp(msg->content, "INITIAL CONNECT"))
                RelayMessage(msg->receiver, msg);
            //send(socket, buffer, recvSize, 0);
        }
        else {
            printf("Client disconnected or error occurred. Error Code: %d\n", WSAGetLastError());
            for (int i = 0; i < clients.size(); i++) {
                std::cout << clients[i].name << '\n';
                if (clients[i].name == name) {
                    clients.erase(clients.begin() + i);
                }
            }
            break;
        }
    }
}

int main() {
    WSADATA wsaData;
    SOCKET serverSocket;
    sockaddr_in serverAddr;

    char buffer[BUFFER_SIZE];
    int clientAddrSize = sizeof(sockaddr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Setup server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for connections
    while (true) {

        listen(serverSocket, SOMAXCONN);
        printf("Waiting for connections...\n");

        // Accept a client socket
        SOCKET tempSocket;
        sockaddr_in tempAddr;
        tempSocket = accept(serverSocket, (struct sockaddr*)&tempAddr, &clientAddrSize);
        if (tempSocket == INVALID_SOCKET) {
            printf("Accept failed. Error Code: %d\n", WSAGetLastError());
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }

        printf("Client connected.\n");

        std::thread clientThread;
        clientThread = std::thread(HandleClient, tempSocket, tempAddr);
        clientThread.detach();
    }

    // Cleanup
    for (int i = 0; i < clients.size(); i++) {
        closesocket(clients[i].clientSockets);
    }
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
