//-----------------------------------------------------------------------------
// ListOpenHandles
// Author:  DLang   2009  
//-----------------------------------------------------------------------------

#include "ListOpenHandles.h"

#include <windows.h>
#include <psapi.h>
#include <winternl.h>
#include <winioctl.h>
#include <errno.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <io.h>


#include <map>
#include <vector>

using  namespace std;

#if 0
 #include <Ntstatus.h>
#else
 const unsigned int STATUS_INFO_LENGTH_MISMATCH = 0xc0000004;
 const DWORD STATUS_SUCCESS = 0;
 // #define STATUS_INVALID_HANDLE ((NTSTATUS)0xC0000008L) 
#endif


/// Simple smart pointer 
template < typename T > 
class ScopePtr
{
private:
    T*      m_pData; // Generic pointer to be stored
    size_t  m_size;

public:
    ScopePtr() : m_pData((T*)malloc(sizeof(T))), m_size(sizeof(T))
    { }

    ScopePtr(size_t size) : m_pData((T*)malloc(size)), m_size(size)
    { }

    ~ScopePtr()
    {
        free(m_pData);
    }

    ScopePtr& operator=(ScopePtr& rhs)
    {
        if (this != &rhs)
        {
            free(m_pData);
            m_pData = rhs.m_pData;
            rhs.m_pData = NULL;
        }
    }

    void Resize(size_t newSize)
    {
        m_pData = (T*)realloc(m_pData, newSize);
        m_size = newSize;
    }

    size_t Size() const { return m_size;}
    operator T*()  { return m_pData; }
    T* operator-> () { return m_pData;}
};

//-----------------------------------------------------------------------------
//  http://forum.sysinternals.com/forum_posts.asp?TID=5677
//  http://forum.sysinternals.com/forum_posts.asp?TID=18892
//  C# http://processhacker.sourceforge.net/hacking/structs.php#systemprocessinformation
//  C# http://forum.sysinternals.com/forum_posts.asp?TID=17533
//  http://www.daniweb.com/forums/thread114975.html
//  http://www.techreplies.com/development-resources-58/enumerating-handles-processes-c-like-procxp-782021/
//  http://www.codeguru.com/forum/archive/index.php/t-328977.html
//  http://social.msdn.microsoft.com/Forums/en-US/netfx64bit/thread/7bfc32ea-b493-4853-a690-fef7b1170162
//  http://msdn.microsoft.com/en-us/library/ms804359.aspx
//  http://msdn.microsoft.com/en-us/library/bb432383(VS.85).aspx



#if 0

typedef enum _OBJECT_INFORMATION_CLASS {
  ObjectBasicInformation=0,
  ObjectNameInformation=1,       // undocumented
  ObjectTypeInformation=2,
} OBJECT_INFORMATION_CLASS;

typedef struct _PUBLIC_OBJECT_BASIC_INFORMATION {
  ULONG  Attributes;
  ACCESS_MASK  GrantedAccess;
  ULONG  HandleCount;
  ULONG  PointerCount;
  ULONG  Reserved[10];
} PUBLIC_OBJECT_BASIC_INFORMATION;

typedef struct _PUBLIC_OBJECT_TYPE_INFORMATION {
    UNICODE_STRING TypeName;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
    ULONG TotalPagedPoolUsage;
    ULONG TotalNonPagedPoolUsage;
    ULONG TotalNamePoolUsage;
    ULONG TotalHandleTableUsage;
    ULONG HighWaterNumberOfObjects;
    ULONG HighWaterNumberOfHandles;
    ULONG HighWaterPagedPoolUsage;
    ULONG HighWaterNonPagedPoolUsage;
    ULONG HighWaterNamePoolUsage;
    ULONG HighWaterHandleTableUsage;
    ULONG InvalidAttributes;
    GENERIC_MAPPING GenericMapping;
    ULONG ValidAccessMask;
    BOOLEAN SecurityRequired;
    BOOLEAN MaintainHandleCount;
    ULONG PoolType;
    ULONG DefaultPagedPoolCharge;
    ULONG DefaultNonPagedPoolCharge;
} PUBLIC_OBJECT_TYPE_INFORMATION; 

#endif

typedef struct _PUBLIC_OBJECT_NAME_INFORMATION 
{
  UNICODE_STRING          Name;
  WCHAR                   NameBuffer[1];
} PUBLIC_OBJECT_NAME_INFORMATION, *PPUBLIC_OBJECT_NAME_INFORMATION;


