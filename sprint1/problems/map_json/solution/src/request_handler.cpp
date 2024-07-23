#include "request_handler.h"
#include "json_loader.h"

namespace http_handler {

StringResponse MakeStringResponse(
    http::status status,
    std::string_view body,
    unsigned http_version,
    bool keep_alive,
    http::verb method,
    std::string_view content_type
) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    
    if (method != http::verb::head) {
        response.body() = body;
    }

    if (method != http::verb::get && method != http::verb::head) {
        response.set(http::field::allow, "GET, HEAD");
    };

    response.content_length(body.size());
    response.keep_alive(keep_alive);

    return response;
}

std::vector<std::string> ParseQuery(std::string_view query, char delim) {
    std::vector<std::string> result;

    while (true) {
        std::size_t pos = query.find(delim, 0);
        result.emplace_back(query.substr(0, pos));
        
        if (pos == query.npos) {
            break;
        }

        query.remove_prefix(pos + 1);
    }

    return result;
}

StringResponse RequestHandler::HandleRequest(StringRequest&& request) {
    const auto response = [&request](http::status status, std::string_view text) {
        return MakeStringResponse(status, text, request.version(), request.keep_alive(), request.method());
    };

    http::verb method = request.method();
    std::string body;

    if (method != http::verb::get && method != http::verb::head) {
        return response(http::status::method_not_allowed, "Invalid method");
    }

    auto query = request.target().substr(1);
    std::vector<std::string> query_keys = ParseQuery(query, '/');
    std::size_t query_size = query_keys.size();

    if (query_size != 0 && query_keys[0] == "api") {
        if (query_size >= 3 && query_keys[1] == "v1" && query_keys[2] == "maps") {
            if (query_size == 3) {
                body = json_loader::GetSerializedMaps(game_.GetMaps());
                
            }

            if (query_size == 4) {
                const model::Map* map = game_.FindMap(model::Map::Id(query_keys[3]));

                if (map) {
                    body = json_loader::GetSerializedMap(*map);
                } else {
                    body = json_loader::GetSerializedError("mapNotFound", "Map not found");
                    return response(http::status::not_found, body);
                }
            }
        } else {
            body = json_loader::GetSerializedError("badRequest", "Bad request");
            return response(http::status::bad_request, body);
        }
    }

    return response(http::status::ok, body);
}

}  // namespace http_handler
