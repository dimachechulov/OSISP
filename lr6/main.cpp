#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>

void ExportRegistryKey(HKEY hKey, const std::wstring& subKey, const std::wstring& searchValue, bool includeSubkeys, std::wofstream* regFile = nullptr);
void ExportSubkeys(HKEY hKey, const std::wstring& subKey, const std::wstring& searchValue, bool includeSubkeys, std::wofstream* regFile);

int main() {
    std::wstring keyPath, valueName, outputPath;
    bool includeSubkeys;

    std::wcout << L"Enter the registry key path: ";
    std::getline(std::wcin, keyPath);

    std::wcout << L"Enter the registry value name (optional): ";
    std::getline(std::wcin, valueName);

    std::wcout << L"Include subkeys? (1/0): ";
    std::wcin >> includeSubkeys;
    std::wcin.ignore();

    std::wcout << L"Enter the output file path for .reg file (optional): ";
    std::getline(std::wcin, outputPath);

    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        std::wcout << L"Registry Key Opened Successfully.\n";
        std::wofstream regFile;
        if (!outputPath.empty()) {
            regFile.open(outputPath);
            if (!regFile.is_open()) {
                std::wcerr << L"Failed to open output file.\n";
                return 1;
            }
            regFile << L"Windows Registry Editor Version 5.00\n\n";
        }
        ExportRegistryKey(hKey, keyPath, valueName, includeSubkeys, outputPath.empty() ? nullptr : &regFile);
        if (regFile.is_open()) {
            regFile.close();
            std::wcout << L"Registry exported to " << outputPath << L"\n";
        }
        RegCloseKey(hKey);
    }
    else {
        std::wcerr << L"Failed to open registry key.\n";
    }

    return 0;
}

void ExportRegistryKey(HKEY hKey, const std::wstring& subKey, const std::wstring& searchValue, bool includeSubkeys, std::wofstream* regFile) {
    DWORD index = 0;
    WCHAR valueName[16383];
    DWORD valueNameSize;
    BYTE data[16383];
    DWORD dataSize;
    DWORD type;

    bool printedKeyName = false;
    if (regFile) {
        *regFile << L"[" << subKey << L"]\n";
    }

    while (true) {
        valueNameSize = sizeof(valueName) / sizeof(valueName[0]);
        dataSize = sizeof(data);
        LONG result = RegEnumValue(hKey, index, valueName, &valueNameSize, NULL, &type, data, &dataSize);

        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }
        else if (result != ERROR_SUCCESS) {
            std::wcerr << L"Error enumerating values.\n";
            break;
        }

        if (searchValue.empty() || searchValue == valueName) {
            if (!printedKeyName) {
                printedKeyName = true;
                std::wcout << subKey << L"\n";
            }
            std::wcout << L"Value Name: " << valueName << L"\n";
            std::wcout << L"Data: ";
            if (regFile) {
                *regFile << L"\"" << valueName << L"\"=";
            }

            switch (type) {
            case REG_SZ:
                std::wcout << reinterpret_cast<WCHAR*>(data) << L"\n";
                if (regFile) {
                    *regFile << L"\"" << reinterpret_cast<WCHAR*>(data) << L"\"\n";
                }
                break;
            case REG_DWORD:
                std::wcout << *reinterpret_cast<DWORD*>(data) << L"\n";
                if (regFile) {
                    *regFile << L"dword:" << std::hex << *reinterpret_cast<DWORD*>(data) << L"\n";
                }
                break;
            default:
                std::wcout << L"Unknown type\n";
                if (regFile) {
                    *regFile << L"unknown\n";
                }
                break;
            }
        }
        index++;
    }

    if (includeSubkeys) {
        ExportSubkeys(hKey, subKey, searchValue, includeSubkeys, regFile);
    }
}

void ExportSubkeys(HKEY hKey, const std::wstring& subKey, const std::wstring& searchValue, bool includeSubkeys, std::wofstream* regFile) {
    DWORD index = 0;
    WCHAR subKeyName[256];
    DWORD subKeyNameSize;
    HKEY hSubKey;

    while (true) {
        subKeyNameSize = sizeof(subKeyName) / sizeof(subKeyName[0]);
        LONG result = RegEnumKeyEx(hKey, index, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL);

        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }
        else if (result != ERROR_SUCCESS) {
            std::wcerr << L"Error enumerating subkeys.\n";
            break;
        }

        std::wstring fullSubKeyPath = subKey + L"\\" + subKeyName;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, fullSubKeyPath.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
            ExportRegistryKey(hSubKey, fullSubKeyPath, searchValue, includeSubkeys, regFile);
            RegCloseKey(hSubKey);
        }
        index++;
    }
}
