#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <wininet.h>
#include <shlobj.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

static void ShowMessage(const wchar_t* text, UINT flags) {
    MessageBoxW(NULL, text, L"Roblox Renewal", flags);
}

static void ClearReadOnlyAndDeleteTree(const wchar_t* directory) {
    wchar_t pattern[MAX_PATH];
    lstrcpynW(pattern, directory, MAX_PATH);
    int length = lstrlenW(pattern);
    if (length > 0 && pattern[length - 1] != L'\\' &&
        pattern[length - 1] != L'/') lstrcatW(pattern, L"\\");
    if (length + 3 >= MAX_PATH) return;
    lstrcatW(pattern, L"*");

    WIN32_FIND_DATAW findData;
    HANDLE find = FindFirstFileW(pattern, &findData);
    if (find == INVALID_HANDLE_VALUE) {
        RemoveDirectoryW(directory);
        return;
    }

    do {
        if (lstrcmpW(findData.cFileName, L".") == 0 ||
            lstrcmpW(findData.cFileName, L"..") == 0) continue;

        wchar_t child[MAX_PATH];
        wsprintfW(child, L"%s\\%s", directory, findData.cFileName);
        DWORD attributes = GetFileAttributesW(child);
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            ClearReadOnlyAndDeleteTree(child);
        } else {
            if (attributes != INVALID_FILE_ATTRIBUTES &&
                (attributes & FILE_ATTRIBUTE_READONLY) != 0) {
                SetFileAttributesW(child, attributes & ~FILE_ATTRIBUTE_READONLY);
            }
            DeleteFileW(child);
        }
    } while (FindNextFileW(find, &findData));
    FindClose(find);

    SetFileAttributesW(directory, FILE_ATTRIBUTE_NORMAL);
    RemoveDirectoryW(directory);
}

static bool NameContainsRoblox(const wchar_t* name) {
    wchar_t lowerName[MAX_PATH];
    lstrcpynW(lowerName, name, MAX_PATH);
    CharLowerW(lowerName);
    return wcsstr(lowerName, L"roblox") != NULL;
}

static void TerminateRobloxProcesses(void) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    DWORD selfId = GetCurrentProcessId();
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID != selfId &&
                NameContainsRoblox(entry.szExeFile)) {
                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                if (process != NULL) {
                    TerminateProcess(process, 1);
                    CloseHandle(process);
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

static bool DownloadInstaller(const wchar_t* destination) {
    HINTERNET session = InternetOpenW(L"RobloxRenewal/1.0",
                                      INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (session == NULL) return false;

    HINTERNET request = InternetOpenUrlW(
        session,
        L"https://www.roblox.com/download/client?arch=64",
        NULL, 0,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
        0);
    if (request == NULL) {
        InternetCloseHandle(session);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                   &status, &statusSize, NULL);
    if (status != 200 && status != 206) {
        InternetCloseHandle(request);
        InternetCloseHandle(session);
        return false;
    }

    HANDLE output = CreateFileW(destination, GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(request);
        InternetCloseHandle(session);
        return false;
    }

    char buffer[65536];
    DWORD available;
    BOOL result = TRUE;
    while (InternetReadFile(request, buffer, sizeof(buffer), &available) &&
           available != 0) {
        DWORD written = 0;
        if (!WriteFile(output, buffer, available, &written, NULL) ||
            written != available) {
            result = FALSE;
            break;
        }
    }
    CloseHandle(output);
    InternetCloseHandle(request);
    InternetCloseHandle(session);

    if (!result) return false;
    HANDLE file = CreateFileW(destination, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return false;
    char signature[2] = {0, 0};
    DWORD read = 0;
    ReadFile(file, signature, 2, &read, NULL);
    CloseHandle(file);
    return read >= 2 && signature[0] == 'M' && signature[1] == 'Z';
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR commandLine, int showCommand) {
    (void)instance;
    (void)previous;
    (void)commandLine;
    (void)showCommand;

    if (MessageBoxW(NULL,
        L"Roblox Renewal will close Roblox processes and permanently remove:\n\n"
        L"%LOCALAPPDATA%\\Roblox\n%TEMP%\\Roblox\n\n"
        L"Continue?",
        L"Roblox Renewal",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return 0;

    TerminateRobloxProcesses();
    Sleep(750);

    wchar_t localAppData[MAX_PATH];
    wchar_t tempPath[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    GetTempPathW(MAX_PATH, tempPath);

    wchar_t localTarget[MAX_PATH];
    wchar_t tempTarget[MAX_PATH];
    wsprintfW(localTarget, L"%s\\Roblox", localAppData);
    wsprintfW(tempTarget, L"%sRoblox", tempPath);
    ClearReadOnlyAndDeleteTree(localTarget);
    ClearReadOnlyAndDeleteTree(tempTarget);

    wchar_t workDirectory[MAX_PATH];
    wsprintfW(workDirectory, L"%sRobloxRenewal", tempPath);
    CreateDirectoryW(workDirectory, NULL);
    wchar_t installer[MAX_PATH];
    wsprintfW(installer, L"%s\\RobloxPlayerInstaller.exe", workDirectory);
    DeleteFileW(installer);

    if (!DownloadInstaller(installer)) {
        ShowMessage(L"The official Roblox installer could not be downloaded. Check your connection.",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    SHELLEXECUTEINFOW execute;
    ZeroMemory(&execute, sizeof(execute));
    execute.cbSize = sizeof(execute);
    execute.lpFile = installer;
    execute.lpVerb = L"open";
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        ShowMessage(L"The installer downloaded successfully, but Windows could not launch it.",
                    MB_OK | MB_ICONERROR);
        return 2;
    }

    ShowMessage(L"Roblox has been reset and the newest official installer was launched.",
                MB_OK | MB_ICONINFORMATION);
    return 0;
}
