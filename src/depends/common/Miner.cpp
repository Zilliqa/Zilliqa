#include "Miner.h"
//#include "EthashAux.h"

using namespace dev;
using namespace eth;

unsigned dev::eth::Miner::s_dagLoadMode = 0;

unsigned dev::eth::Miner::s_dagLoadIndex = 0;

unsigned dev::eth::Miner::s_dagCreateDevice = 0;

uint8_t* dev::eth::Miner::s_dagInHostMemory = nullptr;

bool dev::eth::Miner::s_exit = false;

bool dev::eth::Miner::s_noeval = true;

std::stringstream dev::eth::Miner::s_ssLog;
std::stringstream dev::eth::Miner::s_ssNote;
std::stringstream dev::eth::Miner::s_ssWarn;
