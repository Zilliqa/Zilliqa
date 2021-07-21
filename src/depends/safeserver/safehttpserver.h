/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    httpserver.h
 * @date    31.12.2012
 * @author  Peter Spiess-Knafl <dev@spiessknafl.at>
 * @license See attached LICENSE.txt
 ************************************************************************/

#ifndef JSONRPC_CPP_SAFEHTTPSERVERCONNECTOR_H_
#define JSONRPC_CPP_SAFEHTTPSERVERCONNECTOR_H_

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(_MSC_FULL_VER) && !defined(_SSIZE_T_DEFINED)
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif // !_SSIZE_T_DEFINED */
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "jsonrpccpp/server/abstractserverconnector.h"
#include <map>
#include <microhttpd.h>

namespace jsonrpc {
/**
 * This class provides an embedded HTTP Server, based on libmicrohttpd, to
 * handle incoming Requests and send HTTP 1.1 valid responses. Note that this
 * class will always send HTTP-Status 200, even though an JSON-RPC Error might
 * have occurred. Please always check for the JSON-RPC Error Header.
 */
class SafeHttpServer : public AbstractServerConnector {
public:
  /**
   * @brief SafeHttpServer, constructor for the included SafeHttpServer
   * @param port on which the server is listening
   * @param enableSpecification - defines if the specification is returned in
   * case of a GET request
   * @param sslcert - defines the path to a SSL certificate, if this path is !=
   * "", then SSL/HTTPS is used with the given certificate.
   */
  SafeHttpServer(int port, bool useEpoll = true, const std::string &sslcert = "",
             const std::string &sslkey = "", int threads = 50);

  ~SafeHttpServer();

  //Bind to localhost only, deactivates TLS settings
  SafeHttpServer& BindLocalhost();

  virtual bool StartListening();
  virtual bool StopListening();

  bool virtual SendResponse(const std::string &response, void *addInfo = NULL);
  bool virtual SendOptionsResponse(void *addInfo);

  void SetUrlHandler(const std::string &url, IClientConnectionHandler *handler);

private:
  int port;
  int threads;
  bool running;
  bool useEpoll;
  std::string path_sslcert;
  std::string path_sslkey;
  std::string sslcert;
  std::string sslkey;

  struct MHD_Daemon *daemon;
  bool bindlocalhost;
  std::map<std::string, IClientConnectionHandler *> urlhandler;
  struct sockaddr_in loopback_addr;

  static int callback(void *cls, struct MHD_Connection *connection,
                      const char *url, const char *method, const char *version,
                      const char *upload_data, size_t *upload_data_size,
                      void **con_cls);

  IClientConnectionHandler *GetHandler(const std::string &url);
};

} /* namespace jsonrpc */
#endif /* JSONRPC_CPP_SAFEHTTPSERVERCONNECTOR_H_ */
