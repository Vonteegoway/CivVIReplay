#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Thin OS abstraction so the rest of the code stays platform agnostic.
namespace Platform
{
	namespace fs = std::filesystem;

	// Directory containing the executable (assets are resolved relative to it)
	fs::path exeDir();
	// exeDir() / relative
	fs::path assetPath(const fs::path& relative);

	// Writable per-user config directory (created on demand)
	fs::path configDir();

	// Steam install roots that exist on this machine
	std::vector<fs::path> steamRoots();
	// Every Steam library folder (the directory containing "steamapps")
	std::vector<fs::path> steamLibraries();

	// Civ VI per-user folder: ".../Sid Meier's Civilization VI" (contains Saves, Mods...)
	fs::path civUserDir();
	// Firaxis gameplay database cache, empty path if not found
	fs::path civCacheDbPath();

	void sleepMs(unsigned int ms);
	void installCrashHandler();

	// Case-insensitive lookup of "<folder>/<name>.png" under exeDir()/image. Empty if missing.
	std::string findImage(const std::string& folder, const std::string& name);
}
