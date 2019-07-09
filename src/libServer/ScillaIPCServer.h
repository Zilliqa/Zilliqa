#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/unixdomainsocketserver.h>
class ScillaIPCServer : jsonrpc::AbstractServer<ScillaIPCServer> {
private:
  // BCSimulator bc;
public:
  ScillaIPCServer(jsonrpc::UnixDomainSocketServer &server);

  inline void fetchStateValueI(const Json::Value &request, Json::Value &response) {
    response = this->fetchStateValue(request["query"].asString(), request["value"].asString());
  }

  inline void updateStateValueI(const Json::Value &request, Json::Value &response) {
    this->updateStateValue(request["query"].asString(), request["value"].asString());
  }
  bool fetchStateValue(const std::string& query, const std::string& value);
  void updateStateValue(const std::string& query, const std::string& value);
  int main();
};