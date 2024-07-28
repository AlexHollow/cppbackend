#include <fstream>

#include "json_loader.h"

namespace json_loader {

namespace json = boost::json;

json::value ParseFile(const std::filesystem::path& json_path) {
    static const std::size_t BUFF_SIZE = 1024;
    std::ifstream fin(json_path);
    json::stream_parser p;
    json::error_code ec;

    do {
        char buf[BUFF_SIZE];
        fin.read(buf, sizeof(buf));
        auto nread = fin.gcount();
        p.write(buf, nread, ec);
    } while (!fin.eof());
    
    if (ec) {
        return nullptr;
    }
    
    p.finish(ec);

    if (ec) {
        return nullptr;
    }

    return p.release();
}

void AddRoads(model::Map& map, json::array& json_roads) {
    for (const auto& json_road : json_roads) {
        if (json_road.as_object().contains("x1")) { // if road horizontal
            std::int64_t x0 = json_road.as_object().at("x0").as_int64();
            std::int64_t y0 = json_road.as_object().at("y0").as_int64();
            std::int64_t x1 = json_road.as_object().at("x1").as_int64();

            model::Road road(model::Road::HORIZONTAL, model::Point(x0, y0), x1);

            map.AddRoad(road);
        } else {
            std::int64_t x0 = json_road.as_object().at("x0").as_int64();
            std::int64_t y0 = json_road.as_object().at("y0").as_int64();
            std::int64_t y1 = json_road.as_object().at("y1").as_int64();

            model::Road road(model::Road::VERTICAL, model::Point(x0, y0), y1);
            map.AddRoad(std::move(road));
        }
    }
}

void AddBuildings(model::Map& map, json::array& json_buildings) {
    for (const auto& json_building : json_buildings) {
        model::Rectangle rec;

        rec.position.x = json_building.as_object().at("x").as_int64();
        rec.position.y = json_building.as_object().at("y").as_int64();
        rec.size.width = json_building.as_object().at("w").as_int64();
        rec.size.height = json_building.as_object().at("h").as_int64();

        map.AddBuilding(model::Building(std::move(rec)));
    }
}

void AddOffices(model::Map& map, json::array& json_offices) {
    using Id = util::Tagged<std::string, model::Office>;

    for (const auto& json_office : json_offices) {
        std::string office_id = json_office.as_object().at("id").as_string().c_str();
        std::int64_t x = json_office.as_object().at("x").as_int64();
        std::int64_t y = json_office.as_object().at("y").as_int64();
        std::int64_t offset_x = json_office.as_object().at("offsetX").as_int64();
        std::int64_t offset_y = json_office.as_object().at("offsetY").as_int64();

        model::Office office(Id(office_id), model::Point(x, y), model::Offset(offset_x, offset_y));
        map.AddOffice(std::move(office));
    }
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    using Id = util::Tagged<std::string, model::Map>;

    model::Game game;

    json::value json_value = ParseFile(json_path);
    json::array json_maps = json_value.as_object().at("maps").as_array();

    for (auto& json_map : json_maps) {
        std::string map_id = json_map.as_object().at("id").as_string().c_str();
        std::string map_name = json_map.as_object().at("name").as_string().c_str();

        model::Map map(Id(map_id), map_name);

        AddRoads(map, json_map.as_object().at("roads").as_array());
        AddBuildings(map, json_map.as_object().at("buildings").as_array());
        AddOffices(map, json_map.as_object().at("offices").as_array());

        game.AddMap(map);
    }

    return game;
}

json::array GetRoads(const model::Map::Roads& roads) {
    json::array json_roads;
    for (const auto& road : roads) {
        model::Point start = road.GetStart();
        model::Point end = road.GetEnd();

        json::object obj;
        obj["x0"] = start.x;
        obj["y0"] = start.y;

        if (road.IsHorizontal()) {
            obj["x1"] = end.x;
        } else {
            obj["y1"] = end.y;
        }

        json_roads.push_back(std::move(obj));
    }
    return json_roads;
}

json::array GetBuildings(const model::Map::Buildings& buildings) {
    json::array json_buildings;
    for (const auto& building : buildings) {
        model::Rectangle bounds = building.GetBounds();
        json::object obj;
        obj["x"] = bounds.position.x;
        obj["y"] = bounds.position.y;
        obj["w"] = bounds.size.width;
        obj["h"] = bounds.size.height;
        json_buildings.push_back(std::move(obj));
    }
    return json_buildings;
}

json::array GetOffices(const model::Map::Offices& offices) {
    json::array json_offices;
    for (const auto& office : offices) {
        model::Point pos = office.GetPosition();
        model::Offset offset = office.GetOffset();

        json::object obj;
        obj["id"] = *office.GetId();
        obj["x"] = pos.x;
        obj["y"] = pos.y;
        obj["offsetX"] = offset.dx;
        obj["offsetY"] = offset.dy;
        json_offices.push_back(std::move(obj));
    }
    return json_offices;
}

std::string GetSerializedMaps(const std::vector<model::Map>& maps) {
    json::array json_maps;
    for (const auto& map : maps) {
        json::object obj;
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        json_maps.push_back(std::move(obj));
    }
    return json::serialize(json_maps);
}

std::string GetSerializedMap(const model::Map& map) {
    json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    obj["roads"] = GetRoads(map.GetRoads());
    obj["buildings"] = GetBuildings(map.GetBuildings());
    obj["offices"] = GetOffices(map.GetOffices());
    return json::serialize(obj);
}

std::string GetSerializedError(std::string_view code, std::string_view message) {
    json::object obj;
    obj["code"] = std::string(code);
    obj["message"] = std::string(message);
    return json::serialize(obj);
}

}  // namespace json_loader
