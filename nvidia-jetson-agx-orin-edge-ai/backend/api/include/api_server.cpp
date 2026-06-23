#include "api_server.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

namespace EdgeAI {

class APIServer::Impl {
public:
    Impl(int port) : port_(port) {}
    
    bool start() {
        server_.Get("/api/health", [](const httplib::Request& req, 
                                      httplib::Response& res) {
            json response = {{"status", "ok"}, {"timestamp", time(nullptr)}};
            res.set_content(response.dump(), "application/json");
        });
        
        server_.Post("/api/attendance", [](const httplib::Request& req,
                                          httplib::Response& res) {
            auto data = json::parse(req.body);
            // Process attendance
            json response = {{"status", "success"}, {"message", "Attendance recorded"}};
            res.set_content(response.dump(), "application/json");
        });
        
        server_.Get("/api/employees", [](const httplib::Request& req,
                                        httplib::Response& res) {
            json employees = json::array();
            // Fetch from database
            employees.push_back({{"id", "EMP001"}, {"name", "John Doe"}});
            res.set_content(employees.dump(), "application/json");
        });
        
        server_.listen("0.0.0.0", port_);
        return true;
    }
    
    void stop() {
        server_.stop();
    }
    
private:
    httplib::Server server_;
    int port_;
};

APIServer::APIServer(int port) 
    : pImpl(std::make_unique<Impl>(port)) {}

APIServer::~APIServer() = default;

bool APIServer::start() {
    return pImpl->start();
}

void APIServer::stop() {
    pImpl->stop();
}

} // namespace EdgeAI