typedef struct _FILE_NAME_INFORMATION 
{
  ULONG                   FileNameLength;
  WCHAR                   FileName[1];
} FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;


typedef enum __FILE_INFORMATION_CLASS 
{
    __FileDirectoryInformation=1,
    FileFullDirectoryInformation,
    FileBothDirectoryInformation,
    FileBasicInformation,
    FileStandardInformation,
    FileInternalInformation,
    FileEaInformation,
    FileAccessInformation,
    FileNameInformation,
    FileRenameInformation,
    FileLinkInformation,
    FileNamesInformation,
    FileDispositionInformation,
    FilePositionInformation,
    FileFullEaInformation,
    FileModeInformation,
    FileAlignmentInformation,
    FileAllInformation,
    FileAllocationInformation,
    FileEndOfFileInformation,
    FileAlternateNameInformation,
    FileStreamInformation,
    FilePipeInformation,
    FilePipeLocalInformation,
    FilePipeRemoteInformation,
    FileMailslotQueryInformation,
    FileMailslotSetInformation,
    FileCompressionInformation,
    FileCopyOnWriteInformation,
    FileCompletionInformation,
    FileMoveClusterInformation,
    FileQuotaInformation,
    FileReparsePointInformation,
    FileNetworkOpenInformation,
    FileObjectIdInformation,
    FileTrackingInformation,
    FileOleDirectoryInformation,
    FileContentIndexInformation,
    FileInheritContentIndexInformation,
    FileOleInformation,
    FileMaximumInformation
} __FILE_INFORMATION_CLASS;


struct SYSTEM_HANDLE_INFORMATION
{                                       // XP64     XP32
	ULONG	ProcessId;                  //  4        4
	UCHAR	ObjectTypeNumber;           //  1        1
	UCHAR	Flags;                      //  1        1
	USHORT	Handle;                     //  2        2  
	PVOID	Object;                     //  8        4 
	ACCESS_MASK	GrantedAccess;          //  4        4
};                                      // 24       16

struct SYSTEM_HANDLE_LIST_INFORMATION
{
    ULONG   NumHandles;
    SYSTEM_HANDLE_INFORMATION   Handles[1];
};

enum __SYSTEM_INFORMATION_CLASS
{
    __SystemBasicInformation,                   // 0 
    __SystemProcessorInformation,               // 1 
    __SystemPerformanceInformation,             // 2 
    __SystemTimeOfDayInformation,               // 3 
    __SystemNotImplemented1,                    // 4 
    __SystemProcessesAndThreadsInformation,     // 5 
    __SystemCallCounts,                         // 6 
    __SystemConfigurationInformation,           // 7 
    __SystemProcessorTimes,                     // 8 
    __SystemGlobalFlag,                         // 9 
    __SystemNotImplemented2,                    // 10 
    __SystemModuleInformation,                  // 11 
    __SystemLockInformation,                    // 12 
    __SystemNotImplemented3,                    // 13 
    __SystemNotImplemented4,                    // 14 
    __SystemNotImplemented5,                    // 15 
    __SystemHandleInformation                   // 16 
    // there's more but it would make the post too long...
};

const SYSTEM_INFORMATION_CLASS SystemHandleInformation = (SYSTEM_INFORMATION_CLASS)__SystemHandleInformation;

#if 0
typedef struct _UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
#endif


// How to get IP address from handle
//
// http://forum.sysinternals.com/forum_posts.asp?TID=1193

#pragma comment(lib, "ws2_32.lib")
typedef LONG TDI_STATUS;
typedef PVOID CONNECTION_CONTEXT;       // connection context

typedef struct _TDI_REQUEST {
    union {
        HANDLE AddressHandle;
        CONNECTION_CONTEXT ConnectionContext;
        HANDLE ControlChannel;
    } Handle;

    PVOID RequestNotifyObject;
    PVOID RequestContext;
    TDI_STATUS TdiStatus;
} TDI_REQUEST, *PTDI_REQUEST;

typedef struct _TDI_CONNECTION_INFORMATION {
    LONG UserDataLength;        // length of user data buffer
    PVOID UserData;             // pointer to user data buffer
    LONG OptionsLength;         // length of following buffer
    PVOID Options;              // pointer to buffer containing options
    LONG RemoteAddressLength;   // length of following buffer
    PVOID RemoteAddress;        // buffer containing the remote address
} TDI_CONNECTION_INFORMATION, *PTDI_CONNECTION_INFORMATION;

