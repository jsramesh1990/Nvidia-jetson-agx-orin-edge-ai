#pragma once
#include <string>
#include <functional>
#include <map>

namespace EdgeAI {

class APIServer {
public:
    APIServer(int port = 8000);
    ~APIServer();
    
    bool start();
    void stop();
    void setEndpoint(const std::string& path, 
                     std::function<std::string(const std::string&)> handler);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace EdgeAI
