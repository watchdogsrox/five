#ifndef SAVEGAME_MIGRATION_BUFFER_H
#define SAVEGAME_MIGRATION_BUFFER_H

// Game headers
#include "SaveLoad/savegame_defines.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

class CSaveGameMigrationBuffer
{
public:
	CSaveGameMigrationBuffer(u32 maxBufferSize) :
		m_pBufferMemory(NULL),
		m_BufferSize(0),
		m_MaxBufferSize(maxBufferSize)
	{
	}

	~CSaveGameMigrationBuffer();

	void SetBufferSize(u32 BufferSize);
	MemoryCardError AllocateBuffer();
	void FreeBuffer();

	u32 GetBufferSize() { return m_BufferSize; }
	u8 *GetBuffer() { return m_pBufferMemory; }

private:
	u8 *m_pBufferMemory;
	u32 m_BufferSize;
	u32 m_MaxBufferSize;
};

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_MIGRATION_BUFFER_H
