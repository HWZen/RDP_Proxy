//
// Created by HWZen on 2022/11/11.
// Copyright (c) 2022 HWZen All rights reserved.
// MIT License
//
#ifdef _MSC_VER
#ifndef _WIN32_WINNT
// Windows 10
#define _WIN32_WINNT 0x0A00
#endif // !_WIN32_WINNT
#endif // _MSC_VER

#include <array>
#include <iostream>
#include <memory>
#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>

using asio::awaitable;
using asio::buffer;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
namespace this_coro = asio::this_coro;
using namespace asio::experimental::awaitable_operators;
using std::chrono::steady_clock;
using namespace std::literals::chrono_literals;
using namespace std::string_literals;

constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(asio::use_awaitable);

using ref_sock = std::shared_ptr<asio::ip::tcp::socket>;



awaitable<void> timeout(steady_clock::duration duration)
{
    asio::steady_timer timer(co_await this_coro::executor);
    timer.expires_after(duration);
    co_await timer.async_wait(use_nothrow_awaitable);
}


awaitable<int> proxy(ref_sock from, ref_sock to) {
    static int cnt{0};
    int thisCnt = ++cnt;
    std::array<char, 8102> buf{};
    std::cout << "proxy " << thisCnt << " begin\n";
    for (;;) {
        auto res = co_await (from->async_read_some(buffer(buf), use_nothrow_awaitable) || timeout(100s));
        if (res.index() == 1){
            std::cerr << "from->read timeout\n";
            asio::error_code ec;
            from->close(ec);
            to->close(ec);
            break;
        }
        auto [ec,n] = std::get<0>(res);
        if (ec) {
            std::cerr << "from->read error: " << ec.message() << "\n";
            from->close(ec);
            to->close(ec);
            break;
        }
        auto [ec2, n2] = co_await to->async_write_some(buffer(buf, n), use_nothrow_awaitable);
        if (ec2) {
            std::cerr << "to->write error: " << ec2.message() << "\n";
            from->close(ec2);
            to->close(ec2);
            break;
        }
    }
    std::cout << "proxy " << thisCnt << " end\n";
    co_return true;
}

int main(int argc, char **argv) {
    auto strProxyServerAddress = "127.0.0.1"s;
    auto strProxyServerOutPort = "5151"s;

    if (argc > 1)
        strProxyServerAddress = argv[1];
    if (argc > 2)
        strProxyServerOutPort = argv[2];

    asio::io_service io_service;
    auto endPoint = *tcp::resolver(io_service).resolve(strProxyServerAddress, strProxyServerOutPort);
    co_spawn(io_service, [&]() -> awaitable<void> {
        for(;;){

            ref_sock socket = std::make_shared<tcp::socket>(io_service);
            {
                auto [ec] = co_await socket->async_connect(endPoint,use_nothrow_awaitable);
                if (ec) {
                    std::cerr << "socket->connect error: " << ec.message() << "\n";
                    continue;
                }
                std::cout << "socket->connect success\n";
            }

            ref_sock rdp_socket = std::make_shared<tcp::socket>(io_service);
            {
                auto [ec] = co_await rdp_socket->async_connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"),3389), use_nothrow_awaitable);
                if (ec) {
                    std::cerr << "rdp_socket->connect error: " << ec.message() << "\n";
                    continue;
                }
                std::cout << "rdp_socket->connect success\n";
            }
            co_await( proxy(socket, rdp_socket) && proxy(rdp_socket, socket));
            std::cout << "proxy close\n";
        }

    }, detached);
    io_service.run();
}