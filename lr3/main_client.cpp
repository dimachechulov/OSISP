#include <iostream>
#include <stdio.h>
#include <typeinfo>
#include <string>
#include <windows.h>
#include <initializer_list>
#include <vector>
#include <format>
#include <thread>
#include <mutex>
#include "main3structs.h"

#define _CRT_SECURE_NO_WARNINGS

OVERLAPPED oRead = { 0 };
OVERLAPPED oWrite = { 0 };
short id = 0;
std::vector<usrpipe> users;
std::mutex usersMutex;

void SendMessage(std::string from, std::string to, short message_id, std::string contents) {
    std::lock_guard<std::mutex> lock(usersMutex); // Lock the mutex for thread-safe access
    for (int i = 0; i < users.size(); i++) {
        if (users[i].usr == from) {
            for (auto& usr : users) {
                if (usr.usr == to) {
                    if (users[i].sended_messages <= message_id) {
                        std::cout << "Delivering to " << to << '\n';
                        msg message{ "usr", "dst", Receice_message, "msg", id++ };
                        strcpy_s(message.usr, sizeof(message.usr), from.c_str());
                        strcpy_s(message.msg, sizeof(message.msg), contents.c_str());
                        DWORD bytesWritten;
                        BOOL result = WriteFile(
                            usr.pipe,
                            &message,
                            sizeof(msg),
                            &bytesWritten,
                            &oWrite
                        );
                        users[i].sended_messages += 1;
                        if (!result && GetLastError() == ERROR_IO_PENDING) {
                            WaitForSingleObject(oWrite.hEvent, INFINITE);
                            GetOverlappedResult(usr.pipe, &oWrite, &bytesWritten, FALSE);
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
}

void HandleClient(HANDLE hPipe) {
    while (true) {
        msg buffer;
        DWORD bytesRead;
        BOOL result = ReadFile(
            hPipe,
            &buffer,
            sizeof(msg),
            &bytesRead,
            &oRead
        );

        if (!result && GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(oRead.hEvent, INFINITE);
            GetOverlappedResult(hPipe, &oRead, &bytesRead, FALSE);
        }
        if (GetLastError() == ERROR_BROKEN_PIPE) {

            std::lock_guard<std::mutex> lock(usersMutex); // Lock the mutex for thread-safe access
            for (int i = 0; i < users.size(); i++) {
                if (users[i].pipe == hPipe) {
                    std::cout << "Byu " << users[i].usr << '\n';
                    users.erase(users.begin() + i);
                    break;
                }
            }
            break;
        }
        switch (buffer.cmd) {
        case Connected:
        {
            std::lock_guard<std::mutex> lock(usersMutex); // Lock the mutex for thread-safe access
            users.push_back({ hPipe, buffer.usr, 0 });
            std::cout << "new user: " << buffer.usr << '\n';
        }
        break;

        case Send_message:
            SendMessage(buffer.usr, buffer.dst, buffer.id, buffer.msg);
            break;
        }
    }

    CloseHandle(hPipe);
}

int main() {
    const WCHAR* pipeName = L"\\\\.\\pipe\\MyNamedPipe";

    oRead.Offset = 0;
    oRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (oRead.hEvent == NULL) {
        std::cerr << "CreateEvent failed for oRead, GLE=" << GetLastError() << std::endl;
        return 1;
    }

    oWrite.Offset = sizeof(msg) + 10;
    oWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (oWrite.hEvent == NULL) {
        std::cerr << "CreateEvent failed for oWrite, GLE=" << GetLastError() << std::endl;
        CloseHandle(oRead.hEvent);
        return 1;
    }

    while (true) {
        HANDLE hPipe = CreateNamedPipe(
            pipeName,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            1024 * 16,
            1024 * 16,
            0,
            nullptr
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "CreateNamedPipe failed, GLE=" << GetLastError() << std::endl;
            CloseHandle(oRead.hEvent);
            CloseHandle(oWrite.hEvent);
            return 1;
        }

        std::cout << "Waiting for client to connect..." << std::endl;
        BOOL connected = ConnectNamedPipe(hPipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            std::thread clientThread(HandleClient, hPipe);
            clientThread.detach();
        }
        else {
            CloseHandle(hPipe);
        }
    }

    CloseHandle(oRead.hEvent);
    CloseHandle(oWrite.hEvent);
    return 0;
}
