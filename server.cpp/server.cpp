#include "httplib.h"
#include <iostream>

using namespace httplib;

int main() {
    
Server svr;

svr.Get("/", [](const Request&, Response& res) {

    res.set_content("Server works!", "text/plain");

});

svr.Get("/api/test", [](const Request&, Response& res) {

    res.set_header("Access-Control-Allow-Origin", "*");

    res.set_content(
        R"({"message":"Hello API"})",
        "application/json"
    );

});

std::cout << "Server started!\n";

svr.listen("0.0.0.0", 8080);

return 0;

}