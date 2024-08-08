#pragma once

#include "json_loader.h"

#include <boost/asio/ip/address.hpp>
#include <boost/date_time.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

namespace logger {

namespace logging = boost::log;

void Formatter(logging::record_view const& rec, logging::formatting_ostream& strm);

void LogServerStart(boost::asio::ip::address address, unsigned int port);

void LogServerStop(unsigned int code, std::string_view exception);

void LogError(unsigned int code, std::string_view text, std::string_view where);

void InitBoostLogConsole();

} // namespace logger