typedef struct _TDI_REQUEST_QUERY_INFORMATION {
    TDI_REQUEST Request;
    ULONG QueryType;                          // class of information to be queried.
    PTDI_CONNECTION_INFORMATION RequestConnectionInformation;
} TDI_REQUEST_QUERY_INFORMATION, *PTDI_REQUEST_QUERY_INFORMATION;

#define TDI_QUERY_ADDRESS_INFO			0x00000003
#define IOCTL_TDI_QUERY_INFORMATION		CTL_CODE(FILE_DEVICE_TRANSPORT, 4, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)


// -----------------------------------------------------------------------------
/// Convert Wide character to multibyte
char* WideToCptr(wchar_t* wPtr, int wLen)
{
    if (wLen <= 0)
        return "";

    int cLen = WideCharToMultiByte(CP_ACP, 0, wPtr, wLen/2, NULL, 0, NULL, NULL);

    static ScopePtr<char> cPtr(cLen+1);
    
    if ((size_t)cLen > cPtr.Size())
    {
        cPtr.Resize(cLen+1);
    }

    cLen = WideCharToMultiByte(CP_ACP, 0, wPtr, wLen/2, cPtr, cLen, NULL, NULL);
    cPtr[cLen] = '\0';
    return cPtr;
}

// -----------------------------------------------------------------------------
/// Compare full pathed process name too just name
bool compareProcessName(std::string justProcName, const char* fullPathProc)
{   
    if (justProcName.empty())
        return true;

    const char* pName = strrchr(fullPathProc, '\\');
    pName = (pName != NULL) ? pName+1 : fullPathProc;

    return (strnicmp(justProcName.c_str(),  pName, justProcName.length()) == 0);
}


// -----------------------------------------------------------------------------
/// List openHandles and repeat per settings.
int ListOpenHandles::ReportOpenFiles(ostream& outp)
{
    bool showHeaders = true;
    int  result = 0;

    for (int repIdx = 0; repIdx < m_repeat; repIdx++)
    {
        if (repIdx != 0)
            Sleep((DWORD)(m_waitMin * 60 * 1000));

        time_t now;
        time(&now);
        char buf[512];
        strftime(buf, sizeof(buf), "%x, %X, ", localtime(&now));

        showHeaders = ((repIdx % 30) == 0);
        result = ReportOpenFiles(outp, showHeaders, buf);
    }

    return result;
}

// -----------------------------------------------------------------------------

static void EnableDebugPrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tokenPriv;
    LUID luidDebug;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken) != FALSE) 
    {
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luidDebug) != FALSE)
        {
            tokenPriv.PrivilegeCount           = 1;
            tokenPriv.Privileges[0].Luid       = luidDebug;
            tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, sizeof(tokenPriv), NULL, NULL);
        }
    }
}

// -----------------------------------------------------------------------------

