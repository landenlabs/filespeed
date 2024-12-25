//-----------------------------------------------------------------------------
// FileSpeed
// Author:  DLang   2009  v2.7
//-----------------------------------------------------------------------------


#include <windows.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <io.h>

#include "Options.h"
#include "dirscan.h"
#include "ListOpenHandles.h"
#include "FileSysInfo.h"

using  namespace std;


const size_t KB = (1<<10);
const size_t MB = (1<<20);
const size_t GB = (1<<30);

const char usage[] =
    "FileSpeed v2.7   - " __DATE__ "\n"
    "\n"
    "  -D                ; generate directory listing\n"
    "   -o <file>        ; save output to file, null disables output\n"
    "   -x <num>         ; maximum files to output\n"
    "\n"
    "  -F                ; measure File read performance\n"
    "   -a <#pieces>     ; random access, use with -m\n"
    "   -b <blockSizeKB> ; #KiloBytes to read per transaction, 0=Max which is 8000\n"
    "   -d s|r           ; open files with hint Sequencial or Random access, default is none\n"
    "   -h               ; disable file sharing in OPEN call\n"
    "   -i <infile>      ; file contains list of files to read\n"
    "   -k <#files>      ; keep #files open during scan, default is 0\n"
    "   -l <csvFile>     ; save results in CSV log file\n"
    "   -m <maxMB>       ; maximum Megabytes to read per file, 0=entire file\n"
    "   -r <repeat#>     ; repeat test\n"
    "   -s <seconds>     ; time limit of test in seconds, 0=forever till all files read \n"
    "   -t <seconds>     ; have often to report performance, 0=once at end, default is 10\n"
    "   -u               ; unbuffered i/o, only works on local file access, bypass file cache\n"
    "   -w <waitMinutes> ; if Repeat >1, wait between runs\n"
    "\n"
    "  -C                ; Measure Cache file size\n"
    "   -c min,max,step; Megabyte range, default is 20,200,20\n"
    "\n"
    "  -S               ; Dump NTFS Statistics\n"
    "\n"
    "  -L               ; List Open Files\n"
    "    -p processName ;   Limit file list to specified processName \n"
    "    -y s|m|a       ;   Report s=summary, m=modules, a=all \n"
    "    -r <repeat#>   ;   Repeat output after waitMinutes \n"
    "    -w <waitMin>   ;   WaitMinutes defaults to 0.1 \n"
    "\n"
    "\n"
    "Example:\n"
    "  Generate directory file list\n"
    "   filespeed -D -x 1000 -o filelist.txt d:\\wxdata\\image\\*.wimg  \n"
    "   filespeed -D -x 1000 -o null d:\\wxdata\\image  \n"
    "\n"
    "  Performance Read test\n"
    "   filespeed -F -i filelist.txt \n"
    "   filespeed -F -x 1000 -s 60 d:\\wxdata\\image\\*.dat \n"
    "   filespeed -F -w 10 -r  6 -i filelist.txt    ; wait 10 minutes, repeat 6 times\n" 
    "\n"
    "  Measure File Cache Size\n"
    "   filespeed -C  ; default \n"
    "   filespeed -C -c 1000,2600,100 d:\\file1.dat d:\\file2.dat   ; specify cacge suze & files \n"
    "   filespeed -C -c 1000,2600,100 c:\\c-file.dat d:\\d-file.dat ; spread across two drives\n"
    "\n"                                                  
    "\n";

static char  sPerfHdr[] = 
"   Local,       ,  Sample,       , Read     ,     Rate, time to open,  total time, avg open\n" 
"    Time,   Desc, Seconds,  Files, MegaBytes,     MB/s, (Max seconds), open (sec), seconds\n\n";
static const char sPerfFmt[] =  
"%s, %6s, %7.3f, %6u, %9.1f, %8.3f, %13.4f, %10.3f, %8.4f\n"; 

/* ------------------------------------------------------------------------- */
#ifndef HAVE_GETOPT
const char *optarg = NULL; /* argument associated with option */
int opterr   = 0;       /* if error message should be printed */
int optind   = 1;       /* index into parent argv vector */
int optopt   = 0;       /* character checked for validity */

#define BADCH (int)'?'
#define EMSG ""

