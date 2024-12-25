//-----------------------------------------------------------------------------
// Options
// Author:  DLang   2009  
//-----------------------------------------------------------------------------

#pragma once

#include <stdio.h>
#include <string>
#include <windows.h>

struct Options
{
    Options() : 
        maxFiles(0), maxSeconds(0), waitMinutes(0), reportSeconds(10),
        repeat(1), recSize(0), maxReadMB(0), 
        unbufferedIO(false), noShare(false), openFlag(' '),
        inFilename(), outFILE(0), logFILE(0), showDetail(false),
        minCacheMB(20), maxCacheMB(200), stepCacheMB(20),
        accessPieces(0), keepOpen(0),
        dirCount(0), fileCount(0), badCount(0), error(0) 
        { 
            fileSize.QuadPart = 0; 
        }

    // Controls/behavior
    size_t          maxFiles;       // 0=all files
    size_t          maxSeconds;     // 0=forever
    double          waitMinutes;    // use with repeat
    size_t          reportSeconds;  // how often to report performance 

    size_t          repeat;
    size_t          recSize;        // 0=default to internal max rec size.
    double          maxReadMB;      // 0=entire file

    bool            unbufferedIO;   // true enable FILE_FLAG_NO_BUFFERING
    bool            noShare;        // true disables file sharing
    char            openFlag;       // s=sequencial, r=random

    std::string     inFilename;
    FILE*           outFILE;
    std::string     outFilename;
    FILE*           logFILE;
    std::string     logFilename;
    bool            showDetail;
    double          minCacheMB, maxCacheMB, stepCacheMB;
    size_t          accessPieces;   // 0 = sequencial access, n divides files up.
    size_t          keepOpen;       // #files to leave open (rolling list)

    // Stats
    size_t          dirCount;
    size_t          fileCount;
    size_t          badCount;
    DWORD           error;
    LARGE_INTEGER   fileSize;
};