static string ConnectionDetails(HANDLE hObject)
{
    // http://msdn.microsoft.com/en-us/library/ms800801.aspx

    typedef NTSTATUS (WINAPI * DevIoCtlFile_t)(
            HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
            PIO_STATUS_BLOCK IoStatusBlock, DWORD IoControlCode,
            PVOID InputBuffer, DWORD InputBufferLength,
            PVOID OutputBuffer, DWORD OutputBufferLength);

    static DevIoCtlFile_t DevIoCtlFile = (DevIoCtlFile_t)GetProcAddress(GetModuleHandle("NTDLL.DLL"), "NtDeviceIoControlFile");
    string result;

    if (DevIoCtlFile != NULL)
    {
        IO_STATUS_BLOCK IoStatusBlock;
        TDI_REQUEST_QUERY_INFORMATION tdiRequestAddress = {{0}, TDI_QUERY_ADDRESS_INFO};
        BYTE tdiAddress[128];

        HANDLE hEvent2 = 0; // CreateEvent(NULL, TRUE, FALSE, NULL);
        NTSTATUS ntReturn2 = DevIoCtlFile(hObject, hEvent2, NULL, NULL, &IoStatusBlock, IOCTL_TDI_QUERY_INFORMATION,
            &tdiRequestAddress, sizeof(tdiRequestAddress), &tdiAddress, sizeof(tdiAddress));
        if (hEvent2) 
            CloseHandle(hEvent2);

        if (ntReturn2 == STATUS_SUCCESS)
        {
            struct in_addr *pAddr = (struct in_addr *)&tdiAddress[14];
            std::ostringstream sout;
            sout << inet_ntoa(*pAddr) << ":" <<  ntohs(*(PUSHORT)&tdiAddress[12]);
            result = sout.str();
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
// Return error < 0
//        hndCnt > 0

int ListOpenHandles::ReportOpenFiles(ostream& outp, bool showHeaders, const char* preFix)
{
    // Enable special mode to allow access to system handles  (way cool).
    EnableDebugPrivilege();

    // Open optional log file
    std::ofstream log;
    if (m_logFilename.empty() == false)
        log.open(m_logFilename.c_str(), std::ios::app);

    // Map undocumented or special function calls.
    static HMODULE  hNtDll = GetModuleHandle("ntdll.dll"); 

    typedef NTSTATUS (NTAPI *NtQuerySystemInformation_t)(UINT, PVOID, ULONG, PULONG); 
    static NtQuerySystemInformation_t NtQuerySystemInformation = (NtQuerySystemInformation_t)GetProcAddress(hNtDll, "NtQuerySystemInformation"); 

    typedef NTSTATUS (NTAPI *NtQueryInformationFile_t)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, __FILE_INFORMATION_CLASS); 
    static NtQueryInformationFile_t NtQueryInformationFileW = (NtQueryInformationFile_t)GetProcAddress(hNtDll, "NtQueryInformationFile"); 

    typedef NTSTATUS (NTAPI *NtQueryObject_t)(HANDLE, OBJECT_INFORMATION_CLASS, PVOID, ULONG, PULONG); 
    static NtQueryObject_t NtQueryObjectW = (NtQueryObject_t)GetProcAddress(hNtDll, "NtQueryObject"); 

    typedef NTSTATUS (NTAPI *NtQueryInformationProcess_t)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass,
            PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength OPTIONAL);
    static NtQueryInformationProcess_t NtQueryInformationProcessW = (NtQueryInformationProcess_t)GetProcAddress(hNtDll, "NtQueryInformationProcess"); 

#if 0
    typedef NTSTATUS (NTAPI *NtQueryInformationProcessW)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG); 
    NtQueryInformationProcessW NtQueryInformationProcessA;
    NtQueryInformationProcessA = (NtQueryInformationProcessW)GetProcAddress(hNtDll, "NtQueryInformationProcess"); 

    HMODULE  hPsDll = GetModuleHandle("Psapi.dll"); 
    typedef DWORD (NTAPI *GetModuleFileNameEx_t)(HANDLE hProcess, HMODULE hModule,LPTSTR lpFilename, DWORD nSize);
    GetModuleFileNameEx_t GetModuleFileNameExA = (ZGetModuleFileNameEx_t)GetProcAddress(hPsDll, "GetModuleFileNameEx");
#endif
    
    if (NtQuerySystemInformation == NULL || 
        NtQueryInformationFileW == NULL ||
        NtQueryObjectW == NULL ||
        NtQueryInformationProcessW == NULL )
    {
        outp << "Sorry - ListOpenHandles failed to bind one or more functions. Unable to proceed.\n";
        return -1;
    }

    NTSTATUS status = 0;
    ULONG retSize;

    ScopePtr<SYSTEM_HANDLE_LIST_INFORMATION> pSysHandleListInfo(sizeof(SYSTEM_HANDLE_INFORMATION) * 500);

    if (pSysHandleListInfo == NULL)
    {
        outp << "Failed to allocate memory for required buffers.\n";
        return -2;
    }

    while ((status = NtQuerySystemInformation(SystemHandleInformation, pSysHandleListInfo, pSysHandleListInfo.Size(), &retSize)) == 
        STATUS_INFO_LENGTH_MISMATCH)
    {
        pSysHandleListInfo.Resize(pSysHandleListInfo.Size() + sizeof(SYSTEM_HANDLE_INFORMATION) * 500);
    }
    DWORD numHandles = pSysHandleListInfo->NumHandles;

    // Fallback guesses at object type mapping. Normally this list is not required and is provided
    // by quering the object type.  
    static std::map<unsigned int, const char*> sObjTypeMap;
    if (sObjTypeMap.size() < 20)
    {
        sObjTypeMap[1] = "type1";
        sObjTypeMap[2] = "Directory";
        sObjTypeMap[3] = "SymbolicLink";
        sObjTypeMap[4] = "Token";
        sObjTypeMap[5] = "Process";
        sObjTypeMap[6] = "Thread";
        sObjTypeMap[7] = "Job";
        sObjTypeMap[8] = "type8";
        sObjTypeMap[9] = "Event";
        sObjTypeMap[10] = "type10";
        sObjTypeMap[11] = "Mutant";
        sObjTypeMap[12] = "type12";
        sObjTypeMap[13] = "Semaphore";
        sObjTypeMap[14] = "Timer";
        sObjTypeMap[15] = "type15";
        sObjTypeMap[16] = "KeyedEvent";
        sObjTypeMap[17] = "WindowStn";
        sObjTypeMap[18] = "Desktop";
        sObjTypeMap[19] = "Section";
        sObjTypeMap[20] = "Registry";
        sObjTypeMap[21] = "Port";
        sObjTypeMap[22] = "type22";
        sObjTypeMap[23] = "type23";
        sObjTypeMap[24] = "type24";
        sObjTypeMap[25] = "type25";
        sObjTypeMap[26] = "type26";
        sObjTypeMap[27] = "IoComplete";
        sObjTypeMap[28] = "File";
        sObjTypeMap[29] = "WmiGuid";
        sObjTypeMap[39] = "type39";
    }

    static std::vector<bool> sNeedReadAccess;
    if (sNeedReadAccess.size() == 0)
    {
        sNeedReadAccess.resize(40, false);
        sNeedReadAccess[2] = sNeedReadAccess[20] = sNeedReadAccess[28] = true;
    }

    // EnumProcesses(...)

    ScopePtr<SYSTEM_PROCESS_INFORMATION> pSysProcList(sizeof(SYSTEM_PROCESS_INFORMATION) * 100);
    while ((status = NtQuerySystemInformation(SystemProcessInformation, pSysProcList, pSysProcList.Size(), &retSize)) 
        == STATUS_INFO_LENGTH_MISMATCH)
    {
        pSysProcList.Resize(retSize);
    }

    // Allocate initial space for several special buffers.
    ScopePtr<FILE_NAME_INFORMATION> pFileInformation(512);
    ScopePtr<UNICODE_STRING>  pProcInfo(512);
    ScopePtr<PUBLIC_OBJECT_BASIC_INFORMATION>  pObjBasic;
    ScopePtr<PUBLIC_OBJECT_TYPE_INFORMATION> pObjType;
    ScopePtr<PUBLIC_OBJECT_NAME_INFORMATION> pObjName;

    // Loop over process list, if any
    SYSTEM_PROCESS_INFORMATION* pSysProcInfo = pSysProcList;
    SYSTEM_PROCESS_INFORMATION* pNextSysProcInfo = pSysProcList;
    if (retSize >= sizeof(SYSTEM_PROCESS_INFORMATION))
    {
        HANDLE processHandle = 0;

        // Loop over process table.
        bool more = true;
        while (more)
        {
            pSysProcInfo = pNextSysProcInfo;
            more = (pSysProcInfo->NextEntryOffset != 0);
            pNextSysProcInfo = (SYSTEM_PROCESS_INFORMATION*)((char*)pSysProcInfo + pSysProcInfo->NextEntryOffset);

            if (pSysProcInfo->UniqueProcessId == 0)
                continue;       // skip processId zero

            // Close previously opened process handle.
            if (processHandle > 0)
                CloseHandle(processHandle);

            // Try and open process
            processHandle = OpenProcess(PROCESS_QUERY_INFORMATION |PROCESS_VM_READ|PROCESS_DUP_HANDLE, FALSE, (DWORD)pSysProcInfo->UniqueProcessId);
            if (processHandle == 0)
                continue;       // cannot open system process

            // Get Process name, QueryInformationoProcess enumerators:
            //      ProcessBasicInformation = 0,
            //      ProcessWow64Information = 26
            //      ProcessImageFileName    = 27;
            const PROCESSINFOCLASS ProcessImageFileName = (PROCESSINFOCLASS)27;
            ULONG procRetSize;
            status = NtQueryInformationProcessW(processHandle, ProcessImageFileName, pProcInfo, pProcInfo.Size(), &procRetSize);
            if (status == 0 || status == STATUS_INFO_LENGTH_MISMATCH )
            {
                char* procName = WideToCptr(pProcInfo->Buffer, pProcInfo->Length);
                if (compareProcessName(m_process, procName) == false)
                    continue;   // if specific process requested, ignore others.

                if (m_processId != 0 && m_processId != (DWORD)pSysProcInfo->UniqueProcessId)
                    continue;

                if (showHeaders)
                    outp << "\nProcessName:" << procName << "\n";
            }

            if (m_showProcInfo)
                outp << "----\n"
                    << "Pid: "  << (unsigned)pSysProcInfo->UniqueProcessId
                    << ", Handles:"  << pSysProcInfo->HandleCount
                    << ", PeakPageUsage: " << pSysProcInfo->PeakPagefileUsage
                    << ", PrivatePageCnt: " << pSysProcInfo->PrivatePageCount
                    << endl;
            
            if (m_showModules)
            {
                char szName[MAX_PATH];
                HMODULE hMods[1024];
                if (EnumProcessModules(processHandle, hMods, sizeof(hMods), &retSize))
                {
                    for (int i = 0; i != (retSize / sizeof(HMODULE)); i++ )
                    {
                        // TODO - remove sizeof(TCHAR)
                        if (GetModuleFileNameEx(processHandle, hMods[i], szName, sizeof(szName) / sizeof(TCHAR)))
                        {
                            outp << "\tModName," << szName << ",\t Address," << hex << hMods[i] << dec << "\n";
                        }
                    }
                }

                int fnLen = GetModuleFileNameEx(processHandle, 0, szName, sizeof(szName));
                if (fnLen > 0)
                    outp << "  ModFileName: " << szName << "\n";
            }

            // For summary - count handles by type.
            std::map<unsigned int, unsigned int> handleCountByType;
            size_t accessErrorCount = 0;

            for (int hndIdx = 0; hndIdx != numHandles; hndIdx++)
            {
                ULONG	processId  = pSysHandleListInfo->Handles[hndIdx].ProcessId;
                USHORT  handle     = pSysHandleListInfo->Handles[hndIdx].Handle;
                ACCESS_MASK gaccess= pSysHandleListInfo->Handles[hndIdx].GrantedAccess;

                if (processId != (ULONG)pSysProcInfo->UniqueProcessId)
                {
                    // Ignore file handles for other processes.
                    continue;
                }

                // Get default object type string
                unsigned objType  = (unsigned)pSysHandleListInfo->Handles[hndIdx].ObjectTypeNumber;
                std::string objTypeStr = "?";
                if (sObjTypeMap.find(objType) != sObjTypeMap.end())
                    objTypeStr = sObjTypeMap[objType]; 
                handleCountByType[objType]++;

                // Query the object name (unless it has an access of 
                //  0x0012019f, on which NtQueryObject could hang. */
                if (pSysHandleListInfo->Handles[hndIdx].GrantedAccess == 0x0012019f)
                    continue;

                HANDLE dupHnd = INVALID_HANDLE_VALUE;
                status = DuplicateHandle(processHandle, (HANDLE)handle, GetCurrentProcess(), &dupHnd, 
                        0, FALSE, 0); 
                DWORD err = GetLastError();

                if (status == FALSE || dupHnd == 0)
                {
                    if (err == EIO)
                        accessErrorCount++;

                    continue;       // ERROR - failed to dup handle
                }

                // ----- Get Basic information

                // Skip some magic files because they hang the NtQuery call.
                // Values 0x019f from systernal forum.
                if ((gaccess & 0xffff) == 0x019f || gaccess == 0x100000)
                {
                    // Possible pipe
                    if (m_showAll)
                    outp << setw(5) << (m_showHex? hex : dec) << handle << hex 
                        << ", " << gaccess << dec
                        << ", " << setw(2) 
                        << "Pipe?"
                        << std::endl;
                    continue;
                }

                status = NtQueryObjectW(dupHnd, ObjectBasicInformation, pObjBasic, pObjBasic.Size(), &retSize);
                if (status == 0 && m_showAll)
                {
                    if (pObjBasic->Attributes != 0)
                        outp << " Attr:" << pObjBasic->Attributes;
                }

                // ----- Get Type information.
                status = NtQueryObjectW(dupHnd, ObjectTypeInformation, pObjType, pObjType.Size(), &retSize);
                if (status == STATUS_INFO_LENGTH_MISMATCH) 
                {
                    pObjType.Resize(retSize);
                    status = NtQueryObjectW(dupHnd, ObjectTypeInformation, pObjType, pObjType.Size(), &retSize);
                }
                if (status == 0)
                    objTypeStr = WideToCptr(pObjType->TypeName.Buffer, pObjType->TypeName.Length);

                // ----- Get handle description details
                std::string objDesc;

                status = NtQueryObjectW(dupHnd, (OBJECT_INFORMATION_CLASS)1, pObjName, pObjName.Size(), &retSize);
                if (status == STATUS_INFO_LENGTH_MISMATCH)
                {
                    pObjName.Resize(retSize);
                    status = NtQueryObjectW(dupHnd, (OBJECT_INFORMATION_CLASS)1, pObjName, pObjName.Size(), &retSize);
                }

                if (status == 0 && m_showAll)
                {
                    objDesc = WideToCptr(pObjName->Name.Buffer, pObjName->Name.Length);
                }
                else
                {
                    IO_STATUS_BLOCK statusBlock;
                    // ZeroMemory(&statusBlock, sizeof(statusBlock));

                    NTSTATUS status = NtQueryInformationFileW(dupHnd, &statusBlock, pFileInformation, pFileInformation.Size(), FileNameInformation);
                    if (status == STATUS_SUCCESS && m_showFiles)
                    {
                        objDesc = WideToCptr(pFileInformation->FileName, pFileInformation->FileNameLength);
                    }
                }

                // Display results
                if (objDesc.empty() == false || m_showAll)
                {
                    outp << setw(5) << (m_showHex? hex : dec) << handle << hex 
                        << ", " << gaccess << dec
                        << ", " << setw(2) 
                        << objTypeStr;
                    if (objDesc.empty() == false)
                        outp << ",\t " << objDesc.c_str();

                    if (objDesc == "\\Device\\Tcp" || objDesc == "\\Device\\Udp")
                        outp << " [" << ConnectionDetails(dupHnd) << "]";

                    outp << "\n";
                }

                CloseHandle(dupHnd);
            }

            if (m_showSummary)
            {
                std::map<unsigned int, unsigned int>::const_iterator iter;
#if 0
                // Display handle count summary, one per row
                iter = handleCountByType.begin();
                outp << " Summary\n--------\n";
                for( ; iter != handleCountByType.end(); ++iter)
                {
                    std::string objTypeStr = sObjTypeMap[iter->first];
                    if (objTypeStr.empty())
                        objTypeStr = "<unknown>";

                    // outp << setw(3) << iter->first << ", ";
                    outp << setw(5) << iter->second << " " << objTypeStr  << "\n";
                }
#else
                if (showHeaders)
                {
                    // Display handle count summary in columns, better for repeated running mode.

                    outp << " Summary\n";
                    std::ostringstream sout;
                    sout << preFix;
                    std::string objTypeStr;
                    for(iter = handleCountByType.begin() ; iter != handleCountByType.end(); ++iter)
                    {
                        objTypeStr = sObjTypeMap[iter->first]; 
                        if (objTypeStr.empty())
                            objTypeStr ="<unknown>"; 
                        objTypeStr.resize(7, ' ');
                        sout << objTypeStr << ", ";
                    }
                    objTypeStr = "Total";
                    objTypeStr.resize(7, ' ');
                    sout << objTypeStr << "\n";

                    std::string sline = sout.str();
                    outp << sline;
                    if (log.good())
                        log << sline;
                }

                int totHnd = 0;
                std::ostringstream sout;
                sout << preFix;
                for(iter = handleCountByType.begin() ; iter != handleCountByType.end(); ++iter)
                {
                    totHnd += iter->second;
                    sout << setw(7) << iter->second << ", ";
                }
                sout << setw(7) << totHnd << "\n";

                std::string sline = sout.str();
                outp << sline;
                if (log.good())
                    log << sline;
#endif
            }

            if (m_showAll && accessErrorCount != 0)
                outp << " access Error count= " << accessErrorCount << "\n";
        } 

        if (processHandle > 0)
            CloseHandle(processHandle);
    }
}

