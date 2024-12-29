//-----------------------------------------------------------------------------
// FileSysInfo
// Author:  DLang   2009 
//-----------------------------------------------------------------------------

#define _CRT_SECURE_NO_WARNINGS 1

#include "FileSysInfo.h"
#include <windows.h>

#include <iostream>
#include <iomanip>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <io.h>


using  namespace std;

const size_t KB = (1<<10);
const size_t MB = (1<<20);
const size_t GB = (1<<30);    

//-----------------------------------------------------------------------------
DWORD CheckSum(const char* buffer, DWORD byteCount)
{
    DWORD sum = 0;
    while (byteCount-- != 0)
    {
        sum += *buffer++;
    }
    return sum;
}

//-----------------------------------------------------------------------------

int MeasureCacheFileSize(int nArg, const char* pArg[], Options& options)
{
    const char* filename1;
    const char* filename2;

    if (nArg == 0 || pArg[0] == NULL)
        filename1 = "filespeed-cache1.tmp";
    else
        filename1 = pArg[0];

    if (nArg < 1 || pArg[1] == NULL)
        filename2 = "filespeed-cache2.tmp";
    else
        filename2 = pArg[1];

    cout << 
        "\n"
        "Measure Cache Size\n"
        "==================\n"
        "\n"
        "Measure File system size by measuring speed to read back data just written.\n"
        "   1. A single file is created and 'n' Megabytes are written\n"
        "   2. The same file is left open and repositioned to the front\n"
        "   3. The time to read the entire file is reported\n"
        "\n"
        " The file cache improves read time until the requested data exceeds the cache \n"
        " at which time performance decrease.\n"
        "\n";

    cout << " Test file:" << filename1 << " " << filename2 << "\n\n";

    LARGE_INTEGER cacheSize;

    cacheSize.QuadPart = LONGLONG(MB * options.minCacheMB);
    LONGLONG endCache  = LONGLONG(MB * options.maxCacheMB);
    LONGLONG stepCache = LONGLONG(MB * options.stepCacheMB);
    cout << " Size MB, MB/s, Seconds\n";

    while (cacheSize.QuadPart < endCache)
    {
       HANDLE hFile1 = 
            CreateFile(filename1, 
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL
                | FILE_FLAG_DELETE_ON_CLOSE
                | FILE_FLAG_SEQUENTIAL_SCAN 
                ,0);

        if (hFile1 == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            cerr << "Error: " << err << filename1 << "\n";
            return -int(err);
        }
                
        HANDLE hFile2 = 
            CreateFile(filename2, 
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL
                | FILE_FLAG_DELETE_ON_CLOSE
                | FILE_FLAG_SEQUENTIAL_SCAN 
                ,0);

        if (hFile2 == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            cerr << "Error: " << err << filename2 << "\n";
            return -int(err);
        }
      
                
        const DWORD recSize = MB*8;
        const DWORD bytesPerSector = 512;   // TODO - ask filesystem for sector size.
        static char sBuffer[recSize+1024];

        // For nonbuffered I/O buffer must be on sector boundary
        char* buffer = (char*)(ULONG_PTR(sBuffer + bytesPerSector-1) & ~ULONG_PTR(bytesPerSector-1));
        memset(buffer, 11, recSize);

        DWORD bytesWrote= 0;
        DWORD wCksum = 0;

        LARGE_INTEGER fileSize;
        fileSize.QuadPart = 0;
        while (fileSize.QuadPart < cacheSize.QuadPart 
            && WriteFile(hFile1, buffer, recSize, &bytesWrote, 0)  
            && bytesWrote > 0)
        {
            wCksum += CheckSum(buffer, bytesWrote);
            fileSize.QuadPart += bytesWrote;
            memset(buffer, fileSize.LowPart|11, recSize);
        }

        LARGE_INTEGER endSize;
        endSize.HighPart = 0;
        endSize.LowPart = SetFilePointer(hFile1, 0, &endSize.HighPart, FILE_CURRENT); 

        if (SetFilePointer(hFile1, 0, 0, FILE_BEGIN) != 0)
            std::cout << "Seek to beginning failed\n";

        fileSize.QuadPart = 0;
        DWORD bytesRead= 0;
        DWORD rCksum = 0;

        DWORD startTick = GetTickCount();
        while (fileSize.QuadPart < cacheSize.QuadPart 
            && ReadFile(hFile1, buffer, recSize, &bytesRead, 0) 
            && bytesRead > 0)
        {
            rCksum += CheckSum(buffer, bytesRead);
            fileSize.QuadPart += bytesWrote;

            WriteFile(hFile2, buffer, bytesRead, &bytesWrote, 0);
            if (bytesWrote != bytesRead)
                std::cout << " Write failed\n";
        }
        DWORD endTick = GetTickCount();
        double seconds = (endTick - startTick) / 1000.0;

        cout << " " 
            << setw(5) << fileSize.QuadPart /(double)MB << " MB "
            << ", " 
            << fileSize.QuadPart / (MB * seconds)
            << ", " << seconds
            << ((rCksum != wCksum) ? ", check sum error" : "")
            << endl;

        CloseHandle(hFile1);
        CloseHandle(hFile2);
        remove(filename1);
        remove(filename2);

        cacheSize.QuadPart += stepCache;
    }

    return 0;
}


