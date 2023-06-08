#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <conio.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <psapi.h>

constexpr int COLOR_GREEN = 10;
constexpr int COLOR_YELLOW = 14;
constexpr int COLOR_ORANGE = 12;
constexpr int COLOR_RED = 4;
constexpr int COLOR_LIGHT_BLUE = 11;

enum ProcessFilter {
    AllProcesses,
    SystemInformation
};

void SetConsoleTextColor(const int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void PrintProcessList(HANDLE snapshot, ProcessFilter filter) {
    PROCESSENTRY32W processEntry = { sizeof(PROCESSENTRY32W) };
    if (Process32FirstW(snapshot, &processEntry)) {
        SetConsoleTitle(L"PID Finder/Process Finder");
        SetConsoleTextColor(COLOR_GREEN);
        std::wcout << "PID\t\tProcess Name\t\tCPU Usage\tMemory Usage" << std::endl;
        std::wcout << "---------------------------------------------------------------------" << std::endl;

        do {
            bool isApplication = (processEntry.szExeFile[0] != L'\0');
            if (filter == SystemInformation && isApplication) {
                continue;
            }

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processEntry.th32ProcessID);
            if (hProcess) {
                FILETIME createTime, exitTime, kernelTime, userTime;
                GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime);

                ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
                lastCPU.LowPart = lastSysCPU.LowPart = lastUserCPU.LowPart = 0;
                lastCPU.HighPart = lastSysCPU.HighPart = lastUserCPU.HighPart = 0;

                double percentCPU = 0.0;
                SYSTEM_INFO sysInfo;
                GetSystemInfo(&sysInfo);

                DWORDLONG processMemory = 0;
                PROCESS_MEMORY_COUNTERS_EX pmc;
                if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                    processMemory = pmc.PrivateUsage;
                }

                if (GetSystemTimes(&createTime, &exitTime, &kernelTime)) {
                    ULARGE_INTEGER now;
                    now.LowPart = userTime.dwLowDateTime;
                    now.HighPart = userTime.dwHighDateTime;

                    ULARGE_INTEGER sys;
                    sys.LowPart = kernelTime.dwLowDateTime;
                    sys.HighPart = kernelTime.dwHighDateTime;

                    ULARGE_INTEGER user;
                    user.LowPart = userTime.dwLowDateTime;
                    user.HighPart = userTime.dwHighDateTime;

                    percentCPU = (now.QuadPart - lastCPU.QuadPart) * 100.0 / (sys.QuadPart - lastSysCPU.QuadPart);
                    percentCPU /= sysInfo.dwNumberOfProcessors;

                    lastCPU = now;
                    lastSysCPU = sys;
                    lastUserCPU = user;
                }

                if (percentCPU > 80.0) {
                    SetConsoleTextColor(COLOR_RED);
                }
                else if (percentCPU < 20.0) {
                    SetConsoleTextColor(COLOR_LIGHT_BLUE);
                }
                else if (percentCPU < 60.0) {
                    SetConsoleTextColor(COLOR_GREEN);
                }
                else {
                    SetConsoleTextColor(COLOR_ORANGE);
                }

                std::wcout << processEntry.th32ProcessID << "\t\t";
                SetConsoleTextColor(COLOR_GREEN);
                std::wcout << processEntry.szExeFile << "\t\t";
                std::wcout << std::fixed << std::setprecision(2) << percentCPU << "%\t\t";
                std::wcout << processMemory / (1024.0 * 1024.0) << " MB" << std::endl;

                CloseHandle(hProcess);
            }
        } while (Process32NextW(snapshot, &processEntry));
    }
}

std::string GetSystemInformation() {
    SetConsoleTitle(L"System Information");
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);

    std::ostringstream oss;
    oss << "System Information:" << std::endl;
    oss << "------------------------------------------------------------" << std::endl;
    oss << "Processor Architecture: ";

    switch (systemInfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:
        oss << "x86 (32-bit)" << std::endl;
        break;
    case PROCESSOR_ARCHITECTURE_AMD64:
        oss << "x64 (64-bit)" << std::endl;
        break;
    case PROCESSOR_ARCHITECTURE_ARM:
        oss << "ARM" << std::endl;
        break;
    default:
        oss << "Unknown" << std::endl;
        break;
    }

    oss << "Number of Processors: " << systemInfo.dwNumberOfProcessors << std::endl;
    oss << "Page Size: " << systemInfo.dwPageSize << " bytes" << std::endl;
    oss << "Minimum Application Address: 0x" << std::hex << std::setw(8) << std::setfill('0') << systemInfo.lpMinimumApplicationAddress << std::endl;
    oss << "Maximum Application Address: 0x" << std::hex << std::setw(8) << std::setfill('0') << systemInfo.lpMaximumApplicationAddress << std::endl;

    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    GlobalMemoryStatusEx(&memoryStatus);

    oss << "Total Physical Memory: " << memoryStatus.ullTotalPhys / (1024 * 1024) << " MB" << std::endl;
    oss << "Available Physical Memory: " << memoryStatus.ullAvailPhys / (1024 * 1024) << " MB" << std::endl;
    oss << "Total Virtual Memory: " << memoryStatus.ullTotalVirtual / (1024 * 1024) << " MB" << std::endl;
    oss << "Available Virtual Memory: " << memoryStatus.ullAvailVirtual / (1024 * 1024) << " MB" << std::endl;

    return oss.str();
}

int main() {
    SetConsoleTitle(L"Multitool");

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to retrieve process snapshot." << std::endl;
        return 1;
    }

    const HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;
    GetConsoleMode(console, &consoleMode);
    SetConsoleMode(console, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    ProcessFilter filter = AllProcesses;
    bool optionSelected = false;

    while (true) {
        system("cls");

        SetConsoleTextColor(COLOR_GREEN);
        std::cout << std::endl;
        std::cout << "<------------------------------------------------------------------------------------->" << std::endl;

        if (optionSelected) {
            if (filter == AllProcesses) {
                PrintProcessList(snapshot, filter);
            }
            else if (filter == SystemInformation) {
                std::cout << GetSystemInformation() << std::endl;
            }

            const int key = _getch();
            if (key == VK_TAB) {
                optionSelected = false;
            }
        }
        else {
            if (filter == AllProcesses) {
                SetConsoleTitle(L"Multitool");
                SetConsoleTextColor(COLOR_YELLOW);
                std::cout << ">> Displaying All Processes" << std::endl;
                SetConsoleTextColor(COLOR_GREEN);
                std::cout << "   Display System Information" << std::endl;
            }
            else {
                SetConsoleTitle(L"Multitool");
                SetConsoleTextColor(COLOR_GREEN);
                std::cout << "   Display All Processes" << std::endl;
                SetConsoleTextColor(COLOR_YELLOW);
                std::cout << ">> Displaying System Information" << std::endl;
            }

            int key = _getch();
            if (key == 224/* Reserved */) {
                key = _getch();

                if (key == 72/*H key*/) {
                    filter = AllProcesses;
                }
                else if (key == 80/*P Key*/) {
                    filter = SystemInformation;
                }
            }
            else if (key == VK_RETURN) {
                optionSelected = true;
            }
        }
    }

    // Unreachable
    //CloseHandle(snapshot);
    //return 0;
}
