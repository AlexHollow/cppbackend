#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

// ----- Prototypes -----

std::string ReadInput();
unsigned int ReadSeed();
std::string ReadIp();
std::string ReadPort();

// ----------------------

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return { {buf.data(), sz} };
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    return !ec;
}

class SeabattleAgent {
    using move_result = std::optional<std::pair<int, int>>;

public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    void StartGame(tcp::socket& socket, bool my_initiative) {
        
        while (!IsGameEnded()) {
            PrintFields();

            if (my_initiative) {
                std::cout << "Your turn: "sv;
                std::string move = ReadInput();
                std::cout << std::endl;

                move_result move_coordinates = ParseMove(move);

                if (move_coordinates != std::nullopt) {
                    SendMove(socket, move_coordinates);

                    auto move_res = ReadResult(socket);
                    if (!move_res) {
                        std::cout << "Error reading move"sv << std::endl;
                        continue;
                    }

                    auto shot_res = static_cast<SeabattleField::ShotResult>((*move_res).c_str()[0]);

                    MarkShot(shot_res, move_coordinates, other_field_);
                    ShowMessage(shot_res);
                    //PrintFields();

                    if (shot_res == SeabattleField::ShotResult::MISS) {
                        my_initiative = !my_initiative;
                    }

                } else {
                    std::cout << "Invalid move. Try again"sv << std::endl;
                    continue;
                }
            } else {
                std::cout << "Waiting for turn..."sv << std::endl;

                auto move_res = ReadMove(socket);
                if (!move_res) {
                    std::cout << "Error reading move"sv << std::endl;
                    continue;
                }

                move_result move_coordinates = ParseMove(*move_res);

                auto shot_res = my_field_.Shoot(move_coordinates->first, move_coordinates->second);
                MarkShot(shot_res, move_coordinates, my_field_);
               // PrintFields();

                SendResult(socket, shot_res);

                if (shot_res == SeabattleField::ShotResult::MISS) {
                    my_initiative = !my_initiative;
                }
            }
        }
        
        std::cout << std::endl;

        if (other_field_.IsLoser()) {
            std::cout << "You won!"sv << std::endl;
        } else {
            std::cout << "You lose"sv << std::endl;
        }
    }

private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = sv[0] - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 > 8) return std::nullopt;
        if (p2 < 0 || p2 > 8) return std::nullopt;

        return { {p2, p1} };
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = { static_cast<char>(move.second) + 'A', static_cast<char>(move.first) + '1' };
        return { buff, 2 };
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    bool SendMove(tcp::socket& socket, const move_result& move) {
        std::string line = MoveToString(*move);
        return WriteExact(socket, line);
    }

    std::optional<std::string> ReadMove(tcp::socket& socket) {
        return ReadExact<2>(socket);
    }

    bool SendResult(tcp::socket& socket, const SeabattleField::ShotResult& shot_res) {
        char res = static_cast<char>(shot_res);
        return WriteExact(socket, std::string_view(&res, 1));
    }

    std::optional<std::string> ReadResult(tcp::socket& socket) {
        return ReadExact<1>(socket);
    }

    void MarkShot(const SeabattleField::ShotResult& shot_res, const move_result& move, SeabattleField& field) {
        switch (shot_res) {
        case SeabattleField::ShotResult::MISS:
            field.MarkMiss(move->first, move->second);
            break;
        case SeabattleField::ShotResult::HIT:
            field.MarkHit(move->first, move->second);
            break;
        case SeabattleField::ShotResult::KILL:
            field.MarkKill(move->first, move->second);
            break;
        default: break;
        }
    }

    void ShowMessage(const SeabattleField::ShotResult& shot_res) {
        switch (shot_res) {
        case SeabattleField::ShotResult::MISS:
            std::cout << "Miss"sv << std::endl;
            break;
        case SeabattleField::ShotResult::HIT:
            std::cout << "Hit!"sv << std::endl;
            break;
        case SeabattleField::ShotResult::KILL:
            std::cout << "Kill!"sv << std::endl;
            break;
        default: break;
        }
    }

private:
    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field, unsigned short port) {
    SeabattleAgent agent(field);
    
    net::io_context io_context;

    tcp::acceptor accepter(io_context, tcp::endpoint(tcp::v4(), port));
    std::cout << "Waiting for connection..."sv << std::endl;
    
    boost::system::error_code ec;
    tcp::socket socket(io_context);
    accepter.accept(socket, ec);

    if (ec) {
        std::cout << "Something went wrong"sv << std::endl;
        return;
    }

    agent.StartGame(socket, false);
};

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    SeabattleAgent agent(field);

    net::io_context io_context;

    boost::system::error_code ec;
    tcp::socket socket(io_context);
    tcp::endpoint endpoint = tcp::endpoint(net::ip::make_address(ip_str, ec), port);

    if (ec) {
        std::cout << "Wrong IP format"sv << std::endl;
        return;
    }

    socket.connect(endpoint, ec);

    if (ec) {
        std::cout << "Couldn't connect to server"sv << std::endl;
        return;
    }

    agent.StartGame(socket, true);
};


int main() {
    enum class Mode {
        SERVER = 1,
        CLIENT = 2
    };

    try {
        std::cout << "Welcome to Sea Battle!\n\n"sv;
        std::cout << "Enter mode: \n"sv;
        std::cout << "1 - Server\n"sv;
        std::cout << "2 - Client"sv << std::endl;

        int mode = std::stoi(ReadInput());

        if (mode != 1 && mode != 2) {
            std::cout << "Wrong mode" << std::endl;
            return 1;
        }

		unsigned int seed = ReadSeed();
		std::mt19937 engine(seed);
		SeabattleField fieldL = SeabattleField::GetRandomField(engine);

		switch (mode) {
		case static_cast<int>(Mode::SERVER): {
			int port = std::stoi(ReadPort());
			StartServer(fieldL, port);
			break;
		}
		case static_cast<int>(Mode::CLIENT): {
			std::string ip = ReadIp();
			int port = std::stoi(ReadPort());
			StartClient(fieldL, ip, port);
			break;
		}
		default: break;
		}

    } catch (const std::exception e) {
        std::cout << e.what() << std::endl;
    }

    return 0;
}


std::string ReadInput() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

unsigned int ReadSeed() {
    std::cout << "Enter seed <1234>: "sv << std::endl;
    return std::stoi(ReadInput());
}

std::string ReadIp() {
    std::cout << "Enter IP <255.255.255.255>: "sv << std::endl;
    return ReadInput();
}

std::string ReadPort() {
    std::cout << "Enter port <3333>: "sv << std::endl;
    return ReadInput();
}