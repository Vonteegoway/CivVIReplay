#pragma once

// `CivVIReplay --selftest [--headless] [save...]`: checks assets, config dir, Steam / Civ VI
// detection, the gameplay database, parses the given saves, then opens a window, renders a
// few frames and saves a screenshot. Prints one line per check and returns 1 on any failure.
int runSelfTest(int argc, char** argv);
