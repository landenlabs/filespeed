//-----------------------------------------------------------------------------
// Scan directories.
// Author:  DLang   2008   
//-----------------------------------------------------------------------------

#include <iostream>
#include "dirscan.h"

using namespace std;

// Initialize static members
//
bool (*DirectoryScan::ChrCmp)(char c1, char c2) = DirectoryScan::NCaseChrCmp;
static char sDirChr = '\\';

//-----------------------------------------------------------------------------
// Simple wildcard matcher, used by directory file scanner.
// Patterns supported:
//          ?        ; any single character
//          *        ; zero or more characters

static bool WildCompare(const char* wildStr, int wildOff, const char* rawStr, int rawOff)
{
    const char EOS = '\0';
    while (wildStr[wildOff])
    {
        if (rawStr[rawOff] == EOS)
            return (wildStr[wildOff] == '*');

        if (wildStr[wildOff] == '*')
        {
            if (wildStr[wildOff + 1] == EOS)
                return true;

            do
            {
                // Find match with char after '*'
                while (rawStr[rawOff] && 
                    !DirectoryScan::ChrCmp(rawStr[rawOff], wildStr[wildOff+1]))
                    rawOff++;
                if (rawStr[rawOff] &&
                    WildCompare(wildStr, wildOff + 1, rawStr, rawOff))
                        return true;
                if (rawStr[rawOff])
                    ++rawOff;
            } while (rawStr[rawOff]);

            if (rawStr[rawOff] == EOS)
                return (wildStr[wildOff+1] == '*');
        }
        else if (wildStr[wildOff] == '?')
        {
            if (rawStr[rawOff] == EOS)
                return false;
            rawOff++;
        }
        else
        {
            if (!DirectoryScan::ChrCmp(rawStr[rawOff], wildStr[wildOff]))
                return false;
            if (wildStr[wildOff] == EOS)
                return true;
            ++rawOff;
        }

        ++wildOff;
    }

    return (wildStr[wildOff] == rawStr[rawOff]);
}

//-----------------------------------------------------------------------------
bool PatternMatch(const char* pattern, const char* str)
{
    return WildCompare(pattern, 0, str, 0);
}

//-----------------------------------------------------------------------------
size_t DirectoryScan::GetFilesInDirectory(int depth)
{
    if (m_filesFirst)
    {
        bool recurse = m_recurse;

        m_entryType = eFilesDir;
        m_recurse   = false;
        size_t fileSz = GetFilesInDirectory2(depth);
        size_t dirSz = 0;

        if (recurse)
        {
            m_entryType = eDir;
            m_recurse   = recurse;
            dirSz = GetFilesInDirectory2(depth);
        }
        return fileSz + dirSz;
    }

    m_entryType = eFilesDir;
    return GetFilesInDirectory2(depth);
}

