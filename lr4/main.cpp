#include <iostream>
#include <stdio.h>
#include <typeinfo>
#include <string>
#include <windows.h>
#include <initializer_list>
#include <vector>
#include <format>
#include <windows.h>
#include <iostream>
#include <thread>
#include <queue>
#include <random>

HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
HANDLE stdOutMutex = CreateMutex(NULL, FALSE, TEXT("StdMutex"));
void writeConsole(int x, int y, std::string msg) {
    WaitForSingleObject(stdOutMutex, INFINITE);
    if (stdOut != NULL && stdOut != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        COORD coord;
        coord.X = x;
        coord.Y = y;
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
        WriteConsoleA(stdOut, msg.c_str(), msg.length(), &written, NULL);
    }
    ReleaseMutex(stdOutMutex);
}

int randomNumber() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 4000);
    return dist(gen);
}

struct writerParam {
    unsigned char* dataPtr;
    HANDLE bQMutex;
    std::queue<int>* bQ;
    std::string content;
    int ordinal;
    int block;
};

DWORD WINAPI writerWork(LPVOID writerparam) {
    

        writerParam* data = static_cast<writerParam*>(writerparam);

    WaitForSingleObject(data->bQMutex, INFINITE);
    data->bQ->push(GetCurrentThreadId());
    ReleaseMutex(data->bQMutex);


    writeConsole(30, data->ordinal, "Writer " + std::to_string(data->ordinal) + "(" + std::to_string(data->block) + ") waiting mutex");
    while (1) {
        WaitForSingleObject(data->bQMutex, INFINITE);
        if (data->bQ->front() == GetCurrentThreadId()) {
            ReleaseMutex(data->bQMutex);
            break;
        }
        ReleaseMutex(data->bQMutex);
        Sleep(1);
    }


    //WaitForSingleObject(data->bMutex, INFINITE);
    writeConsole(30, data->ordinal, "Writer " + std::to_string(data->ordinal) + "(" + std::to_string(data->block) + ") took    mutex");
    memcpy(data->dataPtr, data->content.c_str(), data->content.size());
    Sleep(randomNumber());
    writeConsole(30, data->ordinal, "Writer " + std::to_string(data->ordinal) + "(" + std::to_string(data->block) + ") freed   mutex");
    //ReleaseMutex(data->bMutex); 

    WaitForSingleObject(data->bQMutex, INFINITE);
    data->bQ->pop();
    ReleaseMutex(data->bQMutex);

    return 0;
}

struct readerParam {
    unsigned char* dataPtr;
    HANDLE bQMutex;
    std::queue<int>* bQ;
    int ordinal;
    int block;
};

DWORD WINAPI readerWork(LPVOID readerparam) {
    readerParam* data = static_cast<readerParam*>(readerparam);

    WaitForSingleObject(data->bQMutex, INFINITE);
    data->bQ->push(GetCurrentThreadId());
    ReleaseMutex(data->bQMutex);

    writeConsole(0, data->ordinal, "Reader " + std::to_string(data->ordinal) + "(" + std::to_string(data->block) + ") waiting mutex");
    while (1) {
        WaitForSingleObject(data->bQMutex, INFINITE);
        if (data->bQ->front() == GetCurrentThreadId()) {
            ReleaseMutex(data->bQMutex);
            break;
        }
        ReleaseMutex(data->bQMutex);
        Sleep(1); // Avoid busy waiting
    }


    //WaitForSingleObject(data->bMutex, INFINITE);
    writeConsole(0, data->ordinal, "Reader " + std::to_string(data->ordinal) + "(" + std::to_string(data->block) + ") took    mutex");
    writeConsole(100, data->ordinal, (std::string)((char*)data->dataPtr));
    // std::cout << data->dataPtr;
    writeConsole(0, data->ordinal, "Reader " + std::to_string(data->ordinal) + "(" + std::to_string(data->block) + ") freed   mutex");
    //ReleaseMutex(data->bMutex);  

    WaitForSingleObject(data->bQMutex, INFINITE);
    data->bQ->pop();
    ReleaseMutex(data->bQMutex);

    return 0;
}

