#ifndef SAVEGAME_BUFFER_MP_H
#define SAVEGAME_BUFFER_MP_H

#include "SaveLoad/SavegameBuffers/savegame_buffer.h"

class CSaveGameBuffer_MultiplayerCharacter : public CSaveGameBuffer
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
	CSaveGameBuffer_MultiplayerCharacter(u32 MaxBufferSize);
	virtual ~CSaveGameBuffer_MultiplayerCharacter();

	virtual void Init(unsigned initMode);
	virtual void Shutdown(unsigned shutdownMode);

	virtual void FreeAllDataToBeSaved(bool bAssertIfDataHasntAlreadyBeenFreed = false);
};


class CSaveGameBuffer_MultiplayerCommon : public CSaveGameBuffer
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
	CSaveGameBuffer_MultiplayerCommon(u32 MaxBufferSize);
	virtual ~CSaveGameBuffer_MultiplayerCommon();

	virtual void Init(unsigned initMode);
	virtual void Shutdown(unsigned shutdownMode);

	virtual void FreeAllDataToBeSaved(bool bAssertIfDataHasntAlreadyBeenFreed = false);
};


#endif // SAVEGAME_BUFFER_MP_H


