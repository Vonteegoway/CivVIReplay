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
#include <cstdint>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
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

#else // POSIX: Linux and macOS

	static fs::path homeDir()
	{
		const char* home = std::getenv("HOME");
		return home ? fs::path(home) : fs::path("/");
	}

	// Where Civ VI keeps its per-user data: the Windows layout is used by the Windows build
	// under Wine (Proton, CrossOver, Whisky...), the native one by Aspyr's Linux / macOS ports.
	struct CivLocation
	{
		fs::path userDir; // ".../Sid Meier's Civilization VI" (Saves, Mods...)
		fs::path cacheDb; // DebugGameplay.sqlite
	};

	static CivLocation windowsLayout(const fs::path& windowsUserDir)
	{
		return {
			windowsUserDir / "Documents" / "My Games" / civFolderName,
			windowsUserDir / "AppData" / "Local" / "Firaxis Games" / civFolderName / "Cache" / "DebugGameplay.sqlite",
		};
	}

	static CivLocation nativeLayout(const fs::path& civDir)
	{
		return { civDir, civDir / "Cache" / "DebugGameplay.sqlite" };
	}

#ifdef __APPLE__
	fs::path exeDir()
	{
		uint32_t size = 0;
		_NSGetExecutablePath(nullptr, &size);
		std::string buffer(size, '\0');
		if (size == 0 || _NSGetExecutablePath(buffer.data(), &size) != 0)
			return fs::current_path();
		buffer.resize(std::strlen(buffer.c_str()));
		std::error_code ec;
		fs::path exe = fs::canonical(fs::path(buffer), ec);
		if (ec)
			exe = fs::path(buffer);
		return exe.parent_path();
	}

	fs::path configDir()
	{
		fs::path dir = homeDir() / "Library" / "Application Support" / "CivVIReplay";
		std::error_code ec;
		fs::create_directories(dir, ec);
		return dir;
	}

	std::vector<fs::path> steamRoots()
	{
		std::vector<fs::path> roots;
		fs::path steam = homeDir() / "Library" / "Application Support" / "Steam";
		if (isDir(steam / "steamapps"))
			roots.push_back(steam);
		return roots;
	}

	// Candidate Civ VI data locations, most likely first:
	// the Steam (Aspyr) Mac build, then the Windows build inside CrossOver or Whisky bottles.
	static std::vector<CivLocation> civLocations()
	{
		std::vector<CivLocation> locations;
		const fs::path appSupport = homeDir() / "Library" / "Application Support";
		// Since Aspyr's 2023 update the Steam build keeps saves/mods one level deeper and the
		// gameplay cache in a Windows-like "Firaxis Games" folder next to them (confirmed on
		// macOS 26). The pre-2023 layout, with Saves and Cache directly inside, is the fallback;
		// its leftover Cache is stale on updated installs.
		const fs::path aspyr = appSupport / civFolderName;
		locations.push_back({
			aspyr / civFolderName,
			aspyr / "Firaxis Games" / civFolderName / "Cache" / "DebugGameplay.sqlite",
		});
		locations.push_back(nativeLayout(aspyr));

		const fs::path bottleRoots[] = {
			appSupport / "CrossOver" / "Bottles",
			homeDir() / "Library" / "Containers" / "com.isaacmarovitz.Whisky" / "Bottles",
		};
		for (const fs::path& root : bottleRoots)
		{
			std::error_code ec;
			for (fs::directory_iterator bottle(root, ec), end; !ec && bottle != end; bottle.increment(ec))
			{
				fs::path users = bottle->path() / "drive_c" / "users";
				std::error_code ec2;
				for (fs::directory_iterator user(users, ec2), end2; !ec2 && user != end2; user.increment(ec2))
				{
					const std::string name = user->path().filename().string();
					if (name == "Public" || name == "Default")
						continue;
					locations.push_back(windowsLayout(user->path()));
				}
			}
		}
		return locations;
	}
#else
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

	// Candidate Civ VI data locations, most likely first:
	// the Proton prefix of app 289070, then the native (Aspyr) Linux port.
	static std::vector<CivLocation> civLocations()
	{
		std::vector<CivLocation> locations;
		for (const fs::path& lib : steamLibraries())
		{
			fs::path user = lib / "steamapps" / "compatdata" / "289070" / "pfx" / "drive_c" / "users" / "steamuser";
			if (isDir(user))
				locations.push_back(windowsLayout(user));
		}
		const char* xdg = std::getenv("XDG_DATA_HOME");
		fs::path base = (xdg && *xdg) ? fs::path(xdg) : homeDir() / ".local" / "share";
		locations.push_back(nativeLayout(base / "aspyr-media" / civFolderName / civFolderName));
		locations.push_back(nativeLayout(base / "aspyr-media" / civFolderName));
		return locations;
	}
#endif

	// A location counts as "in use" when it has a Saves folder: on macOS the pre-2023 folder is the
	// parent of the current one, and a leftover empty folder must not win over a real install.
	static bool hasSaves(const CivLocation& loc)
	{
		return isDir(loc.userDir / "Saves");
	}

	fs::path civUserDir()
	{
		const std::vector<CivLocation> locations = civLocations();
		for (const CivLocation& loc : locations)
			if (hasSaves(loc))
				return loc.userDir;
		for (const CivLocation& loc : locations)
			if (isDir(loc.userDir))
				return loc.userDir;
		return homeDir();
	}

	fs::path civCacheDbPath()
	{
		const std::vector<CivLocation> locations = civLocations();
		// Prefer the database of the install whose saves we use, so an updated install never
		// picks up a stale database left behind by an older layout...
		for (const CivLocation& loc : locations)
			if (hasSaves(loc) && isFile(loc.cacheDb))
				return loc.cacheDb;
		// ...otherwise any database at all (CivVIDatabase rejects unusable ones)
		for (const CivLocation& loc : locations)
			if (isFile(loc.cacheDb))
				return loc.cacheDb;
		return fs::path();
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
		static const fs::path root = []() {
			fs::path dir = exeDir();
#ifdef __APPLE__
			// Inside an app bundle the assets are in Contents/Resources, next to Contents/MacOS
			if (dir.filename() == "MacOS" && dir.parent_path().filename() == "Contents")
				return dir.parent_path() / "Resources";
#endif
			return dir;
		}();
		return root / relative;
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
