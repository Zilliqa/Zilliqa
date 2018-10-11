


#include "libData/BlockData/Block.h"
#include "libData/DataStructures/CircularArray.h"

typedef std::tuple<uint64_t,uint64_t,BlockType,BlockHash> BlockLink;


enum BlockLinkIndex : unsigned char
	{	
		INDEX = 0,
		DSINDEX = 1,
		BLOCKTYPE = 2,
		BLOCKHASH = 3,
	};


class BlockLinkChain
{
	CircularArray<BlockLink> m_blocklinkchain;
	std::mutex m_mutexBlockLinkChain;

public:

	BlockLink GetBlockLink( [[gnu::unused]] const uint64_t& blocknum) 
	{
		std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);
		/*if(m_blocklinkchain.size() <= blocknum)
		{
			return BlockLink();
		}
		else if(blocknum + m_blocklinkchain.capacity() < m_blocklinkchain.size())
		{
			return GetFromPersistanceStorage(blocknum);
		}
		if(std::get<BlockLinkIndex::INDEX>(m_blocklinkchain[blocknum]) != blocknum)
		{
			LOG_GENERAL(WARNING, "Does not match the given blocknum");
			return BlockLink();
		}
		return m_blocklinkchain[blocknum];*/
		return BlockLink();
	}

	void AddBlockLink(const uint64_t& index,const uint64_t& dsindex, const BlockType blocktype, const BlockHash& blockhash)
	{
		std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);
		m_blocklinkchain.insert_new(index,std::make_tuple(index,dsindex,blocktype,blockhash));
	}

	uint64_t GetLatestIndex()
	{
		std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);
		if(m_blocklinkchain.size() == 0)
		{
			return 0;
		}
		return std::get<BlockLinkIndex::INDEX>(m_blocklinkchain.back());
	}
};