#if 0
http://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/187d190d-95e4-46f8-a6bd-4f8f089b6463/

void WhoUsesFile( LPCTSTR lpFileName, BOOL bFullPathCheck )
{
    BOOL bShow = FALSE;
    CString name;
    CString processName;
    CString deviceFileName;
    CString fsFilePath;
    SystemProcessInformation:YSTEM_PROCESS_INFORMATION* p;
    SystemProcessInformation pi;
    SystemHandleInformation hi;

    if ( bFullPathCheck )
    {
        if ( !SystemInfoUtils::GetDeviceFileName( lpFileName, deviceFileName ) )
        {
            _tprintf( _T("GetDeviceFileName() failed.\n") );
            return;
        }
    }

    hi.SetFilter( _T("File"), TRUE );

    if ( hi.m_HandleInfos.GetHeadPosition() == NULL )
    {
        _tprintf( _T("No handle information\n") );
        return;
    }
    
    pi.Refresh();

    _tprintf( _T("%-6s  %-20s  %s\n"), _T("PID"), _T("Name"), _T("Path") );
    _tprintf( _T("------------------------------------------------------\n") );

    for ( POSITION pos = hi.m_HandleInfos.GetHeadPosition(); pos != NULL; )
    {
        SystemHandleInformation:YSTEM_HANDLE& h = hi.m_HandleInfos.GetNext(pos);

        if ( pi.m_ProcessInfos.Lookup( h.ProcessID, p ) )
        {
            SystemInfoUtils::Unicode2CString( &p->usName, processName );
}
        else
            processName = "";

        //NT4 Stupid thing if it is the services.exe and I call GetName (
        if ( INtDll:wNTMajorVersion == 4 && _tcsicmp( processName, _T("services.exe" ) ) == 0 )
            continue;
        
        hi.GetName( (HANDLE)h.HandleNumber, name, h.ProcessID );

        if ( bFullPathCheck )
            bShow =    _tcsicmp( name, deviceFileName ) == 0;
        else
            bShow =    _tcsicmp( GetFileNamePosition(name), lpFileName ) == 0;

        if ( bShow )
        {
            if ( !bFullPathCheck )
            {
                fsFilePath = "";
                SystemInfoUtils::GetFsFileName( name, fsFilePath );
            }
            
            _tprintf( _T("0x%04X  %-20s  %s\n"), 
                h.ProcessID, 
                processName,
                !bFullPathCheck ? fsFilePath : lpFileName );
        }
    }
}