//-----------------------------------------------------------------------------

void DumpFileStats(int nArg, const char* pArg[], Options& options)
{
    const char* filename = pArg[0];

    if (nArg == 0 || pArg[0] == NULL)
        filename = "c:\\";    
    

    DWORD sectorsPerCluster;
    DWORD bytesPerSector;
    DWORD numberOfFreeClusters;
    DWORD totalNumberOfClusters;

    if (GetDiskFreeSpaceA(filename, 
        &sectorsPerCluster, &bytesPerSector, &numberOfFreeClusters, &totalNumberOfClusters) != 0)
    {
        cout << "       "
            << filename << "  FileSystem\n" 
            << setw(10) << sectorsPerCluster        << "  SectorsPerCluster \n" 
            << setw(10)<< bytesPerSector            << "  ByterPerSector    \n" 
            << setw(10)<< numberOfFreeClusters      << "  # Free Clusters   \n" 
            << setw(10)<< totalNumberOfClusters     << "  Total # Clusters  \n" 
            << "\n";

        HANDLE hFile;

        struct FS_stats 
        {
            union 
            {
                struct 
                {
                    FILESYSTEM_STATISTICS fs;
                    union 
                    {
                        NTFS_STATISTICS ntfs;
                        FAT_STATISTICS fat;
                    };
                    DWORD filler[12];
                };

                DWORDLONG ForceSizeAndAlignment[32];  // pad to a multiple of 64 bytes
            };
        } ntfs_stats[6];

        SecureZeroMemory(&ntfs_stats, sizeof(ntfs_stats));

        DWORD bytesReturned = 0;    
        hFile = CreateFileA(filename, FILE_READ_ATTRIBUTES, 7, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        DWORD err = GetLastError();

        SYSTEM_INFO SystemInfo;
	    GetSystemInfo(&SystemInfo);	          

        cout 
            << setw(10) << SystemInfo.dwAllocationGranularity << " dwAllocationGranularity\n"
            << setw(10) << SystemInfo.dwNumberOfProcessors    << " dwNumberOfProcessors\n"
            << setw(10) << SystemInfo.dwPageSize              << " dwPageSize\n"
            << setw(10) << SystemInfo.lpMaximumApplicationAddress << " lpMaximumApplicationAddress\n"
            << setw(10) << SystemInfo.lpMinimumApplicationAddress << " lpMinimumApplicationAddress\n"
            << "\n";

        int ctlResult = DeviceIoControl(
            hFile, 
            FSCTL_FILESYSTEM_GET_STATISTICS,    // dwIoControlCode
            NULL,                               // lpInBuffer
            0,                                  // nInBufferSize
            &ntfs_stats,                        // output buffer
            sizeof(ntfs_stats),                 // size of output buffer
            &bytesReturned,                     // number of bytes returned
            0);
        err = GetLastError();

        if (ctlResult != 0 || err == ERROR_MORE_DATA)
        {
            // Sum all stats into one.
            for (int n = 1;  n * sizeof(ntfs_stats[0]) <= bytesReturned; n++)
            {
                ntfs_stats[0].fs.UserFileReads                        +=   ntfs_stats[n].fs.UserFileReads;
                ntfs_stats[0].fs.UserFileReadBytes                    +=   ntfs_stats[n].fs.UserFileReadBytes;
                ntfs_stats[0].fs.UserDiskReads                        +=   ntfs_stats[n].fs.UserDiskReads;
                ntfs_stats[0].fs.UserFileWrites                       +=   ntfs_stats[n].fs.UserFileWrites;
                ntfs_stats[0].fs.UserFileWriteBytes                   +=   ntfs_stats[n].fs.UserFileWriteBytes;
                ntfs_stats[0].fs.UserDiskWrites                       +=   ntfs_stats[n].fs.UserDiskWrites;
                                                                      
                ntfs_stats[0].fs.MetaDataReads                        +=   ntfs_stats[n].fs.MetaDataReads;
                ntfs_stats[0].fs.MetaDataReadBytes                    +=   ntfs_stats[n].fs.MetaDataReadBytes;
                ntfs_stats[0].fs.MetaDataDiskReads                    +=   ntfs_stats[n].fs.MetaDataDiskReads;
                ntfs_stats[0].fs.MetaDataWrites                       +=   ntfs_stats[n].fs.MetaDataWrites;
                ntfs_stats[0].fs.MetaDataWriteBytes                   +=   ntfs_stats[n].fs.MetaDataWriteBytes;
                ntfs_stats[0].fs.MetaDataDiskWrites                   +=   ntfs_stats[n].fs.MetaDataDiskWrites;
                ntfs_stats[0].ntfs.LogFileFullExceptions              +=   ntfs_stats[n].ntfs.LogFileFullExceptions;
                ntfs_stats[0].ntfs.OtherExceptions                    +=   ntfs_stats[n].ntfs.OtherExceptions;
                                                                      
                ntfs_stats[0].ntfs.MftReads                           +=   ntfs_stats[n].ntfs.MftReads;
                ntfs_stats[0].ntfs.MftReadBytes                       +=   ntfs_stats[n].ntfs.MftReadBytes;
                                                                       
                ntfs_stats[0].ntfs.MftWrites                          +=   ntfs_stats[n].ntfs.MftWrites;
                ntfs_stats[0].ntfs.MftWriteBytes                      +=   ntfs_stats[n].ntfs.MftWriteBytes;
                ntfs_stats[0].ntfs.MftWritesUserLevel.Write           +=   ntfs_stats[n].ntfs.MftWritesUserLevel.Write;
                ntfs_stats[0].ntfs.MftWritesUserLevel.Create          +=   ntfs_stats[n].ntfs.MftWritesUserLevel.Create;
                ntfs_stats[0].ntfs.MftWritesUserLevel.SetInfo         +=   ntfs_stats[n].ntfs.MftWritesUserLevel.SetInfo;
                ntfs_stats[0].ntfs.MftWritesUserLevel.Flush           +=   ntfs_stats[n].ntfs.MftWritesUserLevel.Flush;
                                                                       
                ntfs_stats[0].ntfs.MftWritesFlushForLogFileFull       +=   ntfs_stats[n].ntfs.MftWritesFlushForLogFileFull;
                ntfs_stats[0].ntfs.MftWritesLazyWriter                +=   ntfs_stats[n].ntfs.MftWritesLazyWriter;
                ntfs_stats[0].ntfs.MftWritesUserRequest               +=   ntfs_stats[n].ntfs.MftWritesUserRequest;
                                                                       
                ntfs_stats[0].ntfs.Mft2Writes                         +=   ntfs_stats[n].ntfs.Mft2Writes;
                ntfs_stats[0].ntfs.Mft2WriteBytes                     +=   ntfs_stats[n].ntfs.Mft2WriteBytes;
                                                                       
                ntfs_stats[0].ntfs.Mft2WritesUserLevel.Write          +=   ntfs_stats[n].ntfs.Mft2WritesUserLevel.Write;
                ntfs_stats[0].ntfs.Mft2WritesUserLevel.Create         +=   ntfs_stats[n].ntfs.Mft2WritesUserLevel.Create;
                ntfs_stats[0].ntfs.Mft2WritesUserLevel.SetInfo        +=   ntfs_stats[n].ntfs.Mft2WritesUserLevel.SetInfo;
                ntfs_stats[0].ntfs.Mft2WritesUserLevel.Flush          +=   ntfs_stats[n].ntfs.Mft2WritesUserLevel.Flush;
                                                                       
                ntfs_stats[0].ntfs.Mft2WritesFlushForLogFileFull      +=   ntfs_stats[n].ntfs.Mft2WritesFlushForLogFileFull;
                ntfs_stats[0].ntfs.Mft2WritesLazyWriter               +=   ntfs_stats[n].ntfs.Mft2WritesLazyWriter;
                ntfs_stats[0].ntfs.Mft2WritesUserRequest              +=   ntfs_stats[n].ntfs.Mft2WritesUserRequest;
 
                ntfs_stats[0].ntfs.RootIndexReads                     +=   ntfs_stats[n].ntfs.RootIndexReads;
                ntfs_stats[0].ntfs.RootIndexReadBytes                 +=   ntfs_stats[n].ntfs.RootIndexReadBytes;
                ntfs_stats[0].ntfs.RootIndexWrites                    +=   ntfs_stats[n].ntfs.RootIndexWrites;
                ntfs_stats[0].ntfs.RootIndexWriteBytes                +=   ntfs_stats[n].ntfs.RootIndexWriteBytes;
 
                ntfs_stats[0].ntfs.BitmapReads                        +=   ntfs_stats[n].ntfs.BitmapReads;
                ntfs_stats[0].ntfs.BitmapReadBytes                    +=   ntfs_stats[n].ntfs.BitmapReadBytes;
                ntfs_stats[0].ntfs.BitmapWrites                       +=   ntfs_stats[n].ntfs.BitmapWrites;
                ntfs_stats[0].ntfs.BitmapWriteBytes                   +=   ntfs_stats[n].ntfs.BitmapWriteBytes;
 
                ntfs_stats[0].ntfs.BitmapWritesFlushForLogFileFull    +=   ntfs_stats[n].ntfs.BitmapWritesFlushForLogFileFull;
                ntfs_stats[0].ntfs.BitmapWritesLazyWriter             +=   ntfs_stats[n].ntfs.BitmapWritesLazyWriter;
                ntfs_stats[0].ntfs.BitmapWritesUserRequest            +=   ntfs_stats[n].ntfs.BitmapWritesUserRequest;
                                                                     
                ntfs_stats[0].ntfs.BitmapWritesUserLevel.Write        +=   ntfs_stats[n].ntfs.BitmapWritesUserLevel.Write;
                ntfs_stats[0].ntfs.BitmapWritesUserLevel.Create       +=   ntfs_stats[n].ntfs.BitmapWritesUserLevel.Create;
                ntfs_stats[0].ntfs.BitmapWritesUserLevel.SetInfo      +=   ntfs_stats[n].ntfs.BitmapWritesUserLevel.SetInfo;
 
                ntfs_stats[0].ntfs.MftBitmapReads                     +=   ntfs_stats[n].ntfs.MftBitmapReads;
                ntfs_stats[0].ntfs.MftBitmapReadBytes                 +=   ntfs_stats[n].ntfs.MftBitmapReadBytes;
                ntfs_stats[0].ntfs.MftBitmapWrites                    +=   ntfs_stats[n].ntfs.MftBitmapWrites;
                ntfs_stats[0].ntfs.MftBitmapWriteBytes                +=   ntfs_stats[n].ntfs.MftBitmapWriteBytes;
 
                ntfs_stats[0].ntfs.MftBitmapWritesFlushForLogFileFull +=   ntfs_stats[n].ntfs.MftBitmapWritesFlushForLogFileFull;
                ntfs_stats[0].ntfs.MftBitmapWritesLazyWriter          +=   ntfs_stats[n].ntfs.MftBitmapWritesLazyWriter;
                ntfs_stats[0].ntfs.MftBitmapWritesUserRequest         +=   ntfs_stats[n].ntfs.MftBitmapWritesUserRequest;
 
                ntfs_stats[0].ntfs.MftBitmapWritesUserLevel.Write     +=   ntfs_stats[n].ntfs.MftBitmapWritesUserLevel.Write;
                ntfs_stats[0].ntfs.MftBitmapWritesUserLevel.Create    +=   ntfs_stats[n].ntfs.MftBitmapWritesUserLevel.Create;
                ntfs_stats[0].ntfs.MftBitmapWritesUserLevel.SetInfo   +=   ntfs_stats[n].ntfs.MftBitmapWritesUserLevel.SetInfo;
                ntfs_stats[0].ntfs.MftBitmapWritesUserLevel.Flush     +=   ntfs_stats[n].ntfs.MftBitmapWritesUserLevel.Flush;
 
                ntfs_stats[0].ntfs.UserIndexReads                     +=   ntfs_stats[n].ntfs.UserIndexReads;
                ntfs_stats[0].ntfs.UserIndexReadBytes                 +=   ntfs_stats[n].ntfs.UserIndexReadBytes;
                ntfs_stats[0].ntfs.UserIndexWrites                    +=   ntfs_stats[n].ntfs.UserIndexWrites;
                ntfs_stats[0].ntfs.UserIndexWriteBytes                +=   ntfs_stats[n].ntfs.UserIndexWriteBytes;
 
                ntfs_stats[0].ntfs.LogFileReads                       +=   ntfs_stats[n].ntfs.LogFileReads;
                ntfs_stats[0].ntfs.LogFileReadBytes                   +=   ntfs_stats[n].ntfs.LogFileReadBytes;
                ntfs_stats[0].ntfs.LogFileWrites                      +=   ntfs_stats[n].ntfs.LogFileWrites;
                ntfs_stats[0].ntfs.LogFileWriteBytes                  +=   ntfs_stats[n].ntfs.LogFileWriteBytes;
                                                                      
                ntfs_stats[0].ntfs.Allocate.Calls                     +=   ntfs_stats[n].ntfs.Allocate.Calls;
                ntfs_stats[0].ntfs.Allocate.Clusters                  +=   ntfs_stats[n].ntfs.Allocate.Clusters;
                ntfs_stats[0].ntfs.Allocate.Hints                     +=   ntfs_stats[n].ntfs.Allocate.Hints;
                                                                                                                          
                ntfs_stats[0].ntfs.Allocate.RunsReturned              +=   ntfs_stats[n].ntfs.Allocate.RunsReturned;
                                                                      
                ntfs_stats[0].ntfs.Allocate.HintsHonored              +=   ntfs_stats[n].ntfs.Allocate.HintsHonored;
                ntfs_stats[0].ntfs.Allocate.HintsClusters             +=   ntfs_stats[n].ntfs.Allocate.HintsClusters;
                ntfs_stats[0].ntfs.Allocate.Cache                     +=   ntfs_stats[n].ntfs.Allocate.Cache;
                ntfs_stats[0].ntfs.Allocate.CacheClusters             +=   ntfs_stats[n].ntfs.Allocate.CacheClusters;
                ntfs_stats[0].ntfs.Allocate.CacheMiss                 +=   ntfs_stats[n].ntfs.Allocate.CacheMiss;
                ntfs_stats[0].ntfs.Allocate.CacheMissClusters         +=   ntfs_stats[n].ntfs.Allocate.CacheMissClusters;
            }


            const int n = 0; 
            {
            cout << "FS# " << n << "\n";
            cout << "     "
                << ((ntfs_stats[n].fs.FileSystemType == FILESYSTEM_STATISTICS_TYPE_NTFS) ?  " NTFS" : "Other")
                << "  Type\n" 
                << setw(10) << ntfs_stats[n].fs.UserFileReads       << "  UserFileReads       \n" 
                << setw(10) << ntfs_stats[n].fs.UserFileReadBytes   << "  UserFileReadBytes   \n" 
                << setw(10) << ntfs_stats[n].fs.UserDiskReads       << "  UserDiskReads       \n" 
                << setw(10) << ntfs_stats[n].fs.UserFileWrites      << "  UserFileWrites      \n" 
                << setw(10) << ntfs_stats[n].fs.UserFileWriteBytes  << "  UserFileWriteBytes  \n" 
                << setw(10) << ntfs_stats[n].fs.UserDiskWrites      << "  UserDiskWrites      \n" 
                                                                                 
                << setw(10) << ntfs_stats[n].fs.MetaDataReads       << "  MetaDataReads       \n" 
                << setw(10) << ntfs_stats[n].fs.MetaDataReadBytes   << "  MetaDataReadBytes   \n" 
                << setw(10) << ntfs_stats[n].fs.MetaDataDiskReads   << "  MetaDataDiskReads   \n" 
                << setw(10) << ntfs_stats[n].fs.MetaDataWrites      << "  MetaDataWrites      \n" 
                << setw(10) << ntfs_stats[n].fs.MetaDataWriteBytes  << "  MetaDataWriteBytes  \n" 
                << setw(10) << ntfs_stats[n].fs.MetaDataDiskWrites  << "  MetaDataDiskWrites  \n" 
                << "\n"
#if 0
                << setw(10) << ntfs_stats[n].fs.SizeOfCompleteStructure   << " Size     \n" 
                << setw(10) << sizeof(ntfs_stats[0])                      << " NTFS size\n" 
                << setw(10) << bytesReturned                              << " Fcntl rSize\n" 
                << "\n" 
#endif
                ;

            cout  
                << setw(10) << ntfs_stats[n].ntfs.LogFileFullExceptions                    << " LogFileFullExceptions                \n"
                << setw(10) << ntfs_stats[n].ntfs.OtherExceptions                          << " OtherExceptions                      \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.MftReads                                 << " MftReads                             \n"
                << setw(10) << ntfs_stats[n].ntfs.MftReadBytes                             << " MftReadBytes                         \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.MftWrites                                << " MftWrites                            \n"
                << setw(10) << ntfs_stats[n].ntfs.MftWriteBytes                            << " MftWriteBytes                        \n"
                << setw(10) << ntfs_stats[n].ntfs.MftWritesUserLevel.Write                 << " MftWritesUserLevel.Write             \n"
                << setw(10) << ntfs_stats[n].ntfs.MftWritesUserLevel.Create                << " MftWritesUserLevel.Create            \n"
                << setw(10) << ntfs_stats[n].ntfs.MftWritesUserLevel.SetInfo               << " MftWritesUserLevel.SetInfo           \n"
                << setw(10) << ntfs_stats[n].ntfs.MftWritesUserLevel.Flush                 << " MftWritesUserLevel.Flush             \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.MftWritesFlushForLogFileFull             << " MftWritesFlushForLogFileFull         \n"
                << setw(10) << ntfs_stats[n].ntfs.MftWritesLazyWriter                      << " MftWritesLazyWriter                  \n"
                << setw(10) << ntfs_stats[n].ntfs.MftWritesUserRequest                     << " MftWritesUserRequest                 \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.Mft2Writes                               << " Mft2Writes                           \n"
                << setw(10) << ntfs_stats[n].ntfs.Mft2WriteBytes                           << " Mft2WriteBytes                       \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.Mft2WritesUserLevel.Write                << " Mft2WritesUserLevel.Write            \n"
                << setw(10) << ntfs_stats[n].ntfs.Mft2WritesUserLevel.Create               << " Mft2WritesUserLevel.Create           \n"
                << setw(10) << ntfs_stats[n].ntfs.Mft2WritesUserLevel.SetInfo              << " Mft2WritesUserLevel.SetInfo          \n"
                << setw(10) << ntfs_stats[n].ntfs.Mft2WritesUserLevel.Flush                << " Mft2WritesUserLevel.Flush            \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.Mft2WritesFlushForLogFileFull            << " Mft2WritesFlushForLogFileFull        \n"
                << setw(10) << ntfs_stats[n].ntfs.Mft2WritesLazyWriter                     << " Mft2WritesLazyWriter                 \n"
                << setw(10) << ntfs_stats[n].ntfs.Mft2WritesUserRequest                    << " Mft2WritesUserRequest                \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.RootIndexReads                           << " RootIndexReads                       \n"
                << setw(10) << ntfs_stats[n].ntfs.RootIndexReadBytes                       << " RootIndexReadBytes                   \n"
                << setw(10) << ntfs_stats[n].ntfs.RootIndexWrites                          << " RootIndexWrites                      \n"
                << setw(10) << ntfs_stats[n].ntfs.RootIndexWriteBytes                      << " RootIndexWriteBytes                  \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.BitmapReads                              << " BitmapReads                          \n"
                << setw(10) << ntfs_stats[n].ntfs.BitmapReadBytes                          << " BitmapReadBytes                      \n"
                << setw(10) << ntfs_stats[n].ntfs.BitmapWrites                             << " BitmapWrites                         \n"
                << setw(10) << ntfs_stats[n].ntfs.BitmapWriteBytes                         << " BitmapWriteBytes                     \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.BitmapWritesFlushForLogFileFull          << " BitmapWritesFlushForLogFileFull      \n" 
                << setw(10) << ntfs_stats[n].ntfs.BitmapWritesLazyWriter                   << " BitmapWritesLazyWriter               \n"
                << setw(10) << ntfs_stats[n].ntfs.BitmapWritesUserRequest                  << " BitmapWritesUserRequest              \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.BitmapWritesUserLevel.Write              << " BitmapWritesUserLevel.Write          \n"
                << setw(10) << ntfs_stats[n].ntfs.BitmapWritesUserLevel.Create             << " BitmapWritesUserLevel.Create         \n"
                << setw(10) << ntfs_stats[n].ntfs.BitmapWritesUserLevel.SetInfo            << " BitmapWritesUserLevel.SetInfo        \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapReads                           << " MftBitmapReads                       \n"
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapReadBytes                       << " MftBitmapReadBytes                   \n"
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWrites                          << " MftBitmapWrites                      \n"
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWriteBytes                      << " MftBitmapWriteBytes                  \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWritesFlushForLogFileFull       << " MftBitmapWritesFlushForLogFileFull   \n"    
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWritesLazyWriter                << " MftBitmapWritesLazyWriter            \n"
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWritesUserRequest               << " MftBitmapWritesUserRequest           \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWritesUserLevel.Write           << " MftBitmapWritesUserLevel.Write       \n"
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWritesUserLevel.Create          << " MftBitmapWritesUserLevel.Create      \n" 
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWritesUserLevel.SetInfo         << " MftBitmapWritesUserLevel.SetInfo     \n"  
                << setw(10) << ntfs_stats[n].ntfs.MftBitmapWritesUserLevel.Flush           << " MftBitmapWritesUserLevel.Flush       \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.UserIndexReads                           << " UserIndexReads                       \n"
                << setw(10) << ntfs_stats[n].ntfs.UserIndexReadBytes                       << " UserIndexReadBytes                   \n"
                << setw(10) << ntfs_stats[n].ntfs.UserIndexWrites                          << " UserIndexWrites                      \n"
                << setw(10) << ntfs_stats[n].ntfs.UserIndexWriteBytes                      << " UserIndexWriteBytes                  \n"
                                                                                                                             
                << setw(10) << ntfs_stats[n].ntfs.LogFileReads                             << " LogFileReads                         \n"
                << setw(10) << ntfs_stats[n].ntfs.LogFileReadBytes                         << " LogFileReadBytes                     \n"
                << setw(10) << ntfs_stats[n].ntfs.LogFileWrites                            << " LogFileWrites                        \n"
                << setw(10) << ntfs_stats[n].ntfs.LogFileWriteBytes                        << " LogFileWriteBytes                    \n"

                << setw(10) << ntfs_stats[n].ntfs.Allocate.Calls                   << "  number of individual calls to allocate clusters                 \n"
                << setw(10) << ntfs_stats[n].ntfs.Allocate.Clusters                << "  number of clusters allocated                                     \n"
                << setw(10) << ntfs_stats[n].ntfs.Allocate.Hints                   << "  number of times a hint was specified                             \n"
                                                                                                                                      
                << setw(10) << ntfs_stats[n].ntfs.Allocate.RunsReturned            << "  number of runs used to satisify all the requests                 \n"

                << setw(10) << ntfs_stats[n].ntfs.Allocate.HintsHonored            << "  number of times the hint was useful                              \n"
                << setw(10) << ntfs_stats[n].ntfs.Allocate.HintsClusters           << "  number of clusters allocated via the hint                        \n"
                << setw(10) << ntfs_stats[n].ntfs.Allocate.Cache                   << "  number of times the cache was useful other than the hint         \n"
                << setw(10) << ntfs_stats[n].ntfs.Allocate.CacheClusters           << "  number of clusters allocated via the cache other than the hint   \n"
                << setw(10) << ntfs_stats[n].ntfs.Allocate.CacheMiss               << "  number of times the cache wasn't useful                          \n"
                << setw(10) << ntfs_stats[n].ntfs.Allocate.CacheMissClusters       << "  number of clusters allocated without the cache                   \n"
                << "\n\n";
            }
        }
        else
        {
            cout << "\n **** Failed to get Filesystem Statistics on " << filename << ", Error=" << err << "\n\n";
        }

        int nDrive = 0;
        char szDevice[MAX_PATH];
        _snprintf (szDevice, sizeof(szDevice), "\\\\.\\PhysicalDrive%d", nDrive);
        HANDLE hDevice = CreateFile (szDevice, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
			      NULL, OPEN_EXISTING, 0, NULL);

        DISK_CACHE_INFORMATION diskCacheInformation;

        ctlResult = DeviceIoControl(
                hDevice, 
                IOCTL_DISK_GET_CACHE_INFORMATION,   // dwIoControlCode
                NULL,                               // lpInBuffer
                0,                                  // nInBufferSize
                &diskCacheInformation,              // output buffer
                sizeof(diskCacheInformation),       // size of output buffer
                &bytesReturned,                     // number of bytes returned
                0);
        err = GetLastError();

        if (ctlResult != 0 || err == ERROR_MORE_DATA)
        {
            cout 
                << setw(10) << diskCacheInformation.WriteCacheEnabled << " WriteCacheEnabled\n"
                << "\n";
        }
        else
        {
            cout << "\n *** Failed to access disk cache info, error=" << err << "\n\n";
        }


        DISK_PERFORMANCE diskPerformance;
        SecureZeroMemory(&diskPerformance, sizeof(diskPerformance));

        ctlResult = DeviceIoControl(
                hDevice, 
                IOCTL_DISK_PERFORMANCE,        // dwIoControlCode
                NULL,                          // lpInBuffer
                0,                             // nInBufferSize
                &diskPerformance,              // output buffer
                sizeof(diskPerformance),       // size of output buffer
                &bytesReturned,                // number of bytes returned
                0);
        err = GetLastError();

        if (ctlResult != 0 || err == ERROR_INSUFFICIENT_BUFFER)
        {
            cout 
                << "Disk Performance\n"
                 << setw(15) << diskPerformance.BytesRead.QuadPart        << "  BytesRead    \n"
                 << setw(15) << diskPerformance.BytesWritten.QuadPart     << "  BytesWritten \n"
                 << setw(15) << diskPerformance.ReadTime.QuadPart         << "  ReadTime     \n"
                 << setw(15) << diskPerformance.WriteTime.QuadPart        << "  WriteTime    \n"
                 << setw(15) << diskPerformance.IdleTime.QuadPart         << "  IdleTime     \n"
                 << setw(15) << diskPerformance.ReadCount                 << "  ReadCount    \n"
                 << setw(15) << diskPerformance.WriteCount                << "  WriteCount   \n"
                 << setw(15) << diskPerformance.QueueDepth                << "  QueueDepth   \n"
                 << setw(15) << diskPerformance.SplitCount                << "  SplitCount   \n"
                 << setw(15) << diskPerformance.QueryTime.QuadPart        << "  QueryTime    \n"
                 << setw(15) << diskPerformance.StorageDeviceNumber       << "  StorageDeviceNumber  \n"
                 << "\n";
            // StorageManagerName[8];
        }
        else
        {
            cout << "\n  *** Failed to access disk performance, err=" << err << "\n\n";
        }

        CloseHandle(hDevice);
        CloseHandle(hFile);
    }
}









