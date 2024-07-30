#pragma once

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <filesystem>
#include <boost/json.hpp>

#include "model.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);

std::string GetSerializedMaps(const std::vector<model::Map>& maps);

std::string GetSerializedMap(const model::Map& map);

std::string GetSerializedError(std::string_view code, std::string_view message);

}  // namespace json_loader
