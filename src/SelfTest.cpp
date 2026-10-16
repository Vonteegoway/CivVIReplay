#include "SelfTest.h"
#include "Platform.h"
#include "SaveLoader.h"
#include "Viewer.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

extern SaveLoader loader;

namespace fs = std::filesystem;

namespace
{
	int failures = 0;

	void report(const char* tag, const std::string& message)
	{
		printf("[%s] %s\n", tag, message.c_str());
		fflush(stdout);
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

	const char* platformName()
	{
#if defined(_WIN32)
		return "Windows";
#elif defined(__APPLE__)
		return "macOS";
#else
		return "Linux";
#endif
	}
}

int runSelfTest(int argc, char** argv)
{
	bool headless = false;
	std::vector<std::string> saves;
	for (int i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "--headless"))
			headless = true;
		else
			saves.push_back(argv[i]);
	}

	printf("CivVIReplay self-test (%s)\n", platformName());

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

	// Writable config dir
	const fs::path configDir = Platform::configDir();
	{
		const fs::path probe = configDir / "selftest.tmp";
		std::ofstream out(probe);
		out << "ok";
		out.close();
		std::error_code ec;
		if (isFile(probe))
			ok("config dir: " + str(configDir));
		else
			fail("config dir not writable: " + str(configDir));
		fs::remove(probe, ec);
	}

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
		warn("Civ VI install not found (saves can still be parsed)");

	const fs::path userDir = Platform::civUserDir();
	if (isDir(userDir / "Saves"))
		ok("Civ VI saves: " + str(userDir / "Saves"));
	else
		warn("Civ VI user folder not found (the Open dialog will start in " + str(userDir) + ")");
	if (isDir(loader.bbgPath))
		info("BBG mod folder: " + str(loader.bbgPath));

	const fs::path cacheDb = Platform::civCacheDbPath();
	if (cacheDb.empty())
		warn("gameplay database not found, the bundled copy will be used");
	else
		info("gameplay database: " + str(cacheDb));

	// Database and saves (LoadSave(true) loads the database, then parses the queue and returns)
	for (const std::string& s : saves)
		loader.paths.push(s);
	loader.LoadSave(true);
	info("database in use: " + str(loader.assets.database.path));
	int leaders = 0;
	loader.assets.database.GetTable("Leaders", countRows, &leaders);
	if (leaders > 0)
		ok("database: " + std::to_string(leaders) + " leaders");
	else
		fail("database: no leaders found");
	info("image assets: " + std::to_string(loader.assets.imageAssets.size()) + " / " + std::to_string(loader.assets.assets.size()) + " database types");

	if (!saves.empty())
	{
		int errors = 0;
		for (int i = 0; i < loader.loadedStateCount; i++)
		{
			const CivVIState& state = loader.States[i];
			const CivVIGame& game = *state.pGame;
			char line[512];
			snprintf(line, sizeof(line), "save %d: turn %d, seed %d, %s, %d players%s%s", i, state.turn,
				game.seed, game.mapSize, game.nbPlayer, state.error.empty() ? "" : ", error: ", state.error.c_str());
			info(line);
			if (!state.error.empty())
				errors++;
		}
		if (loader.loadedStateCount == (int)saves.size() && errors == 0)
			ok("parsed " + std::to_string(loader.loadedStateCount) + " save(s)");
		else
			fail("parsed " + std::to_string(loader.loadedStateCount) + " of " + std::to_string(saves.size()) + " save(s), " + std::to_string(errors) + " with errors");
	}

	// Window, OpenGL, fonts, screenshot
	if (headless)
		info("window test skipped (--headless)");
	else
	{
		Viewer::SelfTest test;
		test.screenshotPath = str(configDir / "selftest.png");
		Viewer::selfTest = &test;
		{
			Viewer viewer(1280, 720);
		}
		Viewer::selfTest = nullptr;

		if (test.windowOk)
			ok("window: " + test.glInfo);
		else
			fail("window: " + (test.error.empty() ? std::string("could not be created") : test.error));
		if (test.windowOk)
		{
			if (test.backendOk)
				ok("ImGui OpenGL backend initialised");
			else
				fail("ImGui OpenGL backend failed (shaders did not compile?)" + (test.error.empty() ? "" : ": " + test.error));
			if (test.screenshotOk)
				ok("screenshot: " + test.screenshotPath);
			else
				fail("screenshot could not be saved to " + test.screenshotPath);
		}
	}

	printf("\nRESULT: %s (%d failure%s)\n", failures ? "FAIL" : "PASS", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
