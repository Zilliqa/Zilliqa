/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/


#include <jsonrpccpp/server.h>

using namespace jsonrpc;
class Mediator;

class AbstractZServer : public jsonrpc::AbstractServer<AbstractZServer>
{

    public:
        AbstractZServer(jsonrpc::AbstractServerConnector &conn, jsonrpc::serverVersion_t type = jsonrpc::JSONRPC_SERVER_V2) : jsonrpc::AbstractServer<AbstractZServer>(conn, type)
        {
            this->bindAndAddMethod(jsonrpc::Procedure("getClientVersion", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,  NULL), &AbstractZServer::getClientVersionI);
            this->bindAndAddMethod(jsonrpc::Procedure("getNetworkId", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,  NULL), &AbstractZServer::getNetworkIdI);
            this->bindAndAddMethod(jsonrpc::Procedure("getProtocolVersion", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,  NULL), &AbstractZServer::getProtocolVersionI);
            this->bindAndAddMethod(jsonrpc::Procedure("createTransaction", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, "param01",jsonrpc::JSON_OBJECT, NULL), &AbstractZServer::createTransactionI);
            this->bindAndAddMethod(jsonrpc::Procedure("getTransaction", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param01",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getTransactionI);
            this->bindAndAddMethod(jsonrpc::Procedure("getDsBlock", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param01",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getDsBlockI);
            this->bindAndAddMethod(jsonrpc::Procedure("getTxBlock", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param01",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getTxBlockI);
            this->bindAndAddMethod(jsonrpc::Procedure("getLatestDsBlock", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,  NULL), &AbstractZServer::getLatestDsBlockI);
            this->bindAndAddMethod(jsonrpc::Procedure("getLatestTxBlock", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,  NULL), &AbstractZServer::getLatestTxBlockI);
            this->bindAndAddMethod(jsonrpc::Procedure("getBalance", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param01",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getBalanceI);
            this->bindAndAddMethod(jsonrpc::Procedure("getGasPrice", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,  NULL), &AbstractZServer::getGasPriceI);
            this->bindAndAddMethod(jsonrpc::Procedure("getStorageAt", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, "param01",jsonrpc::JSON_STRING,"param02",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getStorageAtI);
            this->bindAndAddMethod(jsonrpc::Procedure("getTransactionHistory", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY, "param01",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getTransactionHistoryI);
            this->bindAndAddMethod(jsonrpc::Procedure("getBlockTransactionCount", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, "param01",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getBlockTransactionCountI);
            this->bindAndAddMethod(jsonrpc::Procedure("getCode", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, "param01",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getCodeI);
            this->bindAndAddMethod(jsonrpc::Procedure("createMessage", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, "param01",jsonrpc::JSON_OBJECT, NULL), &AbstractZServer::createMessageI);
            this->bindAndAddMethod(jsonrpc::Procedure("getGasEstimate", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING, "param01",jsonrpc::JSON_OBJECT, NULL), &AbstractZServer::getGasEstimateI);
            this->bindAndAddMethod(jsonrpc::Procedure("getTransactionReceipt", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT, "param01",jsonrpc::JSON_STRING, NULL), &AbstractZServer::getTransactionReceiptI);
            this->bindAndAddMethod(jsonrpc::Procedure("isNodeSyncing", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_BOOLEAN,  NULL), &AbstractZServer::isNodeSyncingI);
            this->bindAndAddMethod(jsonrpc::Procedure("isNodeMining", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_BOOLEAN,  NULL), &AbstractZServer::isNodeMiningI);
            this->bindAndAddMethod(jsonrpc::Procedure("getHashrate", jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,  NULL), &AbstractZServer::getHashrateI);
        }

        inline virtual void getClientVersionI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->getClientVersion();
        }
        inline virtual void getNetworkIdI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->getNetworkId();
        }
        inline virtual void getProtocolVersionI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->getProtocolVersion();
        }
        inline virtual void createTransactionI(const Json::Value &request, Json::Value &response)
        {
            response = this->createTransaction(request[0u]);
        }
        inline virtual void getTransactionI(const Json::Value &request, Json::Value &response)
        {
            response = this->getTransaction(request[0u].asString());
        }
        inline virtual void getDsBlockI(const Json::Value &request, Json::Value &response)
        {
            response = this->getDsBlock(request[0u].asString());
        }
        inline virtual void getTxBlockI(const Json::Value &request, Json::Value &response)
        {
            response = this->getTxBlock(request[0u].asString());
        }
        inline virtual void getLatestDsBlockI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->getLatestDsBlock();
        }
        inline virtual void getLatestTxBlockI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->getLatestTxBlock();
        }
        inline virtual void getBalanceI(const Json::Value &request, Json::Value &response)
        {
            response = this->getBalance(request[0u].asString());
        }
        inline virtual void getGasPriceI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->getGasPrice();
        }
        inline virtual void getStorageAtI(const Json::Value &request, Json::Value &response)
        {
            response = this->getStorageAt(request[0u].asString(), request[1u].asString());
        }
        inline virtual void getTransactionHistoryI(const Json::Value &request, Json::Value &response)
        {
            response = this->getTransactionHistory(request[0u].asString());
        }
        inline virtual void getBlockTransactionCountI(const Json::Value &request, Json::Value &response)
        {
            response = this->getBlockTransactionCount(request[0u].asString());
        }
        inline virtual void getCodeI(const Json::Value &request, Json::Value &response)
        {
            response = this->getCode(request[0u].asString());
        }
        inline virtual void createMessageI(const Json::Value &request, Json::Value &response)
        {
            response = this->createMessage(request[0u]);
        }
        inline virtual void getGasEstimateI(const Json::Value &request, Json::Value &response)
        {
            response = this->getGasEstimate(request[0u]);
        }
        inline virtual void getTransactionReceiptI(const Json::Value &request, Json::Value &response)
        {
            response = this->getTransactionReceipt(request[0u].asString());
        }
        inline virtual void isNodeSyncingI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->isNodeSyncing();
        }
        inline virtual void isNodeMiningI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->isNodeMining();
        }
        inline virtual void getHashrateI(const Json::Value &request, Json::Value &response)
        {
            (void)request;
            response = this->getHashrate();
        }
        virtual std::string getClientVersion() = 0;
        virtual std::string getNetworkId() = 0;
        virtual std::string getProtocolVersion() = 0;
        virtual std::string createTransaction(const Json::Value& param01) = 0;
        virtual Json::Value getTransaction(const std::string& param01) = 0;
        virtual Json::Value getDsBlock(const std::string& param01) = 0;
        virtual Json::Value getTxBlock(const std::string& param01) = 0;
        virtual Json::Value getLatestDsBlock() = 0;
        virtual Json::Value getLatestTxBlock() = 0;
        virtual Json::Value getBalance(const std::string& param01) = 0;
        virtual std::string getGasPrice() = 0;
        virtual std::string getStorageAt(const std::string& param01, const std::string& param02) = 0;
        virtual Json::Value getTransactionHistory(const std::string& param01) = 0;
        virtual std::string getBlockTransactionCount(const std::string& param01) = 0;
        virtual std::string getCode(const std::string& param01) = 0;
        virtual std::string createMessage(const Json::Value& param01) = 0;
        virtual std::string getGasEstimate(const Json::Value& param01) = 0;
        virtual Json::Value getTransactionReceipt(const std::string& param01) = 0;
        virtual bool isNodeSyncing() = 0;
        virtual bool isNodeMining() = 0;
        virtual std::string getHashrate() = 0;
};

class Server: public AbstractZServer
{
    Mediator & m_mediator;
    public:
        Server(Mediator &mediator);
        ~Server();

        virtual std::string getClientVersion();
        virtual std::string getNetworkId();
        virtual std::string getProtocolVersion();
        virtual std::string createTransaction(const Json::Value & _json);
        virtual Json::Value getTransaction(const std::string & transactionHash);
        virtual Json::Value getDsBlock(const std::string & blockHash);
        virtual Json::Value getTxBlock(const std::string & blockHash);
        virtual Json::Value getLatestDsBlock();
        virtual Json::Value getLatestTxBlock();
        virtual Json::Value getBalance(const std::string & address);
        virtual std::string getGasPrice();
        virtual std::string getStorageAt(const std::string & address, const std::string & position);
        virtual Json::Value getTransactionHistory(const std::string & address);
        virtual std::string getBlockTransactionCount(const std::string & blockHash);
        virtual std::string getCode(const std::string & address);
        virtual std::string createMessage(const Json::Value& _json);
        virtual std::string getGasEstimate(const Json::Value& _json);
        virtual Json::Value getTransactionReceipt(const std::string & transactionHash);
        virtual bool isNodeSyncing();
        virtual bool isNodeMining();
        virtual std::string getHashrate();
};
