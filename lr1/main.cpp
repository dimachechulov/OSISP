#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include <codecvt>
#include <vector>
#include <atomic>
#include <sstream>
#include <iostream>
#include <string>
#include <functional>
#pragma comment(lib, "comctl32.lib")
#include <algorithm>

// Global config
int arraySize = 100000, numThreads = 5;

// Global variables for the GUI elements
HWND hWnd;
HWND hCPUUsage;
HWND hTimeTaken;
HWND hButton;
HWND* ThreadLabels;

// Structure to represent a fragment of data
struct Fragment {
    int start;   // Starting index of the fragment
    int end;     // Ending index of the fragment
    std::vector<int> data; // Data within the fragment
    bool processed;  // Flag to indicate if the fragment is processed
    HANDLE threadHandle; // Handle of the thread processing the fragment
};

// Function to split a command line string into arguments
std::vector<std::wstring> splitCommandLine(const std::wstring& commandLine) {
    std::vector<std::wstring> args;
    std::wistringstream stream(commandLine);
    std::wstring arg;

    while (stream >> arg) {
        args.push_back(arg);
    }

    return args;
}

// Thread function to process a fragment of data
DWORD WINAPI processFragment(LPVOID lpParam) {
    Fragment* fragment = (Fragment*)lpParam;
    DWORD threadId = GetCurrentThreadId();
    HWND hThreadLabel = ThreadLabels[fragment->start / (arraySize / numThreads)];

    // Simulate processing data (Sort the fragment)
    std::wstring StartStr = L"Thread " + std::to_wstring(threadId) + L" Start";
    SetWindowText(hThreadLabel, StartStr.c_str());
    int n = fragment->data.size();
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (fragment->data[j] > fragment->data[j + 1]) {
                std::swap(fragment->data[j], fragment->data[j + 1]);
            }
        }
    }
    std::wstring DoneStr = L"Thread " + std::to_wstring(threadId) + L" Done";
    SetWindowText(hThreadLabel, DoneStr.c_str());

    fragment->processed = true;

    return 0;
}

// Global variables to manage threads
std::vector<Fragment> fragments;
LARGE_INTEGER startTime, endTime;

// Function to update the progress bar and CPU usage in the GUI
void updateGUI() {
    // Update progress bar (not implemented in this example)

    HANDLE hProcess = GetCurrentProcess();
    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    FILETIME ftSysIdle, ftSysKernel, ftSysUser;
        // Get process times
    if (!GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        return;
    }

    // Get system times
    if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser)) {
        return;
    }

    // Convert FILETIME to 64-bit values
    ULARGE_INTEGER ulSysKernel, ulSysUser, ulProcKernel, ulProcUser;
    ulSysKernel.LowPart = ftSysKernel.dwLowDateTime;
    ulSysKernel.HighPart = ftSysKernel.dwHighDateTime;
    ulSysUser.LowPart = ftSysUser.dwLowDateTime;
    ulSysUser.HighPart = ftSysUser.dwHighDateTime;

    ulProcKernel.LowPart = ftKernel.dwLowDateTime;
    ulProcKernel.HighPart = ftKernel.dwHighDateTime;
    ulProcUser.LowPart = ftUser.dwLowDateTime;
    ulProcUser.HighPart = ftUser.dwHighDateTime;

    // Calculate CPU usage
    ULONGLONG sysTime = ulSysKernel.QuadPart + ulSysUser.QuadPart;
    ULONGLONG procTime = ulProcKernel.QuadPart + ulProcUser.QuadPart;

    static ULONGLONG lastSysTime = 0, lastProcTime = 0;
    double cpuUsage = 0.0;

    if (lastSysTime != 0 && lastProcTime != 0) {
        cpuUsage = (double)(procTime - lastProcTime) / (sysTime - lastSysTime) * 100.0;
        std::wstring usageStr = L" CPU Load: " + std::to_wstring(cpuUsage) + L"%";
        SetWindowText(hCPUUsage, usageStr.c_str());
    }

    lastSysTime = sysTime;
    lastProcTime = procTime;
}