int main() {

    HANDLE hMapFile;
    //unsigned char* pBuf;
    int totalSize = 16 * 1024;
    int bNum = 16;
    int bSize = totalSize / bNum;

    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        totalSize,
        TEXT("SharedMemory"));

    if (hMapFile == NULL) {
        std::cerr << "No file mapping\n";
        return 0;
    }

    //HANDLE* blocks = new HANDLE[bNum];
    HANDLE* blocksQueuesMutexes = new HANDLE[bNum];
    std::queue<int>* blocksQueues = new std::queue<int>[bNum];
    for (int i = 0; i < bNum; i++) {
        blocksQueuesMutexes[i] = CreateMutex(NULL, FALSE, NULL);
    }
    unsigned char* pBuf;
    // unsigned char** pbuf = new unsigned char*[bNum];
    pBuf = (unsigned char*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
    if (pBuf == NULL) {
        std::cerr << "No file view" << GetLastError() << '\n';
        return 0;
    }
    HANDLE* readers = new HANDLE[bNum];
    HANDLE* readersl = new HANDLE[bNum];
    readerParam* rparams = new readerParam[bNum];
    readerParam* rparamsl = new readerParam[bNum];
    HANDLE* writers = new HANDLE[bNum];
    HANDLE* writersl = new HANDLE[bNum];
    writerParam* wparams = new writerParam[bNum];
    writerParam* wparamsl = new writerParam[bNum];

    for (int i = 0; i < bNum; i++) {
        wparams[i] = { pBuf + (i * bSize),blocksQueuesMutexes[i],&blocksQueues[i],std::to_string(i * 10),(i),i };
        writers[i] = CreateThread(
            NULL,
            0,
            writerWork,
            &(wparams[i]), // should not be null,
            0,
            NULL
        );
        if (writers[i] == NULL) {
            std::cerr << "Failed to create thread " << i << ".\n";
            return 1;
        }

        Sleep(1);
        //std::cout << "creating reader\n";
        rparams[i] = { pBuf + (i * bSize),blocksQueuesMutexes[i],&blocksQueues[i],(i),i };
        readers[i] = CreateThread(
            NULL,
            0,
            readerWork,
            &(rparams[i]), // should not be null,
            0,
            NULL
        );
        if (readers[i] == NULL) {
            std::cerr << "Failed to create thread " << i << ".\n";
            return 1;
        }



        if (i == 0) continue;
        wparamsl[i - 1] = { pBuf + ((i - 1) * bSize),blocksQueuesMutexes[i - 1],&blocksQueues[i - 1],std::to_string((i * 20) + i),(i - 1 + bNum),i - 1 };
        writersl[i - 1] = CreateThread(
            NULL,
            0,
            writerWork,
            &(wparamsl[i - 1]), // should not be null,
            0,
            NULL
        );
        if (writersl[i - 1] == NULL) {
            std::cerr << "Failed to create thread " << i << ".\n";
            return 1;
        }
        Sleep(1);
        rparamsl[i - 1] = { pBuf + ((i - 1) * bSize),blocksQueuesMutexes[i - 1],&blocksQueues[i - 1],(i - 1 + bNum),i - 1 };
        readersl[i - 1] = CreateThread(
            NULL,
            0,
            readerWork,
            &(rparamsl[i - 1]), // should not be null,
            0,
            NULL
        );
        if (readersl[i - 1] == NULL) {
            std::cerr << "Failed to create thread " << i << ".\n";
            return 1;
        }
        wparamsl[i] = { pBuf + ((i) * bSize),blocksQueuesMutexes[i],&blocksQueues[i],std::to_string((i * 20) + i),(i + bNum),i  };
        writersl[i] = CreateThread(
            NULL,
            0,
            writerWork,
            &(wparamsl[i]), // should not be null,
            0,
            NULL
        );
        if (writersl[i] == NULL) {
            std::cerr << "Failed to create thread " << i << ".\n";
            return 1;
        }
        Sleep(1);
        rparamsl[i] = { pBuf + ((i) * bSize),blocksQueuesMutexes[i],&blocksQueues[i],(i+bNum),i};
        readersl[i] = CreateThread(
            NULL,
            0,
            readerWork,
            &(rparamsl[i]), // should not be null,
            0,
            NULL
        );
        if (readersl[i] == NULL) {
            std::cerr << "Failed to create thread " << i << ".\n";
            return 1;
        }


    }


    writeConsole(0, (bNum * 2) + 2, "Waiting");
    //for (int i = 0; i < bNum; i++) {
    //    WaitForSingleObject(readers[i], INFINITE);
    //    WaitForSingleObject(readersl[i], INFINITE);
    //    WaitForSingleObject(writers[i], INFINITE);
    //    WaitForSingleObject(writersl[i], INFINITE);
    //   

    //}
    WaitForMultipleObjects(bNum, readers, TRUE, INFINITE);
    WaitForMultipleObjects(bNum, readersl, TRUE, INFINITE);
    WaitForMultipleObjects(bNum, writers, TRUE, INFINITE);
    WaitForMultipleObjects(bNum, writersl, TRUE, INFINITE);
    writeConsole(0, (bNum * 2) + 3, "Cleaning");
    for (int i = 0; i < bNum; i++) {
        CloseHandle(blocksQueuesMutexes[i]);
        CloseHandle(writers[i]);
        CloseHandle(writersl[i]);
        CloseHandle(readers[i]);
        CloseHandle(readersl[i]);
       
    }
    delete[] blocksQueuesMutexes;
    delete[] rparams;
    delete[] wparams;
    delete[] wparamsl;
    delete[] rparamsl;
    delete[] blocksQueues;


    writeConsole(0, (bNum * 2) + 4, "Done      ");

}
