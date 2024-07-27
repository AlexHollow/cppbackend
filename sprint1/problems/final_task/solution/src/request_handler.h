#pragma once

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include "http_server.h"
#include "model.h"

namespace http_handler {

using namespace std::literals;

namespace beast = boost::beast;
namespace http = boost::beast::http;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view APP_JSON = "application/json"sv;
};

StringResponse MakeStringResponse(
    http::status status,
    std::string_view body,
    unsigned http_version,
    bool keep_alive,
    http::verb method,
    std::string_view content_type = ContentType::APP_JSON
);

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        send(HandleRequest(std::move(req)));
    }

private:
    StringResponse HandleRequest(StringRequest&& request);

private:
    model::Game& game_;
};

}  // namespace http_handler