/* ------------------------------------------------------------------------- */
int getopt(int nargc, const char * nargv[], const char *ostr)
{
	static const char* place = EMSG; /* option letter processing */
	const char* oli; /* option letter list index */
	const char* p;
	
	if (!*place) 
    { /* update scanning pointer */
		if (optind >= nargc || *(place = nargv[optind]) != '-') {
			place = EMSG;
			return (EOF);
		}
		if (place[1] && *++place == '-') 
        { /* found "--" */
			++optind;
			place = EMSG;
			return (EOF);
		}
	} /* option letter okay? */

	if ((optopt = (int)*place++) == (int)':' ||
		!(oli = strchr(ostr, optopt))) 
    {
		/*
		* For backwards compatibility: don't treat '-' as an
		* option letter unless caller explicitly asked for it.
		*/
		if (optopt == (int)'-')
			return (EOF);
		if (!*place)
			++optind;
		if (opterr) {
			if (!(p = strrchr(*nargv, '/')))
				p = *nargv;
			else
				++p;
			fprintf(stderr, "%s: illegal option -- %c\n", p, optopt);
		}
		return (BADCH);
	}

	if (*++oli != ':') 
    { /* don't need argument */
		optarg = NULL;
		if (!*place)
			++optind;
	} else { /* need an argument */
		if (*place) /* no white space */
			optarg = place;
		else if (nargc <= ++optind) 
        { /* no arg */
			place = EMSG;
			if (!(p = strrchr(*nargv, '/')))
				p = *nargv;
			else
				++p;
			if (opterr)
				fprintf(stderr, "%s: option requires an argument -- %c\n",
				p, optopt);
			return (BADCH);
		} else /* white space */
			optarg = nargv[optind];
		place = EMSG;
		++optind;
	}

	return (optopt); /* dump back option letter */
}
#endif

// ---------------------------------------------------------------------------
template <class T>
inline T Max(T v1, T v2)
{
    return (v1 > v2) ? v1 : v2;
}