#endif

#if 0
[DllImport("ntdll.dll", SetLastError = true)]
public static extern uint ZwQuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInformationClass, IntPtr SystemInformation, int SystemInformationLength, out int ReturnLength);

public enum SYSTEM_INFORMATION_CLASS : int
{
    SystemBasicInformation,
    SystemProcessorInformation,
    SystemPerformanceInformation,
    SystemTimeOfDayInformation,
    SystemNotImplemented1,
    SystemProcessesAndThreadsInformation,
    SystemCallCounts,
    SystemConfigurationInformation,
    SystemProcessorTimes,
    SystemGlobalFlag,
    SystemNotImplemented2,
    SystemModuleInformation,
    SystemLockInformation,
    SystemNotImplemented3,
    SystemNotImplemented4,
    SystemNotImplemented5,
    SystemHandleInformation
    // there's more but it would make the post too long...
}

[StructLayout(LayoutKind.Sequential)]
public struct SYSTEM_HANDLE_INFORMATION
{
    public int ProcessId;
    public byte ObjectTypeNumber;
    public byte Flags; // 1 = PROTECT_FROM_CLOSE, 2 = INHERIT
    public short Handle;
    public int Object;
    public int GrantedAccess;
}

public const uint STATUS_INFO_LENGTH_MISMATCH = 0xc0000004;

