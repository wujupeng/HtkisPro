#pragma once

#include <string>
#include <vector>
#include "Config.h"

class RelayConfigStore {
public:
    static std::vector<Config::RelayNodeConfig> load();
    static bool save(const std::vector<Config::RelayNodeConfig>& nodes);
};