// ---------------------------------------------------------------------------
void DirFilecb(
        void* cbData, 
        const char* pDir, 
        WIN32_FIND_DATA* pFileData,
        int depth)      // 0...n is directory depth, -n end-of nth diretory
{
    Options* pOptions = (Options*)cbData;

    assert(pDir != NULL);
    bool isDir = ((pFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

    if (pOptions->maxFiles != 0 && pOptions->fileCount >= pOptions->maxFiles)
        return;

    if (isDir)
    {
        pOptions->dirCount++;
    }
    else
    {
        if (pOptions->outFILE)
            fprintf_s(pOptions->outFILE, "%s\\%s\n", pDir, pFileData->cFileName);
        pOptions->fileCount++;

        LARGE_INTEGER fileSize;
        fileSize.HighPart = pFileData->nFileSizeHigh;
        fileSize.LowPart  = pFileData->nFileSizeLow;

        pOptions->fileSize.QuadPart += fileSize.QuadPart;
    }
}

//-----------------------------------------------------------------------------
int DirListing(int nDir, const char* pDirs[], Options& options)
{
    // Setup default as needed.
    if (nDir == 0)
    {
        const char* sDefDir[] = {"*"};
        nDir  = sizeof(sDefDir)/sizeof(sDefDir[0]);
        pDirs = sDefDir;
    }

    // Get current directory as default starting point.
    char currentDir[MAX_PATH];
    GetCurrentDirectory(sizeof(currentDir), currentDir);

    DirectoryScan dirScan;
    dirScan.m_add_cb     = DirFilecb;
    dirScan.m_cb_data    = &options;
    dirScan.m_filesFirst = dirScan.m_recurse = true;

    // Iterate over dir patterns.
    size_t nFiles = 0;
    DWORD startTick = GetTickCount();
    for (int dirIdx=0; dirIdx < nDir; dirIdx++)
    {
        dirScan.Init(pDirs[dirIdx], currentDir);
        nFiles += dirScan.GetFilesInDirectory();
    }
    DWORD endTick = GetTickCount();

    double seconds = (endTick - startTick)/ 1000.0;

    cout << "DirScan ";
    cout << seconds << " seconds to Scan";
    cout << " Directories:" << options.dirCount;
    cout << ", Files:" << options.fileCount; 
    cout << ", Size:" << options.fileSize.QuadPart / (double)MB << " MB";
    cout << ", " << options.fileCount / seconds << " files/sec";
    cout << "\n";

    return (int)nFiles;
}

//-----------------------------------------------------------------------------

int MeasureFileRead(int nArg, const char* pArg[], Options& options)
{
    char outLine[256];
    size_t outRow = 0;
    if (options.inFilename.empty() && nArg == 0)
        return 0;

    options.fileCount = options.dirCount = options.badCount = 0;
    options.fileSize.QuadPart = 0;

    FILE* inFILE = NULL;
    if (options.inFilename.empty() == false)
    {
        inFILE = fopen(options.inFilename.c_str(), "r");
        if (inFILE == NULL)
        {
            perror(options.inFilename.c_str());
            return -errno;
        }
    }
    else 
    {
        options.outFILE = tmpfile();
        // options.outFILE = fopen(".filelist.tmp", "w+");
        DirListing(nArg, pArg, options);
        inFILE = options.outFILE;
        options.outFILE = 0;
        fseek(inFILE, 0, SEEK_SET);
    }

    size_t nFileHnd = options.keepOpen + 2;
    HANDLE* hFiles = new HANDLE[nFileHnd];
    size_t  hOpenIdx  = 0;
    size_t  hCloseIdx = 0;
    memset(hFiles, -1, sizeof(HANDLE) * nFileHnd); 

    options.fileCount = options.dirCount = options.badCount = 0;
    options.fileSize.QuadPart = 0;

    size_t sampleFileCount = 0;
    LARGE_INTEGER sampleFileSize;
    sampleFileSize.QuadPart = 0;

    double openSeconds = 0;
    double openMaxSeconds = 0;
    double sampleOpenSeconds = 0;
    double sampleMaxOpenSeconds = 0;

    DWORD sampleStartTick;
    DWORD startTick = sampleStartTick = GetTickCount();
    DWORD endTick = (options.maxSeconds == 0) ? -1 : (startTick + options.maxSeconds * 1000);
    size_t lastSeconds = options.reportSeconds;

    char lbuffer[MAX_PATH];
    while (fgets(lbuffer, sizeof(lbuffer), inFILE) != NULL && GetTickCount() < endTick)
    {
        int len = strlen(lbuffer);
        lbuffer[len-1] = '\0';       // remove newline

        DWORD fileFlag = FILE_ATTRIBUTE_READONLY;
        switch (options.openFlag)
        {
        case 's':   fileFlag |= FILE_FLAG_SEQUENTIAL_SCAN; break;
        case 'r':   fileFlag |= FILE_FLAG_RANDOM_ACCESS; break;
        }

        if (options.unbufferedIO) 
            fileFlag |=  FILE_FLAG_NO_BUFFERING;

        DWORD shareFlag =  FILE_SHARE_READ | FILE_SHARE_WRITE;
        if (options.noShare)
            shareFlag = 0;

        DWORD openStartTick = GetTickCount();
        HANDLE hFile = 
            CreateFile(lbuffer, 
                GENERIC_READ,
                shareFlag,
                0,
                OPEN_EXISTING,
                fileFlag,
                0);

        DWORD openEndTick = GetTickCount();
        double seconds = (openEndTick - openStartTick) / 1000.0;
        openSeconds += seconds;
        openMaxSeconds = Max(openMaxSeconds, seconds);
        sampleOpenSeconds += seconds;
        sampleMaxOpenSeconds = Max(sampleMaxOpenSeconds, seconds);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            cerr << "Error: " << err << lbuffer << "\n";
            options.badCount++;
            continue;
        }
          
        if (options.maxReadMB >= 0)
        {
            const DWORD sMaxRecSize = MB*8;
            const DWORD bytesPerSector = 512;   // TODO - ask filesystem for sector size.
            static char sBuffer[sMaxRecSize+1024];

            // For unbuffered I/O buffer must be on sector boundary
            char* buffer = (char*)(ULONG_PTR(sBuffer + bytesPerSector-1) & ~ULONG_PTR(bytesPerSector-1));

            DWORD recSize = (options.recSize != 0) ? options.recSize : sMaxRecSize;
            recSize = (recSize <= sMaxRecSize) ? recSize : sMaxRecSize;

            DWORD bytesRead = 0;
            if (options.maxReadMB > 0 && recSize > MB * options.maxReadMB)
                recSize = DWORD(MB * options.maxReadMB);

            if (options.accessPieces > 1)
            {
                // Special case to test semi-random file access
                // Divide file into pieces and seek to each piece to read some data.

                BY_HANDLE_FILE_INFORMATION handleInfo;
                int ok = GetFileInformationByHandle(hFile, &handleInfo);
                LARGE_INTEGER rdFileSize;
                rdFileSize.LowPart = handleInfo.nFileSizeLow;
                rdFileSize.HighPart = handleInfo.nFileSizeHigh;
                LARGE_INTEGER rdFileSeek;
                rdFileSeek.QuadPart = rdFileSize.QuadPart / options.accessPieces;

                DWORD rdRecSize = recSize / options.accessPieces;
                if (recSize < 16)
                    rdRecSize = 16;

                while (ReadFile(hFile, buffer, rdRecSize, &bytesRead, 0) && bytesRead > 0)
                {
                    // cout << "Read size:" << bytesRead << "\n";

                    sampleFileSize.QuadPart   += bytesRead;
                    options.fileSize.QuadPart += bytesRead;
                    if (options.maxReadMB != 0 && options.fileSize.QuadPart/MB >= options.maxReadMB)
                        break;  // exit early
                    SetFilePointer(hFile, rdFileSeek.LowPart, &rdFileSeek.HighPart, FILE_CURRENT); 

                    LARGE_INTEGER currentPos;
                    currentPos.QuadPart = 0;
                    SetFilePointerEx(hFile, currentPos, &currentPos, FILE_CURRENT); 
                    // cout << " position:" << currentPos.QuadPart << "\n";
                }
            }
            else
            {
                while (ReadFile(hFile, buffer, recSize, &bytesRead, 0) && bytesRead > 0)
                {
                    sampleFileSize.QuadPart   += bytesRead;
                    options.fileSize.QuadPart += bytesRead;
                    if (options.maxReadMB != 0 && options.fileSize.QuadPart/(double)MB >= options.maxReadMB)
                        break;  // exit early
                }
            }
        }

        options.fileCount++;
        sampleFileCount++;
        hFiles[hOpenIdx] = hFile;
        hOpenIdx = (hOpenIdx + 1) % nFileHnd;
        while ((hOpenIdx + nFileHnd - hCloseIdx) % nFileHnd > options.keepOpen)
        {
            CloseHandle(hFiles[hCloseIdx]);
            hFiles[hCloseIdx] = INVALID_HANDLE_VALUE; 
            hCloseIdx = (hCloseIdx + 1) % nFileHnd;
        }

        seconds = (GetTickCount() - startTick) / 1000.0;
        if (options.showDetail || (seconds >= lastSeconds && options.reportSeconds > 0))
        {
            time_t now;
            time(&now);
            struct tm* nowTm;
            nowTm = localtime(&now);
            char timeStr[50];
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", nowTm);

            double sampleSeconds = (GetTickCount() - sampleStartTick) / 1000.0;

            lastSeconds = int(seconds) + options.reportSeconds;
         
            _snprintf(outLine, sizeof(outLine), 
                sPerfFmt,
                timeStr,
                "sample",
                sampleSeconds,  
                sampleFileCount,
                sampleFileSize.QuadPart / (double)MB, 
                sampleFileSize.QuadPart / (MB * sampleSeconds),
                sampleMaxOpenSeconds,
                sampleOpenSeconds,
                sampleOpenSeconds/sampleFileCount
                );
           
            if (outRow++ == 0)
                cout << "\n" << sPerfHdr;
            cout << outLine;

            if (options.logFILE != NULL)
            {
                // sample/total,  seconds, files, mb, mb/s
                fprintf(options.logFILE, outLine);
                fflush(options.logFILE);
            }

            sampleStartTick = GetTickCount();
            sampleFileSize.QuadPart = 0;
            sampleFileCount = 0;
            sampleOpenSeconds = 0;
            sampleMaxOpenSeconds = 0;
        }
    }

    endTick = GetTickCount();
    double seconds = (endTick - startTick) / 1000.0;
    time_t now;
    time(&now);
    struct tm* nowTm;
    nowTm = localtime(&now);
    char timeStr[50];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", nowTm);

    if (sampleFileCount != 0)
    {
        double sampleSeconds = (GetTickCount() - sampleStartTick) / 1000.0;
        lastSeconds = int(seconds) + options.reportSeconds;
        _snprintf(outLine, sizeof(outLine), 
            sPerfFmt,
            timeStr,
            "sample", 
            sampleSeconds,  
            sampleFileCount,
            sampleFileSize.QuadPart / (double)MB, 
            sampleFileSize.QuadPart / (MB * sampleSeconds),
            sampleMaxOpenSeconds,
            sampleOpenSeconds,
            sampleOpenSeconds/sampleFileCount
            );

        cout << outLine;

        // only export this sample if its period is atleast 95% of sample interval.
        if (options.logFILE != NULL && sampleSeconds/options.reportSeconds > 0.95)
        {
            fprintf(options.logFILE, outLine);
            fflush(options.logFILE);
        }
    }

    _snprintf(outLine, sizeof(outLine),
            sPerfFmt,
            timeStr,
            "total",
            seconds,  
            options.fileCount,
            options.fileSize.QuadPart / (double)MB, 
            options.fileSize.QuadPart / (MB * seconds),
            openMaxSeconds,
            openSeconds,
            openSeconds / options.fileCount
            );

    cout << "\n" << outLine << "\n";

    if (options.logFILE != NULL)
    {
        fprintf(options.logFILE, outLine);
        fflush(options.logFILE);
    }

    fclose(inFILE);
    delete [] hFiles;
    return 0;
}




//-----------------------------------------------------------------------------
inline void ToggleBool(bool& b, const char* arg, char value)
{
    b  = (strchr(arg, value) != NULL) ? !b : b;
}

//-----------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    ListOpenHandles listOpenHandles;
    Options options;
    options.outFILE = stdout;
    char cmd = 'd';
    
    int c;
    while ((c = getopt(argc, argv, "CDFLSa:b:c:d:hi:k:l:m:o:p:r:s:t:vw:x:y:")) != -1)
    {
        if (c >= 'A' && c <= 'Z')
        {
            cmd = c;
        }
        else
        {
            switch (c)
            {
              case 'a':     // accessPieces, used with -m
                // file and maxReadMB are divided by #accessPieces
                //  seekOffset = fileSize/pieces
                //  readPieces = maxReadMB/pieces
                //  seek to piece offset, read piece bytes, repeat
                options.accessPieces = atoi(optarg);
                break;

              case 'b':     // block recSize (File Performance command)
                options.recSize = (size_t)(atof(optarg) * KB);
                break;

              case 'c':     // min,max,step cache scan szie
                options.minCacheMB = atof(optarg);
                options.maxCacheMB = options.minCacheMB + 800;
                if (strchr(optarg, ',') != NULL)
                {
                    optarg = strchr(optarg, ',')+1;
                    options.maxCacheMB = atof(optarg);
                }
                if (strchr(optarg, ',') != NULL)
                {
                    optarg = strchr(optarg, ',')+1;
                    options.stepCacheMB = atof(optarg);
                }
                break;

              case 'd':   // open flags
                options.openFlag = optarg[0];
                break;

              case 'h':   // no sHare  - disable file sharing during open
                options.noShare = true;
                break;

              case 'i':     // input file list (Performance command)
                options.inFilename = optarg;
                break;

              case 'k':     // keep files open (File Performance command)
                options.keepOpen = atoi(optarg);
                break;

              case 'l':
                  options.logFilename = optarg;
                  options.logFILE = fopen(optarg, "w");
                  if (options.logFILE == NULL)
                  {
                      perror(optarg);
                      exit(errno);
                  }
                  // sample/total,  seconds, files, mb, mb/s
                  fprintf(options.logFILE, sPerfHdr);
                  break;

              case 'm':     // max readMB (File Performance command)
                options.maxReadMB = atof(optarg);
                break;

              case 'o':     // output filename (DirList command)
                if (stricmp(optarg, "null") == 0)
                    options.outFILE = NULL;
                else
                {
                    options.outFilename = optarg;
                    options.outFILE = fopen(optarg, "w");
                    if (options.outFILE == NULL)
                    {
                        perror(optarg);
                        exit(errno);
                    }
                }
                break;

              case 'r':     // repeat count (File Performance command)
                options.repeat = atoi(optarg);
                break;

              case 's':     // max seconds (File Performance command)
                options.maxSeconds = atoi(optarg);
                break;

              case 't':     // report seconds (File Performance command)
                options.reportSeconds = atoi(optarg);
                break;

              case 'u':   // unbuffered i/o
                options.unbufferedIO = true;
                break;

              case 'v':     // verbose
                options.showDetail = true;
                break;

             case 'w':     // waitMinutes (File Performance command)
                options.waitMinutes = atof(optarg);
                break;

              case 'x':     // max file limit (DirListing command)
                options.maxFiles = atoi(optarg);
                break;

            // ---- List Open Handle specific options
              case 'p':
                listOpenHandles.m_process = optarg;
                listOpenHandles.m_processId = atoi(optarg);
                if (listOpenHandles.m_processId != 0)
                    listOpenHandles.m_process.resize(0);
                break;

              case 'y':
                ToggleBool(listOpenHandles.m_showSummary, optarg, 's');
                ToggleBool(listOpenHandles.m_showModules, optarg, 'm');
                ToggleBool(listOpenHandles.m_showFiles, optarg, 'f');
                ToggleBool(listOpenHandles.m_showEvents, optarg, 'e');
                ToggleBool(listOpenHandles.m_showAll, optarg, 'a');
                ToggleBool(listOpenHandles.m_showHex, optarg, 'x');
                ToggleBool(listOpenHandles.m_showProcInfo, optarg, 'p');
                break;
            }
        }
    }

    int resultCode = 0;

    switch (cmd)
    {
    case 'C':
        resultCode = MeasureCacheFileSize(argc-optind, argv+optind, options);
        break;

    case 'D':
        resultCode = DirListing(argc-optind, argv+optind, options);
        break;

    case 'F':
        if (options.accessPieces > 1 && options.maxReadMB == 0)
            options.accessPieces = 0;

        cout << "FileSpeed options:\n";
        if (options.keepOpen != 0)
            cout << "  KeepOpen     " << options.keepOpen << "\n";
        if (options.inFilename.empty() == false)
            cout << "  InFile:      " << options.inFilename.c_str() << "\n";
        if (options.unbufferedIO)
            cout << "  Unbuffered IO\n";
        if (options.noShare)
            cout << "  No file sharing\n";
        switch (options.openFlag)
        {
        case 's':   cout << "  FileFlag is Sequencial\n";   break;
        case 'r':   cout << "  FileFlag is Random\n";   break;
        }
        if (options.maxFiles > 0)
            cout << "  MaxFiles:    " << options.maxFiles << "\n";
        if (options.recSize > 0)
            cout << "  RecSize:     " << (float)options.recSize / KB << " KB\n";
        if (options.maxReadMB > 0)
            cout << "  MaxReadMb:   " << options.maxReadMB << " MB, ("
                                      << options.maxReadMB*MB/KB << " KB)\n";
        if (options.accessPieces > 0)
            cout << "  #Pieces:     " << options.accessPieces << "\n";
        if (options.maxSeconds > 0)
            cout << "  MaxSeconds:  " << options.maxSeconds << "\n";
        if (options.reportSeconds > 0)
            cout << "  ReportSec:   " << options.reportSeconds << "\n";
        if (options.repeat > 1)
            cout << "  Repeat:      " << options.repeat << "\n";
        if (options.waitMinutes > 0 && options.repeat > 1)
            cout << "  WaitMinutes: " << options.waitMinutes << "\n";
       
        cout << "\n\n";

        for (int testIdx=0; testIdx < (int)options.repeat; testIdx++)
        {
            resultCode = MeasureFileRead(argc-optind, argv+optind, options);

            if (options.repeat > 1 && options.waitMinutes > 0)
            {
                cout << "FileSpeed (Repeat#" << testIdx+1 << " of " << options.repeat << ") ";

                int minutes;
                for (minutes = 0; minutes < options.waitMinutes; minutes++)
                {
                    cout << "   sleeping " << options.waitMinutes - minutes << " minutes\n";
                    Sleep(60000);
                }
                
                int milliseconds = int((options.waitMinutes - minutes) * 60000);
                if (milliseconds > 0)
                    Sleep(milliseconds);
            }
            cout << "\n";
        }
        if (options.logFILE != NULL)
            fclose(options.logFILE);
        break;

    case 'L':
        {
            listOpenHandles.m_filename = options.inFilename;
            listOpenHandles.m_logFilename = options.logFilename;
            listOpenHandles.m_repeat = options.repeat;
            listOpenHandles.m_waitMin = options.waitMinutes;

            if (options.outFilename.empty() == false)
            {
                std::ofstream outf(options.outFilename.c_str());
                if (outf.good())
                {
                    listOpenHandles.ReportOpenFiles(outf);
                    cout << "List Open Handle report saved to:" << options.outFilename << "\n";
                    break;
                }
            }
            listOpenHandles.ReportOpenFiles(cout);
        }
        break;

    case 'S':
        DumpFileStats(argc-optind, argv+optind, options);
        break;

    default:
        std::cout << usage;
    }
	return resultCode;
}

