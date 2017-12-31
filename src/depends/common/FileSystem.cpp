/*
        This file is part of cpp-ethereum.
        cpp-ethereum is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.
        cpp-ethereum is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
        You should have received a copy of the GNU General Public License
        along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file FileSystem.cpp
 * @authors
 *	 Eric Lombrozo <elombrozo@gmail.com>
 *	 Gav Wood <i@gavwood.com>
 * @date 2014
 */

#if defined(_WIN32)
#include <shlobj.h>
#else
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#endif

#include "Common.h"
#include "FileSystem.h"

#include <boost/filesystem.hpp>

using namespace std;
using namespace dev;

namespace fs = boost::filesystem;

// Should be written to only once during startup
static fs::path s_zilliqaDatadir;
static fs::path s_zilliqaIpcPath;

void dev::setDataDir(fs::path const& _dataDir)
{
    s_zilliqaDatadir = _dataDir;
}

void dev::setIpcPath(fs::path const& _ipcDir)
{
    s_zilliqaIpcPath = _ipcDir;
}

fs::path dev::getIpcPath()
{
    // Strip "geth.ipc" suffix if provided.
    if (s_zilliqaIpcPath.filename() == "geth.ipc")
        return s_zilliqaIpcPath.parent_path();
    else
        return s_zilliqaIpcPath;
}

fs::path dev::getDataDir(string _prefix)
{
    if (_prefix.empty())
        _prefix = "zilliqa";
    if (_prefix == "zilliqa" && !s_zilliqaDatadir.empty())
        return s_zilliqaDatadir;
    return getDefaultDataDir(_prefix);
}

fs::path dev::getDefaultDataDir(string _prefix)
{
    if (_prefix.empty())
        _prefix = "zilliqa";

#if defined(_WIN32)
    _prefix[0] = toupper(_prefix[0]);
	char path[1024] = "";
	if (SHGetSpecialFolderPathA(NULL, path, CSIDL_APPDATA, true))
		return fs::path(path) / _prefix;
	else
	{
		BOOST_THROW_EXCEPTION(std::runtime_error("getDataDir() - SHGetSpecialFolderPathA() failed."));
	}
#else
    fs::path dataDirPath;
    char const* homeDir = getenv("HOME");
    if (!homeDir || strlen(homeDir) == 0)
    {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
            homeDir = pwd->pw_dir;
    }

    if (!homeDir || strlen(homeDir) == 0)
        dataDirPath = fs::path("/");
    else
        dataDirPath = fs::path(homeDir);

    return dataDirPath / ("." + _prefix);
#endif
}

fs::path dev::appendToFilename(fs::path const& _orig, string const& _suffix)
{
    if (_orig.filename() == fs::path(".") || _orig.filename() == fs::path(".."))
        return _orig / fs::path(_suffix);
    else
        return _orig.parent_path() / fs::path( _orig.filename().string() + _suffix);
}