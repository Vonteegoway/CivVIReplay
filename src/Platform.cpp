#include "Platform.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <map>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <Dbghelp.h>
#include <shlobj.h>
#include <tchar.h>
#else
#include <csignal>
#include <cstdio>
#include <cstring>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Platform
{
	static const char* civFolderName = "Sid Meier's Civilization VI";

	static bool isDir(const fs::path& p)
	{
		std::error_code ec;
		return !p.empty() && fs::is_directory(p, ec);
	}

	static bool isFile(const fs::path& p)
	{
		std::error_code ec;
		return !p.empty() && fs::is_regular_file(p, ec);
	}

	static std::string toUpper(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
		return s;
	}

	// Value following `key` on a line of a Valve KeyValues file (libraryfolders.vdf / appmanifest)
	static std::string vdfValue(const std::string& line, const std::string& key)
	{
		size_t keyPos = line.find(key);
		if (keyPos == std::string::npos)
			return "";
		size_t begin = line.find('"', keyPos + key.size());
		if (begin == std::string::npos)
			return "";
		size_t end = line.find('"', begin + 1);
		if (end == std::string::npos)
			return "";
		std::string value = line.substr(begin + 1, end - begin - 1);
		// vdf escapes backslashes on Windows ("C:\\Program Files")
		size_t pos = 0;
		while ((pos = value.find("\\\\", pos)) != std::string::npos)
			value.erase(pos++, 1);
		return value;
	}

#ifdef _WIN32
	fs::path exeDir()
	{
		wchar_t buffer[MAX_PATH];
		DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
		return fs::path(std::wstring(buffer, len)).parent_path();
	}

	fs::path configDir()
	{
		// Historical behaviour: imgui.ini lives next to the working directory
		return fs::current_path();
	}

	std::vector<fs::path> steamRoots()
	{
		std::vector<fs::path> roots;
		HKEY hKey = NULL;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam\\", 0, KEY_READ, &hKey) == 0)
		{
			DWORD type = 0;
			DWORD size = sizeof(wchar_t) * MAX_PATH;
			wchar_t szPath[MAX_PATH] = {};
			if (RegGetValueW(hKey, NULL, L"SteamPath", RRF_RT_ANY, &type, &szPath, &size) == 0 && wcslen(szPath) > 0)
				roots.push_back(fs::path(szPath));
			RegCloseKey(hKey);
		}
		return roots;
	}

	fs::path civUserDir()
	{
		wchar_t my_documents[MAX_PATH];
		if (SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents) != S_OK)
			return fs::path();
		return fs::path(my_documents) / "My Games" / civFolderName;
	}

	fs::path civCacheDbPath()
	{
		const char* appdata = std::getenv("LOCALAPPDATA");
		if (!appdata)
			return fs::path();
		fs::path p = fs::path(appdata) / "Firaxis Games" / civFolderName / "Cache" / "DebugGameplay.sqlite";
		return isFile(p) ? p : fs::path();
	}

	void sleepMs(unsigned int ms)
	{
		Sleep(ms);
	}

	typedef BOOL(WINAPI* MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType, CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

	static void create_minidump(struct _EXCEPTION_POINTERS* apExceptionInfo)
	{
		HMODULE mhLib = ::LoadLibrary(_T("dbghelp.dll"));
		MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(mhLib, "MiniDumpWriteDump");

		HANDLE  hFile = ::CreateFile(_T("core.dmp"), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);

		_MINIDUMP_EXCEPTION_INFORMATION ExInfo;
		ExInfo.ThreadId = ::GetCurrentThreadId();
		ExInfo.ExceptionPointers = apExceptionInfo;
		ExInfo.ClientPointers = FALSE;

		const DWORD Flags = MiniDumpNormal |
			MiniDumpWithFullMemoryInfo |
			MiniDumpWithHandleData |
			MiniDumpWithUnloadedModules |
			MiniDumpWithThreadInfo;

		pDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, (MINIDUMP_TYPE)Flags, &ExInfo, NULL, NULL);
		::CloseHandle(hFile);
	}

	static LONG WINAPI unhandled_handler(struct _EXCEPTION_POINTERS* apExceptionInfo)
	{
		create_minidump(apExceptionInfo);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	void installCrashHandler()
	{
		SetUnhandledExceptionFilter(unhandled_handler);
	}

#else // POSIX / Linux

	static fs::path homeDir()
	{
		const char* home = std::getenv("HOME");
		return home ? fs::path(home) : fs::path("/");
	}

	fs::path exeDir()
	{
		std::error_code ec;
		fs::path exe = fs::read_symlink("/proc/self/exe", ec);
		if (ec)
			return fs::current_path();
		return exe.parent_path();
	}

	fs::path configDir()
	{
		const char* xdg = std::getenv("XDG_CONFIG_HOME");
		fs::path dir = (xdg && *xdg) ? fs::path(xdg) : homeDir() / ".config";
		dir /= "civvireplay";
		std::error_code ec;
		fs::create_directories(dir, ec);
		return dir;
	}

	std::vector<fs::path> steamRoots()
	{
		const fs::path home = homeDir();
		const fs::path candidates[] = {
			home / ".steam" / "steam",
			home / ".local" / "share" / "Steam",
			home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam",
		};
		std::vector<fs::path> roots;
		for (const fs::path& c : candidates)
		{
			if (!isDir(c / "steamapps"))
				continue;
			std::error_code ec;
			fs::path canonical = fs::canonical(c, ec);
			if (ec)
				canonical = c;
			if (std::find(roots.begin(), roots.end(), canonical) == roots.end())
				roots.push_back(canonical);
		}
		return roots;
	}

	// Proton prefix user folder for Civ VI (app 289070), empty if Civ VI never ran under Proton
	static fs::path protonUserDir()
	{
		for (const fs::path& lib : steamLibraries())
		{
			fs::path p = lib / "steamapps" / "compatdata" / "289070" / "pfx" / "drive_c" / "users" / "steamuser";
			if (isDir(p))
				return p;
		}
		return fs::path();
	}

	// Native Linux port (Aspyr) data folder
	static fs::path aspyrDir()
	{
		const char* xdg = std::getenv("XDG_DATA_HOME");
		fs::path base = (xdg && *xdg) ? fs::path(xdg) : homeDir() / ".local" / "share";
		return base / "aspyr-media" / civFolderName;
	}

	fs::path civUserDir()
	{
		fs::path proton = protonUserDir();
		if (!proton.empty())
		{
			fs::path p = proton / "Documents" / "My Games" / civFolderName;
			if (isDir(p))
				return p;
		}
		fs::path aspyr = aspyrDir();
		if (isDir(aspyr))
			return aspyr;
		return homeDir();
	}

	fs::path civCacheDbPath()
	{
		fs::path proton = protonUserDir();
		if (!proton.empty())
		{
			fs::path p = proton / "AppData" / "Local" / "Firaxis Games" / civFolderName / "Cache" / "DebugGameplay.sqlite";
			if (isFile(p))
				return p;
		}
		fs::path p = aspyrDir() / "Cache" / "DebugGameplay.sqlite";
		return isFile(p) ? p : fs::path();
	}

	void sleepMs(unsigned int ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	static char crashLogPath[4096];

	static void crashHandler(int sig)
	{
		void* frames[64];
		int n = backtrace(frames, 64);
		const char* header = "CivVIReplay crashed, backtrace:\n";
		if (write(STDERR_FILENO, header, strlen(header)) < 0) {}
		backtrace_symbols_fd(frames, n, STDERR_FILENO);
		int fd = open(crashLogPath, O_WRONLY | O_CREAT | O_TRUNC, 0644); // async-signal-safe
		if (fd >= 0)
		{
			if (write(fd, header, strlen(header)) < 0) {}
			backtrace_symbols_fd(frames, n, fd);
			close(fd);
		}
		signal(sig, SIG_DFL);
		raise(sig);
	}

	void installCrashHandler()
	{
		snprintf(crashLogPath, sizeof(crashLogPath), "%s", (configDir() / "crash.log").c_str());
		for (int sig : { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS })
			signal(sig, crashHandler);
	}
#endif

	fs::path assetPath(const fs::path& relative)
	{
		return exeDir() / relative;
	}

	std::vector<fs::path> steamLibraries()
	{
		std::vector<fs::path> libraries;
		auto add = [&libraries](const fs::path& p) {
			if (!isDir(p / "steamapps"))
				return;
			std::error_code ec;
			fs::path canonical = fs::canonical(p, ec);
			if (ec)
				canonical = p;
			if (std::find(libraries.begin(), libraries.end(), canonical) == libraries.end())
				libraries.push_back(canonical);
		};

		for (const fs::path& root : steamRoots())
		{
			add(root);
			std::ifstream file(root / "steamapps" / "libraryfolders.vdf");
			std::string line;
			while (std::getline(file, line))
			{
				std::string path = vdfValue(line, "\"path\"");
				if (!path.empty())
					add(fs::path(path));
			}
		}
		return libraries;
	}

	std::string findImage(const std::string& folder, const std::string& name)
	{
		// Built once: UPPER("folder/stem") -> real path. Image names come from database
		// type names (upper case) while files on disk are mixed case.
		static const std::map<std::string, std::string> index = []() {
			std::map<std::string, std::string> result;
			const fs::path root = assetPath("image");
			std::error_code ec;
			for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
			{
				const fs::path& p = it->path();
				if (toUpper(p.extension().string()) != ".PNG")
					continue;
				fs::path rel = p.lexically_relative(root);
				rel.replace_extension();
				result[toUpper(rel.generic_string())] = p.string();
			}
			return result;
		}();

		std::string key = toUpper(folder + "/" + name);
		size_t pos;
		while ((pos = key.find("//")) != std::string::npos)
			key.erase(pos, 1);
		auto found = index.find(key);
		return found == index.end() ? std::string() : found->second;
	}
}
