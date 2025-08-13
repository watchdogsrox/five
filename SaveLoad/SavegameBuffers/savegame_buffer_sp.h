#ifndef SAVEGAME_BUFFER_SP_H
#define SAVEGAME_BUFFER_SP_H

#include "SaveLoad/SavegameBuffers/savegame_buffer.h"

class CSaveGameBuffer_SinglePlayer : public CSaveGameBuffer
{
private:

#if SAVEGAME_USES_PSO
	virtual void ReadDataFromPsoFile(psoFile* psofile);
#endif

#if SAVEGAME_USES_XML
	virtual bool ReadDataFromXmlFile(fiStream* pReadStream);

	virtual void AfterReadingFromXmlFile();
#endif

public:
	//	Public functions
	CSaveGameBuffer_SinglePlayer(u32 MaxBufferSize);
	virtual ~CSaveGameBuffer_SinglePlayer();

	virtual void Init(unsigned initMode);
	virtual void Shutdown(unsigned shutdownMode);

	virtual void FreeAllDataToBeSaved(bool bAssertIfDataHasntAlreadyBeenFreed = false);
};


#endif // SAVEGAME_BUFFER_SP_H


