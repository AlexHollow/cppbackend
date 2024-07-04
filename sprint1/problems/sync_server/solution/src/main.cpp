#ifdef WIN32
#include <sdkddkver.h>
#endif
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <string>
#include <string_view>
#include <optional>
#include <thread>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

using namespace std::literals;

// ------ Constants ------
static const net::ip::address ADDRESS = net::ip::make_address("0.0.0.0");
static const int PORT = 8080;

// ------ Structs ------
struct ContentType {
	ContentType() = delete;
	constexpr static std::string_view TEXT_HTML = "text/html"sv;
};

// ------ Prototypes ------
template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handler);
std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buf);
void DumpRequest(const StringRequest& request);

StringResponse MakeStringResponse(
	http::status status,
	std::string_view body,
	unsigned int http_version,
	bool keep_alive,
	http::verb method,
	std::string_view content_type);

StringResponse HandleRequest(StringRequest&& request);

int main() {
	net::io_context io_context;
	tcp::acceptor accepter(io_context, tcp::endpoint(ADDRESS, PORT));

	std::cout << "Server has started..."sv << std::endl;

	while (true) {
		tcp::socket socket(io_context);
		accepter.accept(socket);

		std::thread th([](tcp::socket sock) {
			HandleConnection(sock, HandleRequest);
			}, std::move(socket));

		th.detach();
	}

	return 0;
}

template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handler) {
	try {
		beast::flat_buffer buffer;

		while (auto request = ReadRequest(socket, buffer)) {
			DumpRequest(*request);
			StringResponse response = handler(*std::move(request));

			http::write(socket, response);

			if (response.need_eof()) {
				break;
			}
		}

	} catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
	}

	beast::error_code ec;
	socket.shutdown(tcp::socket::shutdown_send, ec);
}

std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buf) {
	beast::error_code ec;
	StringRequest request;

	http::read(socket, buf, request, ec);

	if (ec == http::error::end_of_stream) {
		return std::nullopt;
	}

	if (ec) {
		throw std::runtime_error("Failed to read request: "s.append(ec.message()));
	}

	return request;
}

void DumpRequest(const StringRequest& request) {
	std::cout << request.method_string() << ' ' << request.target() << std::endl;

	for (const auto& header : request) {
		std::cout << "  " << header.name_string() << " : " << header.value() << std::endl;
	}
}

StringResponse MakeStringResponse(
	http::status status,
	std::string_view body,
	unsigned int http_version,
	bool keep_alive,
	http::verb method,
	std::string_view content_type = ContentType::TEXT_HTML)
{
	StringResponse response(status, http_version);

	// Добавляем заголовок Content-Type: text/html
	response.set(http::field::content_type, content_type);

	if (method != http::verb::get && method != http::verb::head) {
		response.set(http::field::allow, "GET, HEAD");
	};

	response.body() = body;

	// Формируем заголовок Content-Length, сообщающий длину тела ответа
	response.content_length(body.size());

	// Формируем заголовок Connection в зависимости от значения заголовка в запросе
	response.keep_alive(keep_alive);

	return response;
}

StringResponse HandleRequest(StringRequest&& request) {
	const auto text_response = [&request](http::status status, std::string_view text, http::verb method) {
		return MakeStringResponse(status, text, request.version(), request.keep_alive(), method);
		};

	http::verb method = request.method();
	
	if (method == http::verb::get) {
		std::string body = "Hello, ";
		body += request.target().substr(1);

		return text_response(http::status::ok, body, method);
	}

	if (method == http::verb::head) {
		return text_response(http::status::ok, std::string_view(), method);
	}

	return text_response(http::status::method_not_allowed, "Invalid method", method);
}
