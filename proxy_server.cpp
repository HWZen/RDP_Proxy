//
// Created by HWZen on 2022/11/11.
// Copyright (c) 2022 HWZen All rights reserved.
// MIT License
//
#ifdef _MSC_VER
#define _WIN32_WINNT 0x0A00
#endif
#include <array>
#include <iostream>
#include <memory>
#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <iostream>

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

awaitable<void> proxy(ref_sock from, ref_sock to)
{
    static int cnt{0};
    int thisCnt = ++cnt;
    std::cout << "proxy " << thisCnt << " begin\n";
    std::array<char, 8102> buf{};
    for(;;)
    {
        auto res = co_await (from->async_read_some(buffer(buf), use_nothrow_awaitable) || timeout(40s));
        if (res.index() == 1){
            std::cerr << "from->read timeout\n";
            asio::error_code ec;
            from->close(ec);
            to->close(ec);
            break;
        }
        auto [ec, n] = std::get<0>(res);
        if (ec)
        {
            std::cerr << "from->read error: " << ec.message() << "\n";
            from->close(ec);
            to->close(ec);
            break;
        }
        auto [ec2, n2] = co_await to->async_write_some(buffer(buf, n), use_nothrow_awaitable);
        if (ec2)
        {
            std::cerr << "to->write error: " << ec2.message() << "\n";
            from->close(ec);
            to->close(ec);
            break;
        }
    }
    std::cout << "proxy " << thisCnt << " end\n";

}


awaitable<void> listen(tcp::acceptor& inAccept, tcp::acceptor &outAccept)
{
    for(;;){

        auto [ec2, outSocket] = co_await outAccept.async_accept(use_nothrow_awaitable);
        if (ec2){
            std::cerr << "outAccept error: " << ec2.message() << "\n";
            break;
        }
        std::cout << "outAccept: " << outSocket.remote_endpoint() << "\n";

        auto res = co_await (inAccept.async_accept(use_nothrow_awaitable) || timeout(30s));
        if (res.index() == 1){
            std::cerr << "inAccept timeout\n";
            outSocket.close();
            continue;
        }
        auto &[ec1, inSocket] = std::get<0>(res);
        if (ec1){
            std::cerr << "inAccept error: " << ec1.message() << "\n";
            break;
        }
        std::cout << "inAccept: " << inSocket.remote_endpoint() << "\n";

        auto in = std::make_shared<decltype(inSocket)>(std::move(inSocket));
        auto out = std::make_shared<decltype(outSocket)>(std::move(outSocket));
        co_spawn(in->get_executor(), proxy(in, out), detached);
        co_spawn(out->get_executor(), proxy(out, in), detached);
    }
}



int main(int argc, char **argv)
{
    uint16_t inPort = 5150;
    uint16_t outPort = 5151;
    if (argc > 1)
    {
        inPort = static_cast<uint16_t>(std::stoi(argv[1]));
    }
    if (argc > 2)
    {
        outPort = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    tcp::endpoint inEndpoint(tcp::v4(), inPort);
    tcp::endpoint outEndpoint(tcp::v4(), outPort);
    asio::io_context ctx;
    tcp::acceptor inAcceptor(ctx, inEndpoint);
    tcp::acceptor outAcceptor(ctx, outEndpoint);
    co_spawn(ctx, listen(inAcceptor, outAcceptor), detached);
    ctx.run();
}

