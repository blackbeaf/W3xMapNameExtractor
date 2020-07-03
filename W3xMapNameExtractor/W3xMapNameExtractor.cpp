// W3xMapNameExtractor.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <unordered_map>
#include <Windows.h>
#include <time.h>
#include <tchar.h>

using namespace std;
typedef basic_string<TCHAR> tstring;
typedef unordered_map<tstring, tstring> HashTstring2Tstring;
typedef unordered_map<tstring, vector<tstring>> HashTstring2VectorTstring;

/************************************************************************/
/*utility function                                                      */
/************************************************************************/

#define LEN_1K 1024

template<int SIZE=LEN_1K>
inline tstring format(const TCHAR* fmt, ...)
{
    TCHAR str[SIZE] = { 0 };
    va_list arg;
    va_start(arg, fmt);
    _vsntprintf_s(str, SIZE, fmt, arg);
    va_end(arg);
    return tstring(str);
}

inline tstring GetYMDHMS(time_t ts = time(NULL))
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    return format<LEN_1K>(_T("%u-%02u-%02u %02u:%02u:%02u"),
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
}

#define log(fmt, ...) _tprintf(_T("[%s][%s:%d] ") fmt _T("\n"), GetYMDHMS().c_str(),_T(__FUNCTION__),__LINE__,__VA_ARGS__)

tstring GetCurrentDirectory()
{
    static const int len = 100;
    TCHAR dir[len] = { 0 };
    if (GetCurrentDirectory(len, dir) < 1)
    {
        log(_T("get current directory fail"));
        return tstring();
    }
    return tstring(dir);
}

bool WithSuffix(const TCHAR *file, const TCHAR* suffix)
{
    if (NULL==file||NULL==suffix)
    {
        return false;
    }

    size_t len = _tcslen(suffix);
    file += _tcslen(file)-len;
    return 0==_tcsicmp(file, suffix);
}

void ExtractMapName(const TCHAR* file, HashTstring2Tstring &file2map, HashTstring2VectorTstring &map2file)
{
    HANDLE hfile = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE ==hfile)
    {
        log("can't open %s", file);
        return;
    }

    TCHAR mapName[LEN_1K] = { 0 };
    DWORD left = LEN_1K;
    DWORD numberOfReaed=0;
    if (!ReadFile(hfile, mapName, left, &numberOfReaed, NULL))
    {
        log("read %s fail.", file);
        CloseHandle(hfile);
        return;
    }

    CloseHandle(hfile);

    TCHAR utf8[LEN_1K] = { 0 };
    MultiByteToWideChar(CP_UTF8, 0, ((const char*)mapName)+8, -1, utf8, LEN_1K);

    tstring newname(utf8);
    newname.erase(remove_if(newname.begin(), newname.end(),
        [](TCHAR t) {return _tcschr(_T("\\/:*?\"<>|\r\n"), t) != NULL; }), newname.end());
    file2map[file] = newname;
    map2file[newname].push_back(file);
}

void NormalColor()
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED |
        FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void RedColor()
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED);
}

int main()
{
    setlocale(LC_ALL, "chs");
    tstring dir = GetCurrentDirectory();
    if (dir.size()<1)
    {
        log(_T("get current directory fail"));
        return -1;
    }

    log(_T("working directory is %s"), GetCurrentDirectory().c_str());

    //enum all *.w3x file
    HANDLE hfind;
    WIN32_FIND_DATA wfd;
    dir += _T("\\*");
    hfind = FindFirstFile(dir.c_str(), &wfd);
    if (INVALID_HANDLE_VALUE == hfind)
    {
        log(_T("find first file failed"));
        return -1;
    }

    HashTstring2Tstring result;
    HashTstring2VectorTstring duplicatemap;

    do 
    {
        if (wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }

        if (!WithSuffix(wfd.cFileName,_T("w3x")))
        {
            continue;
        }

        ExtractMapName(wfd.cFileName, result, duplicatemap);
    } while (FindNextFile(hfind, &wfd));
    FindClose(hfind);

    if (result.empty())
    {
        RedColor();
        log("no w3x files found");
        NormalColor();
        return -1;
    }

    unsigned countOk = 0,countFail=0;
    for (auto i=result.begin();i!=result.end();++i)
    {
        //put the suffix -0, -1, -2, ... to the duplicate map files.
        tstring finalname = i->second;
        unsigned index = 0;
        const vector<tstring>& files = duplicatemap[finalname];
        if (files.size()>1)
        {
            for (;index<files.size();++index)
            {
                if (i->first==files[index])
                {
                    break;
                }
            }

            finalname += _T("-");
            finalname += format(_T("%u"), index);
        }

        finalname += _T(".w3x");
        if (MoveFile(i->first.c_str(), finalname.c_str()))
        {
            log(" [%u] rename file %-40s to %s ok", ++countOk, i->first.c_str(), finalname.c_str());
        }
        else
        {
            RedColor();
            log(" [%u] rename file %-40s to %s fail", ++countFail, i->first.c_str(), finalname.c_str());
            NormalColor();
        }
    }

    log("Extraction completed. %u succeed, %u failed", countOk, countFail);
}
