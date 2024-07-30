#ifdef WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <syncstream>
#include <thread>

namespace net = boost::asio;
namespace sys = boost::system;
namespace ph = std::placeholders;
using namespace std::chrono;
using namespace std::literals;
using Timer = net::steady_timer;

namespace {

// ------ Class Logger ------

class Logger {
public:
	explicit Logger(std::string id) : id_{ std::move(id) } {}

	void LogMessage(std::string_view message) {
		std::osyncstream sync_out(std::cout);

		sync_out << id_ << "> ["sv << std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time_).count() << "s] "sv << message << std::endl;
	}

private:
	std::string id_;
	std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
};

// ------ Class ThreadChecker ------

class ThreadChecker {
public:
	explicit ThreadChecker(std::atomic_int& counter) : counter_{ counter } {}

	ThreadChecker(const ThreadChecker&) = delete;
	ThreadChecker& operator=(const ThreadChecker&) = delete;

	~ThreadChecker() {
		assert(expected_counter_ == counter_);
	}

private:
	std::atomic_int& counter_;
	int expected_counter_ = ++counter_;
};

// ------ Class Hamburger ------

class Hamburger {
public:
	[[nodiscard]] bool IsCutletRoasted() const {
		return cutlet_roasted_;
	}
	void SetCutletRoasted() {
		if (IsCutletRoasted()) {
			throw std::logic_error("Cutlet has been roasted already"s);
		}
		cutlet_roasted_ = true;
	}

	[[nodiscard]] bool HasOnion() const {
		return has_onion_;
	}

	void AddOnion() {
		if (IsPacked()) {
			throw std::logic_error("Hamburger has been packed already"s);
		}
		AssureCutletRoasted();
		has_onion_ = true;
	}

	[[nodiscard]] bool IsPacked() const {
		return is_packed_;
	}
	void Pack() {
		AssureCutletRoasted();
		is_packed_ = true;
	}

private:
	void AssureCutletRoasted() const {
		if (!cutlet_roasted_) {
			throw std::logic_error("Cutlet has not been roasted yet"s);
		}
	}

	bool cutlet_roasted_ = false;
	bool has_onion_ = false;
	bool is_packed_ = false;
};

std::ostream& operator<<(std::ostream& out, const Hamburger& hamburger) {
	return out << "Hamburger: "sv << (hamburger.IsCutletRoasted() ? "roasted cutlet"sv : " raw cutlet"sv)
		<< (hamburger.HasOnion() ? ", onion"sv : ""sv)
		<< (hamburger.IsPacked() ? ", packed"sv : ", not packed"sv);
}

// ------ Class Order ------

using OrderHandler = std::function<void(sys::error_code ec, int id, Hamburger* hamburger)>;

class Order : public std::enable_shared_from_this<Order> {
public:
	Order(net::io_context& io_context, int id, bool with_onion, OrderHandler handler)
		: io_context_{ io_context }
		, id_{ id }
		, with_onion_{ with_onion }
		, handler_{ std::move(handler) } {}

	void Execute() {
		logger_.LogMessage("Order has been started."sv);

		RoastCutlet();
		if (with_onion_) {
			MarinadeOnion();
		}
	}

private:
	void RoastCutlet() {
		logger_.LogMessage("Start roasting cutlet"sv);

		roast_timer_.async_wait(
			net::bind_executor(strand_, [self = shared_from_this()](sys::error_code ec) {
				self->OnRoasted(ec);
				}));
	}

	void OnRoasted(sys::error_code ec) {
		ThreadChecker checker(counter_);

		if (ec) {
			logger_.LogMessage("Roast error : "s + ec.what());
		} else {
			logger_.LogMessage("Cutlet has been roasted."sv);
			hamburger_.SetCutletRoasted();
		}
		CheckReadiness(ec);
	}

	void MarinadeOnion() {
		logger_.LogMessage("Start marinade onion"sv);

		marinade_timer_.async_wait(
			net::bind_executor(strand_, [self = shared_from_this()](sys::error_code ec) {
				self->OnOnionMarinaded(ec);
				}));
	}

