
#include "savegame_migration_buffer.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES


// Game headers
#include "Network/Network.h"
#include "SaveLoad/savegame_channel.h"


CSaveGameMigrationBuffer::~CSaveGameMigrationBuffer()
{
	FreeBuffer();
}


void CSaveGameMigrationBuffer::SetBufferSize(u32 BufferSize)
{
	if (BufferSize > m_MaxBufferSize)
	{
		savegameAssertf(0, "CSaveGameMigrationBuffer::SetBufferSize - new buffer size %d is larger than the expected maximum %d - let Graeme know", BufferSize, m_MaxBufferSize);
		m_MaxBufferSize = BufferSize;
	}
	m_BufferSize = BufferSize;
}


MemoryCardError CSaveGameMigrationBuffer::AllocateBuffer()
{
	if (m_pBufferMemory)
	{
		savegameAssertf(0, "CSaveGameMigrationBuffer::AllocateBuffer - buffer has already been allocated");
		return MEM_CARD_ERROR;
	}

	u32 BufferSize = m_MaxBufferSize;
	savegameDisplayf("CSaveGameMigrationBuffer::AllocateBuffer - Allocating %d bytes for save game file", BufferSize);

	if (BufferSize == 0)
	{
		savegameAssertf(0, "CSaveGameMigrationBuffer::AllocateBuffer - something has gone wrong. BufferSize is 0");
		return MEM_CARD_ERROR;
	}


	{
		if (!CNetwork::IsNetworkOpen())
		{
			savegameAssertf(BufferSize <= m_MaxBufferSize, "Single player savegame data is too large: %dKB. Memory doesn't grow on trees, you know...", BufferSize / 1024);

			// SP: Use network heap
			sysMemAutoUseNetworkMemory mem;
			m_pBufferMemory = (u8*) rage_aligned_new(16) u8[BufferSize];
			savegameAssertf(m_pBufferMemory, "Unable to allocate %u bytes for SaveGame (from network heap)!", BufferSize);
		}
		else
		{
			savegameErrorf("CSaveGameMigrationBuffer::AllocateBuffer - didn't expect the player to export a single player save game during MP");
			savegameAssertf(0, "CSaveGameMigrationBuffer::AllocateBuffer - didn't expect the player to export a single player save game during MP");
			return MEM_CARD_ERROR;
		}
	}

	if (m_pBufferMemory)
	{
		memset(m_pBufferMemory, 0, BufferSize);
		return MEM_CARD_COMPLETE;
	}
	else
	{
		savegameDisplayf("CSaveGameMigrationBuffer::AllocateBuffer - failed to allocate %d bytes so try again next frame", BufferSize);
		return MEM_CARD_BUSY;
	}
}


void CSaveGameMigrationBuffer::FreeBuffer()
{
	if (m_pBufferMemory)
	{
		if (CNetwork::GetNetworkHeap()->IsValidPointer(m_pBufferMemory))
		{		
			sysMemAutoUseNetworkMemory mem;
			delete [] m_pBufferMemory;
		}
		else
		{
// 			sysMemAutoUseFragCacheMemory mem;
// 			delete [] m_pBufferMemory;
			Quitf(0, "CSaveGameMigrationBuffer::FreeBuffer - memory was not allocated from the Network heap");
		}	

		m_pBufferMemory = NULL;
	}
}

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES
