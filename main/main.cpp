#include <windows.h>
#include <iostream>
#include <csignal>
#include <signal.h>
#include <string>
#include <vector>

HANDLE MainProcessHandle = NULL;

std::wstring createBootCommand(std::wstring processName, std::wstring qucikLogin)
{
    std::wstring processNameInternal = L"\"" + processName + L"\"";
    std::wstring commandLine = L"--enable-logging";
    std::wstring realProcessName = processNameInternal;
    realProcessName += L" ";
    realProcessName += commandLine;
    if (qucikLogin.length() > 0)
    {
        realProcessName += L" -q ";
        realProcessName += qucikLogin;
    }
    return realProcessName;
}

void CreateSuspendedProcess(std::wstring processName, std::wstring dllPath)
{
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    // 修改标准输出流
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    //  创建并挂起进程
    if (!CreateProcessW(NULL, (LPWSTR)processName.c_str(), NULL, NULL, TRUE, CREATE_SUSPENDED, (LPVOID)NULL, NULL, &si, &pi))
    {
        // 输出错误信息
        DWORD error = GetLastError();
        LPVOID errorMsg;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&errorMsg,
            0,
            NULL);
        std::cerr << "Error: " << (char *)errorMsg << std::endl;
        LocalFree(errorMsg);
        std::cerr << "Failed to start process." << std::endl;
        return;
    }
    MainProcessHandle = pi.hProcess;
    std::cout << "[NapCat Backend] Main Process ID:" << pi.dwProcessId << std::endl;
    // 注入 DLL
    LPVOID pRemoteBuf = VirtualAllocEx(pi.hProcess, NULL, (dllPath.size() + 1) * sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(pi.hProcess, pRemoteBuf, (LPVOID)dllPath.c_str(), (dllPath.size() + 1) * sizeof(wchar_t), NULL);

    HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, pRemoteBuf, 0, NULL);
    WaitForSingleObject(hThread, INFINITE);
    // 恢复进程
    ResumeThread(pi.hThread);
    // 等待进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 关闭句柄
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    system("pause");
    // 判断进程是否残留
    if (WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 0);
    }
}

void CreateSuspendedProcessW(const wchar_t *processName, const wchar_t *dllPath)
{
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFOW));
    // 修改标准输出流
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    // ACSPORT

    if (!CreateProcessW(NULL, (LPWSTR)processName, NULL, NULL, TRUE, CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, (LPVOID)NULL, NULL, &si, &pi))
    {
        // 输出错误信息
        DWORD error = GetLastError();
        LPVOID errorMsg;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&errorMsg,
            0,
            NULL);
        // 输出错误代码和程序路径
        std::wcerr << L"Error Code: " << error << std::endl;
        std::wcerr << L"Process Path: " << processName << std::endl;
        std::wcerr << L"Error: " << (wchar_t *)errorMsg << std::endl;
        LocalFree(errorMsg);
        std::wcerr << L"Failed to start process." << std::endl;
        return;
    }
    MainProcessHandle = pi.hProcess;
    std::wcout << L"[NapCat Backend] Main Process ID:" << pi.dwProcessId << std::endl;
    // 注入 DLL
    LPVOID pRemoteBuf = VirtualAllocEx(pi.hProcess, NULL, (wcslen(dllPath) + 1) * sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(pi.hProcess, pRemoteBuf, (LPVOID)dllPath, (wcslen(dllPath) + 1) * sizeof(wchar_t), NULL);

    HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, pRemoteBuf, 0, NULL);
    WaitForSingleObject(hThread, INFINITE);
    // 恢复进程
    ResumeThread(pi.hThread);
    // 等待进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 关闭句柄
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    // 判断进程是否残留
    if (WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 0);
    }
}

bool IsUserAnAdmin()
{
    BOOL fIsRunAsAdmin = FALSE;
    DWORD dwError = ERROR_SUCCESS;
    PSID pAdministratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdministratorsGroup))
    {
        dwError = GetLastError();
    }
    else
    {
        if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin))
        {
            dwError = GetLastError();
        }
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin;
}

void signalHandler(int signum)
{
    if (MainProcessHandle != NULL)
    {
        std::cout << "[NapCat Backend] Terminate Main Process." << std::endl;
        TerminateProcess(MainProcessHandle, 0);
    }
    exit(signum);
}

std::wstring AnsiToUtf16(const std::string &str)
{
    int size_needed = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    std::vector<std::wstring> args;
    for (int i = 0; i < argc; i++)
    {
        std::wstring argTemp = AnsiToUtf16(argv[i]);
        args.push_back(argTemp);
        std::wcout << "argv[" << i << "]:" << argTemp << std::endl;
    }
    if (argc < 3)
    {
        system("pause");
        return 1;
    }
    std::wstring quickLoginQQ = L"";
    if (argc == 4)
    {
        quickLoginQQ = args[3];
    }
    std::wstring bootCommand = createBootCommand(args[1], quickLoginQQ);
    std::wcout << L"Boot Command:" << bootCommand << std::endl;
    CreateSuspendedProcessW(bootCommand.c_str(), args[2].c_str());
    return 0;
}