// Function to restart the processing threads
void RestartThreads() {
    // Reset the processed flag for each fragment
    SetWindowText(hTimeTaken, L"");
    //EnableWindow(hButton, FALSE);
    QueryPerformanceCounter(&startTime);

    // Create and start threads
    for (int i = 0; i < numThreads; ++i) {
        fragments[i].processed = false;
        fragments[i].threadHandle = CreateThread(
            NULL, // Default security attributes
            0, // Default stack size
            processFragment, // Thread function
            &fragments[i], // Argument to thread function
            0, // Default creation flags
            NULL // Thread ID (not used)
        );
        if (fragments[i].threadHandle == NULL) {
            // Handle error
            std::cerr << "Error creating thread: " << GetLastError() << std::endl;
            return;
        }
    }
}

// Window procedure for the main window
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {

    case WM_CREATE:
        // Create button control
        hButton = CreateWindow(L"BUTTON", L"Restart", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  
            210, 10, 200, 30,
            hWnd,
            (HMENU)1,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            NULL);
        EnableWindow(hButton, FALSE);

        // Create CPU usage label
        hCPUUsage = CreateWindowEx(WS_EX_CLIENTEDGE, _T("STATIC"), _T(""),
            WS_CHILD | WS_VISIBLE, 10, 10, 200, 20, hWnd, (HMENU)2, GetModuleHandle(NULL), NULL);

        // Create time taken label
        hTimeTaken = CreateWindowEx(WS_EX_CLIENTEDGE, _T("STATIC"), _T(""),
            WS_CHILD | WS_VISIBLE,
            10, 40, 200, 20, hWnd, (HMENU)3, GetModuleHandle(NULL), NULL);

        // Create the Thread labels
        ThreadLabels = new HWND[numThreads];
        for (int i = 0; i < numThreads; ++i) {
            ThreadLabels[i] = CreateWindowEx(WS_EX_CLIENTEDGE, _T("STATIC"), _T(""),
                WS_CHILD | WS_VISIBLE,
                10, 70 + i * 30, 200, 20, hWnd, (HMENU)(4 + i), GetModuleHandle(NULL), NULL);
        }

        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // Button ID is 1
            // Restart the threads
            RestartThreads();
        }
        break;

    case WM_TIMER:
        updateGUI();
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        // Clean up thread handles
        for (auto& fragment : fragments) {
            if (fragment.threadHandle != NULL) {
                CloseHandle(fragment.threadHandle);
            }
        }
        PostQuitMessage(0);
        return 0;
     default:
        bool allProcessed = true;
        for (const auto& fragment : fragments) {
            if (!fragment.processed) {
                allProcessed = false;
                break;
            }
        }
        if (allProcessed) {
            // Stop timer and calculate time
            QueryPerformanceCounter(&endTime);
            LARGE_INTEGER frequency;
            QueryPerformanceFrequency(&frequency);
            auto duration = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;

            // Display total execution time in GUI
            std::wstring timeTakenStr = L"Time Taken: " + std::to_wstring(duration) + L" ms";
            SetWindowText(hTimeTaken, timeTakenStr.c_str());
            for (auto& fragment : fragments) {
                fragment.processed = false;
            }
            EnableWindow(hButton, TRUE);
        }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    std::wstring commandLine(lpCmdLine);
    std::vector<std::wstring> args = splitCommandLine(commandLine);
    if (args.size() > 0) {
        arraySize = std::stoi(args[0]);
    }
    if (args.size() > 1) {
        numThreads = std::stoi(args[1]);
    }
    ThreadLabels = new HWND[numThreads];

    // Register window class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = _T("ParallelProcessing");
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, _T("Call to RegisterClassEx failed!"), _T("Error"), MB_OK);
        return 1;
    }

    // Create main window
    hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, _T("ParallelProcessing"), _T("Parallel Processing"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 20 + (numThreads + 4) * 30,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        MessageBox(NULL, _T("Call to CreateWindowEx failed!"), _T("Error"), MB_OK);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    std::vector<int> data(arraySize);
    for (int i = 0; i < arraySize; ++i) {
        data[i] = rand() % 100;
    }

    // Divide the data into fragments
    int fragmentSize = arraySize / numThreads;
    int start = 0;
    for (int i = 0; i < numThreads; ++i) {
        int end = (i == numThreads - 1) ? arraySize : start + fragmentSize;
        fragments.push_back({ start, end, std::vector<int>(data.begin() + start, data.begin() + end), false, NULL });
        start = end;
    }

    // Start timer
    QueryPerformanceCounter(&startTime);

    // Create and start threads (handled in RestartThreads function)
    RestartThreads();

    // Process messages and update GUI
    MSG msg;
    SetTimer(hWnd, 1, 100, NULL);
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);


    }
    return (int)msg.wParam;
}
