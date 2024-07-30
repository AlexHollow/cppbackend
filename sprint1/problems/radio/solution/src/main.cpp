#include <boost/asio.hpp>
#include "audio.h"

#include <array>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace net = boost::asio;
using net::ip::udp;

using namespace std::literals;

static const int PORT = 3333;
static const std::size_t MAX_FRAMES = 65000;
static const std::chrono::duration MAX_REC_TIME = 1.5s;

// ----- Prototypes -----
void StartServer(uint16_t port);
void StartClient(std::string_view server_ip, uint16_t port);
void CleanInputTread(std::istream& input);

int main(int argc, char** argv) {
    std::cout << "Enter mode:\n"sv;
    std::cout << "c - Client\n"sv;
    std::cout << "s - Server\n"sv;

    char mode = 0;
    std::cin >> mode;
    mode = std::tolower(mode);
    std::cin.get();

	try {

		switch (mode) {
		case 'c': {
            std::cout << "Enter server IP: ";
			std::string server_ip;
			std::getline(std::cin, server_ip);
            std::cout << std::endl;

			if (!server_ip.empty()) {
				StartClient(server_ip, PORT);
            }

			break;
		}
		case 's': {
			StartServer(PORT);
			break;
		}
		default: break;
		}

    } catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    return 0;
}

void StartServer(uint16_t port) {
    Player player(ma_format_u8, 1);
    net::io_context io_context;
    udp::socket socket(io_context, udp::endpoint(udp::v4(), PORT));
    
    std::cout << "Waiting for messages...\n"sv;

    for (;;) {
        std::array<char, MAX_FRAMES> receive_buffer;
        udp::endpoint remote_endpoint;

        auto size = socket.receive_from(net::buffer(receive_buffer), remote_endpoint);
        std::size_t frames_count = size / player.GetFrameSize();

        player.PlayBuffer(receive_buffer.data(), frames_count, MAX_REC_TIME);
        std::cout << "Playing done\n"sv;
    }
}

void StartClient(std::string_view server_ip, uint16_t port) {
    Recorder recorder(ma_format_u8, 1);
    Player player(ma_format_u8, 1);
    net::io_context io_context;
    udp::socket socket(io_context, udp::v4());
    boost::system::error_code ec;
    udp::endpoint endpoint = udp::endpoint(net::ip::make_address(server_ip, ec), PORT);

    int action = 0;
    for (;action != 2;) {
        std::cout << "\nEnter action:\n"sv;
        std::cout << "1 - Send message\n"sv;
        std::cout << "2 - Exit\n\n"sv;
        std::cin >> action;

        switch (action) {
        case 1: {
            Recorder::RecordingResult rec_result = recorder.Record(MAX_FRAMES, MAX_REC_TIME);
            std::size_t size = rec_result.frames * recorder.GetFrameSize();
            std::cout << "Recording done\n"sv;

            socket.send_to(net::buffer(rec_result.data.data(), size), endpoint);
        }
        default: break;
        }

        if (std::cin.fail()) {
            CleanInputTread(std::cin);
        }
    }
}

void CleanInputTread(std::istream& input) {
    input.clear();
    char trash_buffer[50];
    input >> trash_buffer;
}


//while (true) {
//    std::string str;
//
//    std::cout << "Press Enter to record message..." << std::endl;
//    std::getline(std::cin, str);
//
//    auto rec_result = recorder.Record(65000, 1.5s);
//    std::cout << "Recording done" << std::endl;
//
//    player.PlayBuffer(rec_result.data.data(), rec_result.frames, 1.5s);
//    std::cout << "Playing done" << std::endl;
//}