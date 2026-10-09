#pragma once
#include <functional>
#include <string>
#include <thread>
namespace qualpf::infrastructure {
enum class ConnectionAction { opened, accessed, removed };
struct ConnectionEvent {
    ConnectionAction action;
    std::string name, role;
    std::thread::id thread;
};
using ConnectionTrace = std::function<void(const ConnectionEvent &)>;
} // namespace qualpf::infrastructure
