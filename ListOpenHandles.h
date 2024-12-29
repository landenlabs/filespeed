//-----------------------------------------------------------------------------
// ListOpenHandles
// Author:  DLang   2009  
//-----------------------------------------------------------------------------

#pragma once

#include <iostream>
#include <string>
 
class ListOpenHandles
{
public:
    ListOpenHandles() : 
        m_waitMin(0),
        m_repeat(0),
        m_showModules(false), 
        m_showAll(false), 
        m_showSummary(false),
        m_showFiles(true),
        m_showEvents(false),
        m_showHex(false),
        m_showProcInfo(true),
        m_processId(0)
    {}

    int ReportOpenFiles(std::ostream& out);
    int ReportOpenFiles(std::ostream& out, bool showHeaders, const char* prefix);

    double      m_waitMin;          // -w <minutes>
    size_t      m_repeat;           // -r <num>
    std::string m_process;          // -p <process>
    unsigned    m_processId;        // -p <process>
    std::string m_filename;         // -i <filename>

    bool        m_showModules;      // -y m
    bool        m_showAll;          // -y a
    bool        m_showSummary;      // -y s
    bool        m_showFiles;        // -y f
    bool        m_showEvents;       // -y e
    bool        m_showHex;          // -y x
    bool        m_showProcInfo;     // -y p

    std::string m_logFilename;
};

