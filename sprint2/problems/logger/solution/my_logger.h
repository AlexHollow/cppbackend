#pragma once
#pragma warning(disable : 4996)

#include <chrono>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>

using namespace std::literals;
namespace fs = std::filesystem;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        std::lock_guard<std::mutex> lg(m_);
        std::string new_log_file_name = "sample_log_" + GetFileTimeStamp() + ".log";
        fs::path file_path = fs::path("/var/log") / fs::path(new_log_file_name);

        if (new_log_file_name != current_log_file_name_) {
            log_file_.close();
            log_file_.open(file_path, std::ios::app);
        }

        if (log_file_.is_open()) {
            current_log_file_name_ = new_log_file_name;

            log_file_ << GetTimeStamp() << ": ";
            SendValueToStream(args...);
            log_file_ << std::endl;
        }
    }

    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lg(m_);
        manual_ts_ = ts;
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;

    template<typename Head, typename... Tail>
    void SendValueToStream(Head value, Tail... args) {
        log_file_ << value;

        if constexpr (sizeof...(args) != 0) {
            SendValueToStream(args...);
        }
    }

    std::chrono::system_clock::time_point GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }

        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto t_c = std::chrono::system_clock::to_time_t(GetTime());
        return std::put_time(std::gmtime(&t_c), "%F %T");
    }

    std::string GetFileTimeStamp() const {
        const auto t_c = std::chrono::system_clock::to_time_t(GetTime());
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&t_c), "%F");
        return ss.str();
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::ofstream log_file_;
    std::string current_log_file_name_;
    std::mutex m_;
};
