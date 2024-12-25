//-----------------------------------------------------------------------------
// Class to scan directories with pattern matching at every directory level.
// Author:  DLang   2008
//-----------------------------------------------------------------------------

#pragma once

#include <windows.h>
#include <vector>
#include <ctype.h>
// #include "wsi_stdhdr.h"

// Compare a pattern to a string.
// Patterns supported:
//          ?        ; any single character
//          *        ; zero or more characters
bool PatternMatch(const char* pattern, const char* str);

struct DirectoryScan
{
public:
    DirectoryScan() : 
        m_fileFilter(0), m_pFiles(0), 
        m_recurse(false), m_abort(false),
        m_add_cb(0), m_cb_data(0), m_tmpPtr(0), 
        m_filesFirst(false), m_entryType(eFilesDir)
        { m_dir[0] = '\0'; }

    ~DirectoryScan() { free(m_tmpPtr); }

    // Initialize base directory, appendStar adds "\*" to end if FromFiles is a directory
    void Init(const char* pFromFiles, const char* pCwd, bool appendStar = true);
    size_t GetFilesInDirectory(int depth=0);
    size_t GetFilesInDirectory2(int depth=0);

    // If m_fileFirst true, then process only files in first pass, directories only
    // in 2nd pass, else both in one pass.
    bool        m_filesFirst;
    enum  EntryType { eFilesDir, eFile, eDir };
    EntryType   m_entryType;


    char        m_dir[MAX_PATH];
    const char* m_fileFilter;
    std::vector<const char*> m_dirFilters;
    bool        m_recurse;
    bool        m_abort;    
	std::vector<std::string>* m_pFiles;  // optional list to populate with files

    typedef void (*Add_cb)(void *, const char* pDir, WIN32_FIND_DATA * pFileData, int depth);
    Add_cb      m_add_cb;
    void *      m_cb_data;

    // Number of slashes in source pattern.
    int         m_subDirCnt;

    // Memory allocated and needs to be freed.
    void *      m_tmpPtr;

    // Text comparison functions.
    static bool YCaseChrCmp(char c1, char c2) { return c1 == c2; }
    static bool NCaseChrCmp(char c1, char c2) { return tolower(c1) == tolower(c2); }
    static bool (*ChrCmp)(char c1, char c2);
};