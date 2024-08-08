#pragma once

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/algorithm/string.hpp>
#include <filesystem>
#include <map>
#include <variant>

#include "http_server.h"
#include "model.h"

namespace http_handler {

using namespace std::literals;

namespace sys = boost::system;
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace fs = std::filesystem;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;

using Response = std::variant<StringResponse, FileResponse>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view TEXT_CSS = "text/css"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    constexpr static std::string_view TEXT_JS = "text/javascript"sv;
    constexpr static std::string_view APP_JSON = "application/json"sv;
    constexpr static std::string_view APP_XML = "application/xml"sv;
    constexpr static std::string_view IMAGE_PNG = "image/png"sv;
    constexpr static std::string_view IMAGE_JPEG = "image/jpeg"sv;
    constexpr static std::string_view IMAGE_GIF = "image/gif"sv;
    constexpr static std::string_view IMAGE_BMP = "image/bmp"sv;
    constexpr static std::string_view IMAGE_ICO = "image/vnd.microsoft.icon"sv;
    constexpr static std::string_view IMAGE_TIFF = "image/tiff"sv;
    constexpr static std::string_view IMAGE_SVG = "image/svg+xml"sv;
    constexpr static std::string_view AUDIO_MP3 = "audio/mpeg"sv;
    constexpr static std::string_view EMPTY_HEADER = "application/octet-stream"sv;
};

const std::map<std::string_view, std::string_view> EXTENSIONS = {
    { ".htm", ContentType::TEXT_HTML },
    { ".html", ContentType::TEXT_HTML },
    { ".css", ContentType::TEXT_CSS },
    { ".txt", ContentType::TEXT_PLAIN },
    { ".js", ContentType::TEXT_JS },
    { ".json", ContentType::APP_JSON },
    { ".xml", ContentType::APP_XML },
    { ".png", ContentType::IMAGE_PNG },
    { ".jpg", ContentType::IMAGE_JPEG },
    { ".jpe", ContentType::IMAGE_JPEG },
    { ".jpeg", ContentType::IMAGE_JPEG },
    { ".gif", ContentType::IMAGE_GIF },
    { ".bmp", ContentType::IMAGE_BMP },
    { ".ico", ContentType::IMAGE_ICO },
    { ".tif", ContentType::IMAGE_TIFF },
    { ".tiff", ContentType::IMAGE_TIFF },
    { ".svg", ContentType::IMAGE_SVG },
    { ".svgz", ContentType::IMAGE_SVG },
    { ".mp3", ContentType::AUDIO_MP3 }
};

std::string_view GetContentType(std::string extension);

StringResponse MakeStringResponse(
    http::status status,
    std::string_view body,
    unsigned http_version,
    bool keep_alive,
    http::verb method,
    std::string_view content_type = ContentType::APP_JSON
);

Response MakeFileResponse(
    const StringRequest& request,
    const fs::path& base_path,
    const std::string& query
);


class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, std::string_view path)
        : game_{game}
        , base_path_(path) {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto response = HandleRequest(std::move(req));

        if (std::holds_alternative<StringResponse>(response)) {
            send(std::get<StringResponse>(response));
        } else {
            send(std::get<FileResponse>(response));
        }
    }

private:
    Response HandleRequest(StringRequest&& request);

private:
    model::Game& game_;
    fs::path base_path_;
};

}  // namespace http_handler
