#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <syncstream>
#include <utility>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

using namespace std::literals;
using namespace std::chrono_literals;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// ---------- class Order ----------

class Order : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io_context, int id, std::shared_ptr<Sausage> sausage, std::shared_ptr<Bread> bread, HotDogHandler handler)
        : io_context_{ io_context }
        , id_{ id }
        , sausage_{ std::move(sausage) }
        , bread_{ std::move(bread) }
        , handler_{ std::move(handler) } {}

    void Execute(std::shared_ptr<GasCooker> gas_cooker) {
        FrySausage(gas_cooker);
        BakeBread(gas_cooker);
    }

    int GetId() const {
        return id_;
    }

    std::shared_ptr<HotDog> GetHotDog() {
        return hotdog_;
    }

private:
    void FrySausage(std::shared_ptr<GasCooker> gas_cooker) {
        sausage_->StartFry(*gas_cooker, [self = shared_from_this()]() {
            self->OnFrySausageStart();
        });
    }

    void OnFrySausageStart() {
        fry_timer_.expires_from_now(1500ms);
        fry_timer_.async_wait(net::bind_executor(strand_, [self = shared_from_this()](sys::error_code error) {
            self->OnSausageFried();
        }));
    }

    void OnSausageFried() {
        sausage_->StopFry();
        CheckReadiness();
    }

    void BakeBread(std::shared_ptr<GasCooker> gas_cooker) {
        bread_->StartBake(*gas_cooker, [self = shared_from_this()]() {
            self->OnBakeStart();
        });
    }

    void OnBakeStart() {
        bake_timer_.expires_from_now(1s);
        bake_timer_.async_wait(net::bind_executor(strand_, [self = shared_from_this()](sys::error_code error) {
            self->OnBreadBaked();
        }));
    }

    void OnBreadBaked() {
        bread_->StopBaking();
        CheckReadiness();
    }

    void CheckReadiness() {
        if (IsCooked()) {
            hotdog_ = std::make_shared<HotDog>(id_, sausage_, bread_);
            Deliver();
        }
    }

    [[nodiscard]] bool IsCooked() const {
        return sausage_->IsCooked() && bread_->IsCooked();

    }

    void Deliver() {
        Result<HotDog> result(*hotdog_);
        handler_(result);
    }

private:
    net::io_context& io_context_;
    int id_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;
    HotDogHandler handler_;
    std::shared_ptr<HotDog> hotdog_;
    net::steady_timer fry_timer_{ io_context_ };
    net::steady_timer bake_timer_{ io_context_ };
    net::strand<net::io_context::executor_type> strand_ = net::make_strand(io_context_);
};

// ---------- class Cafeteria ----------

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{ io } {
    }

    void OrderHotDog(HotDogHandler handler) {
        std::shared_ptr<Sausage> sausage = store_.GetSausage();
        std::shared_ptr<Bread> bread = store_.GetBread();

        const int order_id = ++next_order_id_;
        std::shared_ptr<Order> order = std::make_shared<Order>(io_, order_id, sausage, bread, std::move(handler));
        order->Execute(gas_cooker_);
    }

private:
    net::io_context& io_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    int next_order_id_ = 0;
};