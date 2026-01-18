#ifndef SCAN_UNIMPLEMENTED_UTILS_H
#define SCAN_UNIMPLEMENTED_UTILS_H

#include "config.h"
#include "dfile.h"
#include "dictionary.h"
#include "interpreter.h"
#include "platform_compat.h"
#include "scan_unimplemented.h"
#include "scan_unimplemented_sfall.h"
#include "sfall_metarules.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

#if defined(__linux__) and not defined(__ANDROID__)
#include <filesystem>
#elif defined(_WIN32)
#include <sys/stat.h>
#include <windows.h>
#else
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#endif

std::string to_lower(const std::string& str)
{
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr;
};

bool ends_with(const std::string& str, const std::string& suffix)
{
    auto low = to_lower(str);
    auto low_suffix = to_lower(suffix);
    if (str.size() < suffix.size()) {
        return false;
    }
    return std::equal(low_suffix.rbegin(), low_suffix.rend(), low.rbegin());
}

std::string toHexString(unsigned int value, bool prefix = true, bool uppercase = false)
{
    char buffer[20]; // Enough for "0x" + 8 hex digits + null terminator

    if (prefix) {
        if (uppercase)
            std::snprintf(buffer, sizeof(buffer), "0X%X", value);
        else
            std::snprintf(buffer, sizeof(buffer), "0x%x", value);
    } else {
        if (uppercase)
            std::snprintf(buffer, sizeof(buffer), "%X", value);
        else
            std::snprintf(buffer, sizeof(buffer), "%x", value);
    }

    return std::string(buffer);
}

#if defined(__linux__) and not defined(__ANDROID__)
#define directory_iterator std::filesystem::directory_iterator
#else

// This is a simple directory iterator that mimics the behavior of C++17's std::filesystem::directory_iterator
class directory_iterator {
public:
    class DirEntry {
    public:
        explicit DirEntry(const std::string& fullPath)
            : _path(fullPath)
            , _isDir(false)
            , _isFile(false)
        {
#ifdef _WIN32
            struct _stat s;
            if (_stat(fullPath.c_str(), &s) == 0) {
                _isDir = (s.st_mode & _S_IFDIR);
                _isFile = (s.st_mode & _S_IFREG);
            }
#else
            struct stat s;
            if (stat(fullPath.c_str(), &s) == 0) {
                _isDir = S_ISDIR(s.st_mode);
                _isFile = S_ISREG(s.st_mode);
            }
#endif
        }

        bool is_directory() const { return _isDir; }
        bool is_regular_file() const { return _isFile; }
        const std::string& path() const { return _path; }

    private:
        std::string _path;
        bool _isDir;
        bool _isFile;
    };

    class Iterator {
    public:
        Iterator() = default;

        explicit Iterator(const std::string& path)
            : _root(path)
        {
#ifdef _WIN32
            std::string searchPath = path + "\\*";
            _hFind = FindFirstFileA(searchPath.c_str(), &_findData);
            _hasMore = (_hFind != INVALID_HANDLE_VALUE);
#else
            _dir = opendir(path.c_str());
#endif
            ++(*this); // load first valid
        }

        ~Iterator()
        {
#ifdef _WIN32
            if (_hFind != INVALID_HANDLE_VALUE) {
                FindClose(_hFind);
            }
#else
            if (_dir) {
                closedir(_dir);
            }
#endif
        }

        DirEntry operator*() const
        {
            return DirEntry(_root + "/" + _entryName);
        }

        Iterator& operator++()
        {
#ifdef _WIN32
            while (_hasMore) {
                std::string name = _findData.cFileName;
                if (name != "." && name != "..") {
                    _entryName = name;
                    _hasMore = FindNextFileA(_hFind, &_findData);
                    return *this;
                }
                _hasMore = FindNextFileA(_hFind, &_findData);
            }
            _hFind = INVALID_HANDLE_VALUE;
#else
            if (!_dir) return *this;
            struct dirent* entry;
            while ((entry = readdir(_dir))) {
                std::string name = entry->d_name;
                if (name != "." && name != "..") {
                    _entryName = name;
                    return *this;
                }
            }
            closedir(_dir);
            _dir = nullptr;
#endif
            _entryName.clear();
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
#ifdef _WIN32
            return _hFind != other._hFind;
#else
            return _dir != other._dir;
#endif
        }

    private:
        std::string _root;
        std::string _entryName;

#ifdef _WIN32
        HANDLE _hFind = INVALID_HANDLE_VALUE;
        WIN32_FIND_DATAA _findData;
        bool _hasMore = false;
#else
        DIR* _dir = nullptr;
#endif
    };

    explicit directory_iterator(const std::string& path)
        : _path(path)
    {
    }
    Iterator begin() const { return Iterator(_path); }
    Iterator end() const { return Iterator(); }

private:
    std::string _path;
};

#endif

#endif