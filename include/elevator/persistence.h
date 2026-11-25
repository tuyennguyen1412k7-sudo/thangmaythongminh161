#pragma once
#include <string>
#include <vector>

#include "elevator/config.h"

SystemConfig loadConfig(const std::string& path);
std::vector<ScenarioEvent> loadScenario(const std::string& path);
void saveSnapshot(const Snapshot& s, const std::string& path);
Snapshot loadSnapshot(const std::string& path);
