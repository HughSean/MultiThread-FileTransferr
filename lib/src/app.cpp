﻿//
// Created by xSeung on 2023/4/21.
//
#include "app.h"
#include "config.h"
#include "filesystem"
#include "spdlog/spdlog.h"
#include "task.h"
#include "json/value.h"

namespace mtft {

    App::App() {
        mstop = false;
        // mudplisten = std::thread([this] { udplisten(); });
        mtcplisten = std::thread([this] { tcplisten(); });
        // std::filesystem::create_directories(DIR);
        // spdlog::info("工作目录: {}", DIR);
    }
    App::~App() {
        mstop = true;
        io_context ioc;
        // ip::udp::socket usck(ioc, ip::udp::endpoint(ip::udp::v4(), 0));
        ip::tcp::socket tsck(ioc, ip::tcp::endpoint(ip::tcp::v4(), 0));
        // usck.send_to(buffer(""), ip::udp::endpoint(ip::address_v4::loopback(), UDPPORT));
        tsck.connect(ip::tcp::endpoint(ip::address_v4::loopback(), TCPPORT));
        // usck.close();
        // mudplisten.join();
        mtcplisten.join();
        // spdlog::info("清理工作目录: {}", DIR);
        // std::filesystem::remove_all(DIR);
    }

    void App::send(const std::string& fPath, const ip::address_v4& ip) {
        std::ifstream infs(fPath, std::ios::binary);
        if (!infs.good()) {
            spdlog::warn("文件({})读取失败", fPath);
            return;
        }
        io_context        ioc;
        ip::tcp::endpoint edp(ip, TCPPORT);
        ip::tcp::socket   sck(ioc);
        streambuf         buf;
        Json::Value       json;
        try {
            // 连接到对端TCP监听线程
            sck.connect(edp);
            // 获取文件信息[name, size]
            int64_t totalsize = infs.seekg(0, std::ios::end).tellg();
            auto    rvec      = FileReader::Builder(THREAD_N, totalsize, fPath);
            auto    name      = fPath.substr(fPath.find_last_of('\\') + 1);
            spdlog::info("name: {} size:{}", name, totalsize);
            // 信息写入json, 并发送到对端
            json[FILESIZE] = totalsize;
            json[FILENAME] = name;
            WriteJsonToBuf(buf, json);
            auto size = write(sck, buf);
            buf.consume(size);
            // 接收对端传回的配置信息
            size = sck.receive(buf.prepare(JSONSIZE));
            buf.commit(size);
            ReadJsonFromBuf(buf, json);
            std::vector<std::tuple<ip::tcp::endpoint, FileReader::ptr>> vec;
            vec.reserve(json.size());
            for (auto&& iter : json) {
                auto id   = iter[ID].asInt();
                auto port = iter[PORT].asInt();
                vec.emplace_back(ip::tcp::endpoint(ip, port), rvec.at(id));
            }
            auto task = std::make_shared<Task>(vec, name);
            mpool.submit(task);
        }
        catch (const std::exception& e) {
            spdlog::warn("send {} to {}: {}", fPath, ip.to_string(), e.what());
        }
    }

    void App::scan() {
        io_context        ioc;
        streambuf         buf;
        Json::Value       json;
        ip::udp::endpoint remote(ip::address_v4::broadcast(), UDPPORT);
        ip::udp::socket   sck(ioc, ip::udp::endpoint(ip::udp::v4(), 0));
        sck.set_option(socket_base::broadcast(true));
        json[TYPE] = SCAN;
        WriteJsonToBuf(buf, json);
        sck.send_to(buf.data(), remote);
    }

    void App::udplisten() {
        io_context        ioc;
        streambuf         buf;
        error_code        ec;
        ip::udp::endpoint remote;
        ip::udp::endpoint edp(ip::address_v4::any(), UDPPORT);
        ip::udp::socket   sck(ioc, edp);
        while (!mstop) {
            Json::Value json;
            auto        size = sck.receive_from(buf.prepare(JSONSIZE), remote);
            buf.commit(size);
            ReadJsonFromBuf(buf, json);
            auto str = json[TYPE].asString();
            if (str == SCAN) {
                // 响应扫描请求
                json[TYPE] = RESPONSE;
                WriteJsonToBuf(buf, json);
                buf.consume(sck.send_to(buf.data(), ip::udp::endpoint(remote.address(), UDPPORT)));
            }
            else if (str == RESPONSE) {
                spdlog::info("扫描到{}", remote.address().to_string());
            }
        }
    }
    void App::tcplisten() {
        io_context        ioc;
        streambuf         buf;
        ip::tcp::acceptor acp(ioc, ip::tcp::endpoint(ip::address_v4::any(), TCPPORT));
        while (true) {
            try {
                Json::Value json;
                auto        sck = acp.accept();
                // 回环地址意为关机信号
                if (sck.remote_endpoint().address().is_loopback()) {
                    break;
                }
                // 接收文件信息, 配置相应的数据结构
                auto size = sck.receive(buf.prepare(JSONSIZE));
                buf.commit(size);
                ReadJsonFromBuf(buf, json);
                auto totalsize = json[FILESIZE].asUInt64();
                auto name      = json[FILENAME].asString();
                spdlog::info("name:{} size:{}", name, totalsize);
                std::string dir = std::format("{}{}", name, DIR);
                spdlog::info("创建工作目录: ", dir);
                std::filesystem::create_directory(dir);
                auto wvec = FileWriter::Builder(THREAD_N, totalsize, name, dir);
                auto task = std::make_shared<Task>(wvec, name);
                mpool.submit(task);
                // 将FileWriter id对应的端口发送给对方
                auto        id_port = task->getPorts();
                Json::Value elem;
                Json::Value arr;
                for (auto&& [id, port] : id_port) {
                    elem[ID]   = id;
                    elem[PORT] = port;
                    arr.append(elem);
                }
                WriteJsonToBuf(buf, arr);
                write(sck, buf);
            }
            catch (const std::exception& e) {
                spdlog::warn(e.what());
            }
        }
    }
    void App::interpreter(const std::string& cmd) {
        if (cmd == "scan") {
            scan();
        }
    }
}  // namespace mtft
