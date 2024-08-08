#include "logger.h"

#include <iostream>

namespace logger {

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

void Formatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    boost::json::value data = {
        { "timestamp", to_iso_extended_string(*rec[timestamp]) },
        { "data", (*rec[additional_data]).as_object() },
        { "message", *rec[logging::expressions::smessage]}
    };

    strm << boost::json::serialize(data);
}

void LogServerStart(boost::asio::ip::address address, unsigned int port) {
    boost::json::value data = {
        { "port", port },
        { "address", address.to_string() }
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "server started";
}

void LogServerStop(unsigned int code, std::string_view exception) {
    boost::json::value data;
    if (exception.empty()) {
        data = {
            { "code", code }
        };
    } else {
        data = {
            { "code", code },
            { "exception", exception }
        };
    }
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "server exited";
}

void LogError(unsigned int code, std::string_view text, std::string_view where) {
    boost::json::value data = {
        { "code", code },
        { "text", text },
        { "where", where }
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data) << "error";
}

void InitBoostLogConsole() {
    logging::add_common_attributes();
    logging::add_console_log(
        std::clog,
        logging::keywords::format = &Formatter
    );
}

} // namespace logger