//-----------------------------------------------------------------------------
size_t DirectoryScan::GetFilesInDirectory2(int depth)
{
    WIN32_FIND_DATA FileData;    // Data structure describes the file found
    HANDLE  hSearch;             // Search handle returned by FindFirstFile
    bool    is_more = true;
    size_t  fileCnt = 0;
    size_t  dirLen = strlen(m_dir);
	size_t	matchCnt = 0;

    strcat_s(m_dir, sizeof(m_dir), "\\*");

    // Start searching for folder (directories), starting with srcdir directory.

    hSearch = FindFirstFile(m_dir, &FileData);
    if (hSearch == INVALID_HANDLE_VALUE)
    {
        // No files found
        if (depth == 0)
        {
            // PresentError(GetLastError(), "Failed to open directory, ", m_dir); 
        }
        return 0;
    }

    m_dir[dirLen] = '\0';
    while (is_more && !m_abort)
    {
        if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (m_entryType != eFile &&
               (FileData.cFileName[0] != '.' ||  isalnum(FileData.cFileName[1])))
            {
                if (m_recurse || depth < (int)m_dirFilters.size())
                {
                    int newDepth = depth + 1;
                    if ((int)m_dirFilters.size() < newDepth || PatternMatch(m_dirFilters[depth], FileData.cFileName))
                    {
                        if (m_entryType == eFilesDir &&
                            m_add_cb && depth >= (int)m_dirFilters.size())
                            m_add_cb(m_cb_data, m_dir, &FileData, depth);

                        m_dir[dirLen] = sDirChr;
                        strcpy_s(m_dir + dirLen +1, sizeof(m_dir) - dirLen - 1, FileData.cFileName);
                        fileCnt += GetFilesInDirectory(depth+1);
                        m_dir[dirLen] = '\0';
                    }
                }
                else if (m_fileFilter != NULL &&
                    PatternMatch(m_fileFilter, FileData.cFileName))
                {
                    if (m_add_cb)
                        m_add_cb(m_cb_data, m_dir, &FileData, depth);
                }
            }
        }
        else
        {
            if (m_entryType != eDir)
            {
                ++fileCnt;
                if (m_fileFilter == NULL ||
                    PatternMatch(m_fileFilter, FileData.cFileName))
                {
                    if (m_add_cb && depth >= (int)m_dirFilters.size())
                        m_add_cb(m_cb_data, m_dir, &FileData, depth);

                    if (m_pFiles)
                    {
			            static std::string sSlash("\\");
					    std::string fullFile(m_dir);
                        fullFile += sSlash;
                        fullFile += FileData.cFileName;
                        m_pFiles->push_back(fullFile);    
                    }
                }
            }
        }

        is_more = (FindNextFile(hSearch, &FileData) != 0) ? true : false;
    }

    // Close directory before calling callback incase client wants to delete dir.
    FindClose(hSearch);

    DWORD err = GetLastError();
    if (err != ERROR_NO_MORE_FILES)
    {
        cout << "Error " << err << "\n";
        exit(-1);
    }

    if (m_entryType != eFile)
    {
        // Set attribute for end-of-directory
        FileData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        FileData.cFileName[0] = '\0';
        int dirDepth = -1 -depth;
    	if (m_add_cb && (m_dirFilters.size() == 0 || depth >= (int)m_dirFilters.size()))
            m_add_cb(m_cb_data, m_dir, &FileData, dirDepth); 
    }

    return fileCnt;
}

// ---------------------------------------------------------------------------
void DirectoryScan::Init(const char* pFromFiles, const char* pCwd, bool appendStar)
{
    // Free previous temporary buffer.
    free(m_tmpPtr);
    m_tmpPtr    = 0;
    m_subDirCnt = 0;

    const char* tmpDir = pCwd;   
    m_fileFilter  = "*";
    m_abort       = false;
    m_pFiles      = NULL; 

    // Make local copy of input pattern
    int strLen = strlen(pFromFiles) + 3;
    char* pDupFromFiles = (char*)malloc(strLen);
    strcpy_s(pDupFromFiles, strLen, pFromFiles);
    m_tmpPtr      = pDupFromFiles;

    if (appendStar)
    {
        // If input is a directory, append slash
        // If input does not exist and does not contain wildcards, append slash
        DWORD attr = GetFileAttributes(pFromFiles);
        if ((attr == INVALID_FILE_ATTRIBUTES && pFromFiles[strcspn(pFromFiles, "*?")] == '\0')
            || 
            (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0))
        {
            strcat_s(pDupFromFiles, strLen, "\\");
        }
    }

    // Pull input pattern apart at directory levels.
    int posWild   = strcspn(pDupFromFiles, "*?");
    char* pBegDir = pDupFromFiles;
    char* pEndDir;

    while (pEndDir = strchr(pBegDir, sDirChr))
    {
        m_subDirCnt++;

        if (pEndDir - pDupFromFiles > posWild)
        {
            // Have wildcards in dirPath
            if (pBegDir != pDupFromFiles)
                pBegDir[-1] = '\0';
            m_dirFilters.push_back(pBegDir);
        }
        else 
            tmpDir = pDupFromFiles;
        pBegDir = pEndDir + 1;
    }

    if (pBegDir != pDupFromFiles)
        pBegDir[-1] = '\0';
    if (*pBegDir)
        m_fileFilter = pBegDir;

    strcpy_s(m_dir, sizeof(m_dir), tmpDir);
}

