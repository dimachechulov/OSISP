#include <windows.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>

DWORD CHUNK_SIZE = 1024 * 1024;

struct ThreadData {
    HANDLE file;
    DWORD offset;
    DWORD size;
};
void processData(char* data, DWORD size) {
    for (DWORD i = 0; i < size; ++i) {
        data[i] = (data[i] * 4) % 22;
        data[i] = (data[i] * 2) % 28;
        data[i] = (data[i] * 10) % 34;
        data[i] = (data[i] * 23) % 23;
    }
}

DWORD WINAPI ReadAndProcessData(LPVOID param) {
    struct ThreadData {
        HANDLE file;
        DWORD offset;
    };

    ThreadData* data = (ThreadData*)param;
    HANDLE file = data->file;
    DWORD offset = data->offset;
    delete data;

    OVERLAPPED ol = { 0 };
    ol.Offset = offset;

    char* buffer = new char[CHUNK_SIZE];
    DWORD bytesRead = 0;

    if (!ReadFile(file, buffer, CHUNK_SIZE, &bytesRead, &ol)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(file, &ol, &bytesRead, TRUE)) {
                std::cerr << "Error waiting for overlapped result." << std::endl;
            }
        }
        else {
            std::cerr << "Error reading file." << std::endl;
        }
    }

    processData(buffer, bytesRead);

    DWORD bytesWritten = 0;
    if (!WriteFile(file, buffer, bytesRead, &bytesWritten, &ol)) {
        if (GetLastError() == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(file, &ol, &bytesWritten, TRUE)) {
                std::cerr << "Error waiting for overlapped result." << std::endl;
            }
        }
        else {
            std::cerr << "Error writing to file." << std::endl;
        }
    }

    delete[] buffer;
    return 0;
}

void AsyncProcessFile(const WCHAR* filePath) {
    HANDLE file = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file for async processing." << std::endl;
        return;
    }

    DWORD fileSize = GetFileSize(file, NULL);
    std::vector<HANDLE> threads;

    for (DWORD offset = 0; offset < fileSize; offset += CHUNK_SIZE) {
        ThreadData* data = new ThreadData{ file, offset };
        HANDLE thread = CreateThread(NULL, 0, ReadAndProcessData, data, 0, NULL);
        if (thread) {
            threads.push_back(thread);
        }
        else {
            std::cerr << "Error creating thread." << std::endl;
            delete data;
        }
    }

    for (HANDLE thread : threads) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }

    CloseHandle(file);
}

DWORD WINAPI ProcessBuffer(LPVOID param) {
    struct ThreadData {
        char* buffer;
        DWORD offset;
        DWORD size;
    };

    ThreadData* data = (ThreadData*)param;
    processData(data->buffer + data->offset, data->size);
    delete data;
    return 0;
}

void ProcessFile(const WCHAR* filePath) {
    HANDLE file = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file for processing." << std::endl;
        return;
    }

    DWORD fileSize = GetFileSize(file, NULL);
    char* buffer = new char[fileSize];
    DWORD bytesRead = 0;
    ReadFile(file, buffer, fileSize, &bytesRead, NULL);

    std::vector<HANDLE> threads;
    for (DWORD offset = 0; offset < fileSize; offset += CHUNK_SIZE) {
        DWORD size = min(CHUNK_SIZE, fileSize - offset);
        ThreadData* data = new ThreadData{ buffer, offset, size };
        HANDLE thread = CreateThread(NULL, 0, ProcessBuffer, data, 0, NULL);
        if (thread) {
            threads.push_back(thread);
        }
        else {
            std::cerr << "Error creating thread." << std::endl;
            delete data;
        }
    }

    for (HANDLE thread : threads) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }

    SetFilePointer(file, 0, NULL, FILE_BEGIN);
    DWORD bytesWritten = 0;
    WriteFile(file, buffer, fileSize, &bytesWritten, NULL);
    delete[] buffer;
    CloseHandle(file);
}

int main() {
    for (int i = 0; i < 10; i++) {
        const WCHAR* filePath = L"input.txt";
        auto start = std::chrono::high_resolution_clock::now();
        AsyncProcessFile(filePath);
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Asynchronous processing time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms ";

        start = std::chrono::high_resolution_clock::now();
        ProcessFile(filePath);
        end = std::chrono::high_resolution_clock::now();
        std::cout << "Simple processing time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
    }
    return 0;
}
