﻿#define _CRT_SECURE_NO_WARNINGS 0

#include "pch.h"
#include <string>
#include <map>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <Windows.h>
#include <MinHook.h>
#include "Functions.h"

std::map<std::wstring, std::wstring> appRunnerFileMaps;
FuncCreateFileA originalCreateFileA;
FuncCreateFileW originalCreateFileW;
FuncOpenFile originalOpenFile;
FuncZwCreateFile originalZwCreateFile;
FuncZwOpenFile originalZwOpenFile;

std::wstring AnsiToUnicode(const std::string &str) {
    int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    wchar_t *strUnicode = new wchar_t[len];
    wmemset(strUnicode, 0, len);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, strUnicode, len);

    std::wstring strTemp(strUnicode);
    delete[]strUnicode;
    strUnicode = NULL;
    return strTemp;
}

std::string UnicodeToAnsi(const std::wstring &str) {

    int len = WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
    char *strAnsi = new char[len];
    memset(strAnsi, 0, len);
    WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, strAnsi, len, NULL, NULL);

    std::string strTemp(strAnsi);
    delete[]strAnsi;
    strAnsi = NULL;
    return strTemp;
}

void MakeFileFullPath(std::wstring &str) {
    wchar_t buffer[300];

    auto size = GetFullPathNameW(str.c_str(), 300, buffer, nullptr);
    if (size <= 300) {
        str = std::wstring(buffer);
        return;
    }

    auto bufferOnHeap = new wchar_t[size];
    GetFullPathNameW(str.c_str(), size, bufferOnHeap, nullptr);
    delete[] bufferOnHeap;
}

void MakeStringLower(std::wstring &str) {
    for (size_t i = 0; i < str.length(); i++) {
        str[i] = std::tolower(str[i]);
    }
}

void TrimStart(std::wstring &str, const std::wstring &prefix) {
    if (str.find(prefix, 0) == 0) {
        str = str.substr(prefix.length(), str.length() - prefix.length());
    }
}

bool GetFileMapTarget(const std::wstring &from, std::wstring &target) {
    auto key = from;
    TrimStart(key, std::wstring(LR"(\\?\)"));
    TrimStart(key, std::wstring(LR"(\??\)"));
    MakeFileFullPath(key);
    MakeStringLower(key);

    auto iterator = appRunnerFileMaps.find(key);
    if (iterator != appRunnerFileMaps.end()) {
        target = iterator->second;
        return true;
    }

    return false;
}

HANDLE HookCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    auto fileName = AnsiToUnicode(std::string(lpFileName));

    std::wstring mapTarget;
    if (GetFileMapTarget(fileName, mapTarget)) {
        fileName = mapTarget;
    }

    auto ansiFileName = UnicodeToAnsi(fileName);
    return originalCreateFileA(ansiFileName.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE HookCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    auto fileName = std::wstring(lpFileName);

    std::wstring mapTarget;
    if (GetFileMapTarget(fileName, mapTarget)) {
        fileName = mapTarget;
    }

    return originalCreateFileW(fileName.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HFILE HookOpenFile(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle) {
    auto fileName = AnsiToUnicode(std::string(lpFileName));

    std::wstring mapTarget;
    if (GetFileMapTarget(fileName, mapTarget)) {
        fileName = mapTarget;
    }

    auto ansiFileName = UnicodeToAnsi(fileName);
    return originalOpenFile(ansiFileName.c_str(), lpReOpenBuff, uStyle);
}

NTSTATUS NTAPI HookZwCreateFile(
    _Out_ PHANDLE FileHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _In_opt_ PLARGE_INTEGER AllocationSize,
    _In_ ULONG FileAttributes,
    _In_ ULONG ShareAccess,
    _In_ ULONG CreateDisposition,
    _In_ ULONG CreateOptions,
    _In_reads_bytes_opt_(EaLength) PVOID EaBuffer,
    _In_ ULONG EaLength
) {
    auto fileName = std::wstring(ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length / 2);

    std::wstring mapTarget;
    if (GetFileMapTarget(fileName, mapTarget)) {
        mapTarget = LR"(\??\)" + mapTarget;

        ObjectAttributes->ObjectName->Buffer = (wchar_t *)mapTarget.c_str();
        ObjectAttributes->ObjectName->Length = mapTarget.length() * 2;
        ObjectAttributes->ObjectName->MaximumLength = mapTarget.capacity() * 2;
    }

    return originalZwCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, AllocationSize, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, EaBuffer, EaLength);
}

NTSTATUS NTAPI HookZwOpenFile(
    _Out_ PHANDLE FileHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _In_ ULONG ShareAccess,
    _In_ ULONG OpenOptions
) {
    auto fileName = std::wstring(ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length / 2);

    std::wstring mapTarget;
    if (GetFileMapTarget(fileName, mapTarget)) {
        mapTarget = LR"(\??\)" + mapTarget;

        ObjectAttributes->ObjectName->Buffer = (wchar_t *)mapTarget.c_str();
        ObjectAttributes->ObjectName->Length = mapTarget.length() * 2;
        ObjectAttributes->ObjectName->MaximumLength = mapTarget.capacity() * 2;
    }

    return originalZwOpenFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions);
}

void InitializeHooks() {
    auto appRunnerFileMapsPtr = getenv("SlimeNull.AppRunner.FileMaps");

    if (appRunnerFileMapsPtr == nullptr) {
        return;
    }

    auto appRunnerFileMapsString = std::string(appRunnerFileMapsPtr);

    auto stringStream = std::istringstream(appRunnerFileMapsString);
    auto token = std::string();
    auto key = std::wstring();

    while (std::getline(stringStream, token, '|')) {
        if (key.length() == 0) {
            auto unicodeToken = AnsiToUnicode(token);
            MakeFileFullPath(unicodeToken);
            MakeStringLower(unicodeToken);

            key = unicodeToken;
        }
        else {
            auto wValue = AnsiToUnicode(token);
            appRunnerFileMaps[key] = wValue;
            key = std::wstring();
        }
    }

    MH_Initialize();

    MH_CreateHook(&CreateFileA, &HookCreateFileA, (void **)&originalCreateFileA);
    MH_EnableHook(&CreateFileA);

    MH_CreateHook(&CreateFileW, &HookCreateFileW, (void **)&originalCreateFileW);
    MH_EnableHook(&CreateFileW);

    MH_CreateHook(&OpenFile, &HookOpenFile, (void **)&originalOpenFile);
    MH_EnableHook(&OpenFile);

    auto moduleNtdll = GetModuleHandleA("ntdll.dll");

    if (moduleNtdll != nullptr) {
        auto procZwCreateFile = GetProcAddress(moduleNtdll, "ZwCreateFile");
        auto procZwOpenFile = GetProcAddress(moduleNtdll, "ZwOpenFile");

        if (procZwCreateFile != nullptr) {
            MH_CreateHook(procZwCreateFile, &HookZwCreateFile, (void **)&originalZwCreateFile);
            MH_EnableHook(procZwCreateFile);
        }


        if (procZwOpenFile != nullptr) {
            MH_CreateHook(procZwOpenFile, &HookZwOpenFile, (void **)&originalZwOpenFile);
            MH_EnableHook(procZwOpenFile);
        }
    }
}

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        InitializeHooks();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

