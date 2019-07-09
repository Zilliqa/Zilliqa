#include <iostream>
// #include "BCSimulator.h"
#include "ScillaIPCServer.h"
#include "libPersistence/ContractStorage2.h"
#include "libUtils/DataConversion.h"

using namespace jsonrpc;
using namespace std;

ScillaIPCServer::ScillaIPCServer(UnixDomainSocketServer &server, const dev::h160& address) : 
                                 AbstractServer<ScillaIPCServer>(server, JSONRPC_SERVER_V2) {

  this->bindAndAddMethod(Procedure("fetchStateValue", PARAMS_BY_NAME, JSON_BOOLEAN, 
                                   "query", JSON_STRING, "value", JSON_STRING, NULL), 
                                   &ScillaIPCServer::fetchStateValueI);

  this->bindAndAddMethod(Procedure("updateStateValue", jsonrpc::PARAMS_BY_NAME, 
                                   "query", JSON_STRING, "value", JSON_STRING, NULL), 
                                   &ScillaIPCServer::updateStateValueI);
  this->contractAddress = address;

}

void setContractAddress(const dev::h160& address) {
  this->contractAddress = address;
}

dev::h160 getContractAddress() {
  return this->contractAddress;
}

void ScillaIPCServer::updateStateValue(const string &query, const string &value) {
  return ContractStorage2::UpdateStateValue(this->contractAddress, DataConversion::StringToCharArray(query),
                                            0, DataConversion:: StringToCharArray(value), 0);
  // cout << "HELLO!!";
  // bc.updateStateValue(StringToCharArray(name), StringToCharArray(value));
}

bool ScillaIPCServer::fetchStateValue(const string &query, const string &value) {
  return ContractStorage2::FetchStateValue(this->contractAddress, DataConversion::StringToCharArray(query),
                                           0, DataConversion:: StringToCharArray(value), 0);
  // return true;
  // return bc.fetchStateValue(StringToCharArray(name), StringToCharArray(value));
}

int ScillaIPCServer::main() {
  try {
    UnixDomainSocketServer server("/Users/advaypal/Desktop/Repos/zilliqa/ipc-experiment/unixdomainsocketexample");
    ScillaIPCServer s(server);
    if (s.StartListening()) {
      cout << "Server started successfully" << endl;
      getchar();
      s.StopListening();
    } else {
      cout << "Error starting Server" << endl;
    }
  } catch (jsonrpc::JsonRpcException &e) {
    cerr << e.what() << endl;
  }
}