public static SYSTEM_HANDLE_INFORMATION[] EnumHandles()
{
    int retLength = 0;
    int handleCount = 0;
    SYSTEM_HANDLE_INFORMATION[] returnHandles;
    int allocLength = 0x1000;
    IntPtr data = Marshal.AllocHGlobal(allocLength);
    
    // This is needed because ZwQuerySystemInformation with SystemHandleInformation doesn't
    // actually give a real return length when called with an insufficient buffer. This code
    // tries repeatedly to call the function, doubling the buffer size each time it fails.
    while (ZwQuerySystemInformation(SYSTEM_INFORMATION_CLASS.SystemHandleInformation, data,
        allocLength, out retLength) == STATUS_INFO_LENGTH_MISMATCH)
        data = Marshal.ReAllocHGlobal(data, new IntPtr(allocLength *= 2));
    
    // The structure of the buffer is the handle count plus an array of SYSTEM_HANDLE_INFORMATION
    // structures.
    handleCount = Marshal.ReadInt32(data);
    returnHandles = new SYSTEM_HANDLE_INFORMATION[handleCount];
    
    for (int i = 0; i < handleCount; i++)
    {
        returnHandles = (SYSTEM_HANDLE_INFORMATION)Marshal.PtrToStructure(
            new IntPtr(data.ToInt32() + 4 + i * Marshal.SizeOf(typeof(SYSTEM_HANDLE_INFORMATION))),
            typeof(SYSTEM_HANDLE_INFORMATION));
    }
    
    Marshal.FreeHGlobal(data);
    
    return returnHandles;
}


#endif
