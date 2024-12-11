#include <iostream>
#include <stdio.h>
#include <typeinfo>
#include <string>
#include <windows.h>
#include <initializer_list>
#include <vector>

#include <thread>
#include "main3structs.h"
#include <vector>
#include <windows.h>
#include <iostream>
#include <string.h> 

char usr[20];
char dst[20];

OVERLAPPED oRead = { 0 };
OVERLAPPED oWrite = { 0 };
short id = 0;

void init(HANDLE hPipe){
    msg message {"usr","dst",Connected,"msg",0};
    strcpy_s(message.usr,sizeof(message.usr),usr);
    DWORD bytesWritten;
    BOOL result = WriteFile(
            hPipe,
            &message,
            sizeof(msg),
            &bytesWritten,
            &oWrite
        );
        if (!result && GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(oWrite.hEvent, INFINITE);
            GetOverlappedResult(hPipe, &oWrite, &bytesWritten, FALSE);
        }
}

void read(HANDLE hPipe){
    while(true){
        msg buffer;
        DWORD bytesRead;
        BOOL result = ReadFile(
            hPipe,
            &buffer,
            sizeof(msg),
            &bytesRead,
            &oRead
        );
       
        if( GetLastError() == 109 ){
            continue;
        }
        if (!result && GetLastError() == ERROR_IO_PENDING) {
            WaitForSingleObject(oRead.hEvent, INFINITE);
            GetOverlappedResult(hPipe, &oRead, &bytesRead, FALSE);
        }

        if (buffer.cmd == Receice_message) {
            std::cout << "New message: " << buffer.usr << ':' << buffer.msg << std::endl;
        } 
    }
}

void write(HANDLE hPipe){

    while(true){
        msg message {"usr","dst",Send_message,"Hello, Pipe!",id++};
        char m[20];

        DWORD bytesWritten;
        std::cout <<"Send message";
        std::cin >> m;
        strcpy_s(message.msg,sizeof(message.msg), m);
        strcpy_s(message.usr, sizeof(message.usr), usr);
        strcpy_s(message.dst, sizeof(message.dst),dst);
        BOOL result = WriteFile(
            hPipe,
            &message,
            sizeof(msg),
            &bytesWritten,
            &oWrite
        );
        if( GetLastError() == 109 ){
            continue;
        }
        if (!result && GetLastError() == ERROR_IO_PENDING) {
                WaitForSingleObject(oWrite.hEvent, INFINITE);
                GetOverlappedResult(hPipe, &oWrite, &bytesWritten, FALSE);
        }
    }
}

int main() {
    HANDLE hPipe = CreateFile(
        L"\\\\.\\pipe\\MyNamedPipe",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED ,
        nullptr
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFile failed, GLE=" << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "Connected to server." << std::endl;
    std::cout << "You name: ";
    std::cin >> usr;
    std::cout << "Who do you want to send a message to: ";
    std::cin >> dst;

    oRead.Offset=sizeof(msg)+10;
    oRead.hEvent=CreateEvent(NULL, TRUE, FALSE, NULL);
    
    oWrite.Offset=0;
    oWrite.hEvent=CreateEvent(NULL, TRUE, FALSE, NULL);


    
    init(hPipe);
    std::thread writeThread(write, hPipe);
    writeThread.detach();
    std::thread readThread(read, hPipe);
    readThread.detach();


    while(true){}

    CloseHandle(hPipe);
    return 0;
}
