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

std::string DecodeURL(std::string_view url) {
    std::string result;
    auto it = url.begin();

    while (it != url.end()) {

        if (*it == '+') {
            result.push_back(' ');
            ++it;
            continue;
        }

        if (*it == '%') {
            std::string temp;
            temp.push_back(*(++it));
            temp.push_back(*(++it));
            char* p_end{};
            result.push_back(static_cast<char>(std::strtol(temp.data(), &p_end, 16)));
            ++it;
            continue;
        }
        result.push_back(*(it++));
    }

    return result;
}

bool IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }

    return true;
}

std::string_view GetContentType(std::string extension) {
    boost::algorithm::to_lower(extension);

    if (EXTENSIONS.count(extension)) {
        return EXTENSIONS.at(extension);
    }

    return ContentType::EMPTY_HEADER;
}

Response MakeFileResponse(
    const StringRequest& request,
    const fs::path& base_path,
    const std::string& query
) {
    fs::path rel_path = DecodeURL(query);
    fs::path abs_path = fs::weakly_canonical(base_path / rel_path);
    
    if (!IsSubPath(abs_path, base_path)) {
        return MakeStringResponse(
            http::status::bad_request,
            "Bad request",
            request.version(),
            request.keep_alive(),
            request.method(),
            ContentType::TEXT_PLAIN
        );
    }

    if (query.empty()) {
        abs_path = abs_path / fs::path("index.html");
    }

    http::file_body::value_type file;

    if (sys::error_code ec; file.open(abs_path.string().data(), beast::file_mode::read, ec), ec) {
        return MakeStringResponse(
            http::status::not_found,
            "File not found",
            request.version(),
            request.keep_alive(),
            request.method(),
            ContentType::TEXT_PLAIN
        );
    }

    FileResponse response;
    response.version(11);
    response.result(http::status::ok);
    response.insert(http::field::content_type, GetContentType(abs_path.extension().string()));
    response.body() = std::move(file);
    response.prepare_payload();
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

Response RequestHandler::HandleRequest(StringRequest&& request) {
	const auto string_response = [&request](http::status status, std::string_view text) {
		return MakeStringResponse(status, text, request.version(), request.keep_alive(), request.method());
	};

	http::verb method = request.method();
	std::string body;

	if (method != http::verb::get && method != http::verb::head) {
		return string_response(http::status::method_not_allowed, "Invalid method");
	}

	auto query = request.target().substr(1);
	std::vector<std::string> query_keys = ParseQuery(query, '/');
	std::size_t query_size = query_keys.size();

	if (query_size != 0 && query_keys[0] == "api") {
		if (query_size >= 3 && query_keys[1] == "v1" && query_keys[2] == "maps") {
			if (query_size == 3) {
				body = json_loader::GetSerializedMaps(game_.GetMaps());
                return string_response(http::status::ok, body);
			}

			if (query_size == 4) {
				const model::Map* map = game_.FindMap(model::Map::Id(query_keys[3]));

				if (map) {
					body = json_loader::GetSerializedMap(*map);
					return string_response(http::status::ok, body);
				} else {
					body = json_loader::GetSerializedError("mapNotFound", "Map not found");
					return string_response(http::status::not_found, body);
				}
			}
            return string_response(http::status::ok, body);

        } else {
            body = json_loader::GetSerializedError("badRequest", "Bad request");
            return string_response(http::status::bad_request, body);
        }
    }

    return MakeFileResponse(request, base_path_, std::string(query));
}

}  // namespace http_handler