	void OnOnionMarinaded(sys::error_code ec) {
		ThreadChecker checker(counter_);

		if (ec) {
			logger_.LogMessage("Marinade onion error: "s + ec.what());
		} else {
			logger_.LogMessage("Onion has been marinaded."sv);
			onion_marinaded_ = true;
		}
		CheckReadiness(ec);
	}

	void CheckReadiness(sys::error_code ec) {
		if (delivered_) {
			return;
		}

		if (ec) {
			return Deliver(ec);
		}

		if (CanAddOnion()) {
			logger_.LogMessage("Add onion"sv);
			hamburger_.AddOnion();
		}

		if (IsReadyToPack()) {
			Pack();
		}
	}

	void Deliver(sys::error_code ec) {
		delivered_ = true;
		handler_(ec, id_, ec ? nullptr : &hamburger_);
	}

	[[nodiscard]] bool CanAddOnion() const {
		return hamburger_.IsCutletRoasted() && onion_marinaded_ && !hamburger_.HasOnion();
	}

	[[nodiscard]] bool IsReadyToPack() const {
		return hamburger_.IsCutletRoasted() && (!with_onion_ || hamburger_.HasOnion());
	}

	void Pack() {
		logger_.LogMessage("Packing"sv);

		auto start = std::chrono::steady_clock::now();

		while (std::chrono::steady_clock::now() - start < 500ms) {
			//idle for 500ms
		}

		hamburger_.Pack();
		logger_.LogMessage("Packed"sv);

		Deliver({});
	}

private:
	net::io_context& io_context_;
	int id_;
	bool with_onion_;
	OrderHandler handler_;
	Logger logger_{ std::to_string(id_) };
	net::steady_timer roast_timer_{ io_context_, 1s };
	net::steady_timer marinade_timer_{ io_context_, 2s };
	Hamburger hamburger_;
	bool onion_marinaded_ = false;
	bool delivered_ = false;
	std::atomic_int counter_ = 0;
	net::strand<net::io_context::executor_type> strand_ = net::make_strand(io_context_);
};

// ------ Class Restaurant ------

class Restaurant {
public:
	explicit Restaurant(net::io_context& io_context) : io_context_{ io_context } {}

	int MakeHamburger(bool with_onion, OrderHandler handler) {
		const int order_id = ++next_order_id_;
		std::make_shared<Order>(io_context_, order_id, with_onion, std::move(handler))->Execute();
		return order_id;
	}

private:
	net::io_context& io_context_;
	int next_order_id_ = 0;
};


template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
	n = std::max(1u, n);
	std::vector<std::jthread> workers;
	workers.reserve(n - 1);

	while (--n) {
		workers.emplace_back(fn);
	}
	fn();
}

}  // namespace

int main() {
    const unsigned num_workers = 4;
    net::io_context io(num_workers);

    Restaurant restaurant{io};

    Logger logger{"main"s};

    struct OrderResult {
        sys::error_code ec;
        Hamburger hamburger;
    };

    std::unordered_map<int, OrderResult> orders;

    // Обработчик заказа может быть вызван в любом из потоков, вызывающих io.run().
    // Чтобы избежать состояния гонки при обращении к orders, выполняем обращения к orders через
    // strand, используя функцию dispatch.
    auto handle_result
        = [strand = net::make_strand(io), &orders](sys::error_code ec, int id, Hamburger* h) {
              net::dispatch(strand, [&orders, id, res = OrderResult{ec, ec ? Hamburger{} : *h}] {
                  orders.emplace(id, res);
              });
          };

    const int num_orders = 16;
    for (int i = 0; i < num_orders; ++i) {
        restaurant.MakeHamburger(i % 2 == 0, handle_result);
    }

    assert(orders.empty());
    RunWorkers(num_workers, [&io] {
        io.run();
    });
    assert(orders.size() == num_orders);

    for (const auto& [id, order] : orders) {
        assert(!order.ec);
        assert(order.hamburger.IsCutletRoasted());
        assert(order.hamburger.IsPacked());
        assert(order.hamburger.HasOnion() == (id % 2 != 0));
    }
}
