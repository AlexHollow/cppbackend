#pragma once

#include "request_handler.h"
#include "logger.h"

#include <boost/beast/http.hpp>
#include <chrono>

namespace logger {

namespace beast = boost::beast;
namespace http = beast::http;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

template<typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    LoggingRequestHandler(SomeRequestHandler& handler) : request_handler_{ handler } {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(std::string_view ip, http::request<Body, http::basic_fields<Allocator>>&& request, Send&& send) {
        LogRequest(ip, request);

        const auto start_point = std::chrono::steady_clock::now();
        auto handler = [&send, self = this, start_point](auto&& response) {
            self->LogResponse(response, start_point);
            send(std::forward<decltype(response)>(response));
        };

        request_handler_(std::forward<decltype(request)>(request), std::move(handler));
    }

private:
    static void LogRequest(std::string_view ip, const http::request<http::string_body>& request) {
        auto target = request.target();
        boost::json::value data = {
            { "ip", ip },
            { "URI", std::string(target.data(), target.size()) },
            { "method", GetMethod(request.method()) }
        };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "request received";
    }

    template<typename Body>
    static void LogResponse(const http::response<Body>& response, const std::chrono::_V2::steady_clock::time_point& start_point) {
        const auto dur = std::chrono::steady_clock::now() - start_point;
        boost::json::value data = {
            { "response_time", std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() },
            { "code", response.result_int() },
            { "content_type", std::string() }
        };

        if (response.base().count(http::field::content_type) == 0) {
            data.as_object().at("content_type") = nullptr;
        } else {
            data.as_object().at("content_type") = std::string(response.base().at(http::field::content_type));
        }
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "response sent";
    }

    static std::string GetMethod(http::verb method) {
        switch (method) {
        case http::verb::get: return "GET";
        case http::verb::head: return "HEAD";
        default: return "UNKNOWN";
        }
    }

private:
    SomeRequestHandler& request_handler_;
};

} // namespace logger