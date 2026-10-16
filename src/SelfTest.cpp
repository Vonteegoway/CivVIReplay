#include "SelfTest.h"
#include "Platform.h"
#include "SaveLoader.h"
#include "Viewer.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif !defined(_WIN32)
#include <sys/utsname.h>
#endif

#ifndef CIVREPLAY_GIT_COMMIT
#define CIVREPLAY_GIT_COMMIT "unknown"
#endif

namespace fs = std::filesystem;

namespace
{
	int failures = 0;
	std::ofstream logFile; // copy of the output, next to the screenshot

	void report(const char* tag, const std::string& message)
	{
		printf("[%s] %s\n", tag, message.c_str());
		fflush(stdout);
		if (logFile.is_open())
			logFile << '[' << tag << "] " << message << '\n' << std::flush;
	}
	void ok(const std::string& m) { report(" ok ", m); }
	void info(const std::string& m) { report("info", m); }
	void warn(const std::string& m) { report("warn", m); }
	void fail(const std::string& m) { failures++; report("FAIL", m); }

	std::string str(const fs::path& p) { return p.u8string(); }

	bool isDir(const fs::path& p)
	{
		std::error_code ec;
		return !p.empty() && fs::is_directory(p, ec);
	}

	bool isFile(const fs::path& p)
	{
		std::error_code ec;
		return !p.empty() && fs::is_regular_file(p, ec);
	}

	int countRows(void* data, int, char**, char**)
	{
		++*static_cast<int*>(data);
		return 0;
	}

	std::string systemDescription()
	{
		std::string s;
#if defined(_WIN32)
		s = "Windows";
#elif defined(__APPLE__)
		s = "macOS";
		char version[64];
		size_t size = sizeof(version);
		if (sysctlbyname("kern.osproductversion", version, &size, nullptr, 0) == 0)
			s += std::string(" ") + version;
#else
		utsname u;
		s = uname(&u) == 0 ? std::string(u.sysname) + " " + u.release : "Linux";
#endif
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
		s += ", arm64";
#elif defined(__x86_64__) || defined(_M_X64)
		s += ", x86_64";
#endif
		return s;
	}

	// Newest saves from the user's Civ VI folder, for a run without arguments
	std::vector<std::string> recentSaves(size_t count)
	{
		std::vector<std::pair<fs::file_time_type, fs::path>> found;
		const fs::path saves = Platform::civUserDir() / "Saves";
		for (const char* sub : { "Single", "Multi", "Hotseat", "PlayByCloud" })
		{
			std::error_code ec;
			for (fs::directory_iterator it(saves / sub, ec), end; !ec && it != end; it.increment(ec))
			{
				std::string ext = it->path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
				if (ext != ".civ6save")
					continue;
				std::error_code ec2;
				fs::file_time_type t = fs::last_write_time(it->path(), ec2);
				if (!ec2)
					found.emplace_back(t, it->path());
			}
		}
		std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
		std::vector<std::string> result;
		for (size_t i = 0; i < found.size() && i < count; i++)
			result.push_back(found[i].second.string());
		return result;
	}
}

