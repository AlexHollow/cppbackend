#pragma once

#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>
#include <syncstream>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        {
            std::lock_guard g(lock_time_);
            if (manual_ts_) {
                return *manual_ts_;
            }
        }
        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp(const std::chrono::_V2::system_clock::time_point now) const {
        // const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        return std::put_time(std::localtime(&t_c), "%F %T");
    }

    // Для имени файла возьмите дату с форматом "%Y_%m_%d"
    std::string GetFileTimeStamp(const std::chrono::_V2::system_clock::time_point now) const {
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t_c), "%Y_%m_%d");
        return ss.str();
    }

    void CheckNameLogFile(const std::chrono::_V2::system_clock::time_point now) {
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        int day_of_month = std::localtime(&t_c)->tm_mday;
        if (day_of_file_ != day_of_month) {
            day_of_file_ = day_of_month;
            std::string file_name = "/var/log/sample_log_"s + GetFileTimeStamp(now) + ".log"s;
            log_file_.close();
            log_file_.open(file_name); //, std::ios::app
            if (!log_file_) {
                throw std::runtime_error("File not open!"s);
            }
        }
    }

    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }

    template<typename T0, class... Ts>
    void LogImpl(std::ostream& out, const T0& v0, const Ts&... args) {
        using namespace std::literals;
        out << v0;
        (..., (out << args)); //  << " "sv
    }

    // Выведите в поток все аргументы.
    template<class... Ts>
    void Log(const Ts&... args) {
        const auto now = GetTime();


        std::stringstream strm;
        if constexpr (sizeof...(args) != 0) {
            LogImpl(strm, args...);
        }
        std::lock_guard g(lock_file_);
        CheckNameLogFile(now);
        // std::osyncstream sync_stream(log_file_);
        log_file_ << GetTimeStamp(now) << ": "sv << strm.str() << std::endl;
    }


    // Установите manual_ts_. Учтите, что эта операция может выполняться
    // параллельно с выводом в поток, вам нужно предусмотреть 
    // синхронизацию.
    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard g(lock_time_);
        manual_ts_ = ts;
    }

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    // для демонстрации пока оставим файл в текущей директории
    int day_of_file_ = 0;
    std::ofstream log_file_;

    mutable std::mutex lock_time_;
    mutable std::mutex lock_file_;
};