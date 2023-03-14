find_package(Libevent REQUIRED)

add_library(Network
    Peer.cpp
    P2PComm.cpp
    Guard.cpp
    Blacklist.cpp
    ReputationManager.cpp
    RumorManager.cpp
    DataSender.cpp
    SendJobs.cpp
    P2PMessage.cpp)

target_include_directories(Network PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(Network PUBLIC
    Constants
    libevent::core
    libevent::pthreads
    RumorSpreading
    Utils
    OpenSSL::Crypto)