int runSelfTest(int argc, char** argv)
{
	Platform::installCrashHandler();

	bool headless = false;
	std::vector<std::string> saves;
	for (int i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "--headless"))
			headless = true;
		else if (!strncmp(argv[i], "--", 2))
		{
			fprintf(stderr, "unknown option %s\nusage: CivVIReplay --selftest [--headless] [save.Civ6Save ...]\n", argv[i]);
			return 2;
		}
		else
			saves.push_back(argv[i]);
	}

	const fs::path configDir = Platform::configDir();
	logFile.open(configDir / "selftest.txt", std::ios::trunc);

	info(std::string("CivVIReplay self-test, build ") + CIVREPLAY_GIT_COMMIT + " (" + __DATE__ + "), " + systemDescription());

	// Assets next to the executable (or in the app bundle)
	info("executable: " + str(Platform::exeDir()));
	const fs::path font = Platform::assetPath("font/ProggyTiny.ttf");
	if (isFile(font))
		ok("font: " + str(font));
	else
		fail("font not found: " + str(font));
	if (!Platform::findImage("icon", "icon1").empty())
		ok("images: " + str(Platform::assetPath("image")));
	else
		fail("images not found under " + str(Platform::assetPath("image")));
	const fs::path backupDb = Platform::assetPath("image") / "database" / "backup.sqlite";
	if (isFile(backupDb))
		ok("bundled database: " + str(backupDb));
	else
		fail("bundled database not found: " + str(backupDb));

	if (logFile.is_open())
		ok("config dir: " + str(configDir) + " (output copied to selftest.txt)");
	else
		warn("config dir not writable: " + str(configDir) + " (settings will not be saved)");

	// Steam / Civ VI
	const std::vector<fs::path> roots = Platform::steamRoots();
	if (roots.empty())
		warn("Steam not found");
	for (const fs::path& r : roots)
		info("steam root: " + str(r));
	for (const fs::path& lib : Platform::steamLibraries())
		info("steam library: " + str(lib));

	loader.waitForCivPaths = false;
	loader.init();
	if (loader.isValidCivPath())
		ok("Civ VI install: " + str(loader.civPath));
	else
		warn("Civ VI install not found: the app will ask for the Civ VI folder at startup (saves can still be parsed here)");
	if (loader.isValidBBGPath())
		ok(loader.steamInstall ? "BBG mod: Steam Workshop" : "BBG mod folder: " + str(loader.bbgPath));
	else
		warn("BBG mod folder not found: the app will ask for it at startup (" + str(loader.bbgPath) + ")");

	const fs::path userDir = Platform::civUserDir();
	if (isDir(userDir / "Saves"))
		ok("Civ VI saves: " + str(userDir / "Saves"));
	else
		warn("Civ VI user folder not found (the Open dialog will start in " + str(userDir) + ")");

	const fs::path cacheDb = Platform::civCacheDbPath();
	if (cacheDb.empty())
		warn("gameplay database not found, the bundled copy will be used (mod-added civs/units will be missing)");
	else
		info("gameplay database: " + str(cacheDb));

	// Database (LoadSave(true) with an empty queue loads it and returns)
	loader.LoadSave(true);
	info("database in use: " + str(loader.assets.database.path));
	int leaders = 0;
	loader.assets.database.GetTable("Leaders", countRows, &leaders);
	if (leaders > 0)
		ok("database: " + std::to_string(leaders) + " leaders");
	else
		fail("database: no leaders found");
	{
		size_t resolvable = 0, images = 0;
		for (unsigned int id : loader.assets.imageAssets)
		{
			const Asset& a = loader.assets.assets[id];
			if (!a.bImage)
				continue;
			images++;
			if (!Platform::findImage(a.folderName, a.imageName).empty())
				resolvable++;
		}
		const std::string line = std::to_string(resolvable) + " / " + std::to_string(images) + " image files found for " + std::to_string(loader.assets.assets.size()) + " database types";
		if (images > 0 && resolvable == images)
			ok(line);
		else
			warn(line);
	}

	// Saves: given on the command line, otherwise the newest from the user's folder
	bool autoSaves = false;
	if (saves.empty())
	{
		saves = recentSaves(5);
		autoSaves = true;
		if (saves.empty())
			info("no saves found in " + str(userDir / "Saves") + ", parse test skipped (pass save files as arguments)");
		else
			info("using the " + std::to_string(saves.size()) + " most recent save(s) from " + str(userDir / "Saves"));
	}
	int parsed = 0, errors = 0;
	for (const std::string& s : saves)
	{
		if (!isFile(s))
		{
			fail("save not found: " + s);
			continue;
		}
		info("parsing " + s);
		const int before = loader.loadedStateCount;
		loader.paths.push(s);
		loader.LoadSave_WithoutAsset(true);
		if (loader.loadedStateCount == before)
		{
			fail("save produced no state: " + s);
			continue;
		}
		const CivVIState& state = loader.States[loader.loadedStateCount - 1];
		const CivVIGame& game = *state.pGame;
		char line[512];
		snprintf(line, sizeof(line), "  turn %d, seed %d, %s, %d players%s%s", state.turn, game.seed, game.mapSize, game.nbPlayer,
			state.error.empty() ? "" : ", error: ", state.error.c_str());
		info(line);
		parsed++;
		if (!state.error.empty())
			errors++;
	}
	if (!saves.empty())
	{
		if (parsed == (int)saves.size() && errors == 0)
			ok("parsed " + std::to_string(parsed) + " save(s)");
		else if (autoSaves && parsed > 0)
			warn("parsed " + std::to_string(parsed) + " of " + std::to_string(saves.size()) + " save(s), " + std::to_string(errors) + " with errors");
		else
			fail("parsed " + std::to_string(parsed) + " of " + std::to_string(saves.size()) + " save(s), " + std::to_string(errors) + " with errors");
	}

	// Window, OpenGL, fonts, screenshot
	if (headless)
		info("window test skipped (--headless)");
	else
	{
		Viewer::SelfTest test;
		test.screenshotPath = str(configDir / "selftest.png");
		test.expectSaves = parsed > 0;
		Viewer::selfTest = &test;
		{
			Viewer viewer(1280, 720);
		}
		Viewer::selfTest = nullptr;

		for (const std::string& e : test.glfwErrors)
			warn("GLFW: " + e);
		if (!test.windowOk)
			fail("window: " + (test.error.empty() ? std::string("could not be created") : test.error));
		else
		{
			ok("window: " + test.glInfo);
			if (test.loaderOk)
				ok("OpenGL function loader initialised");
			else
				fail("OpenGL function loader failed (ImGui backend)");
			if (test.uiReady)
				ok("UI ready after " + std::to_string(test.capturedFrame) + " frames");
			else
				warn("UI still loading after " + std::to_string(test.capturedFrame) + " frames, the screenshot may be incomplete");
			if (test.glError != 0)
			{
				char hex[32];
				snprintf(hex, sizeof(hex), "0x%04X", test.glError);
				fail(std::string("OpenGL error ") + hex + " while rendering");
			}
			if (!test.screenshotOk)
				fail("screenshot could not be saved to " + test.screenshotPath);
			else if (test.distinctColors < 4) // a frame with nothing drawn has one colour; an empty UI (menu bar + prompt) has about 15
				fail("screenshot looks blank (" + std::to_string(test.distinctColors) + " colours): the ImGui shaders probably failed, see the ERROR lines above");
			else
				ok("screenshot: " + test.screenshotPath + " (" + std::to_string(test.distinctColors) + " sampled colours)");
		}
	}

	char result[128];
	snprintf(result, sizeof(result), "RESULT: %s (%d failure%s)", failures ? "FAIL" : "PASS", failures, failures == 1 ? "" : "s");
	report("====", result);
	return failures ? 1 : 0;
}
