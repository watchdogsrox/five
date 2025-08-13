#ifndef SAVEGAME_BUFFER_H
#define SAVEGAME_BUFFER_H

// Rage headers
#include "parser/tree.h"

// Game headers
#include "SaveLoad/SavegameRtStructures/savegame_rtstructure.h"

enum SaveBlockDataStatus
{
	SAVEBLOCKDATA_IDLE,
	SAVEBLOCKDATA_PENDING,
	SAVEBLOCKDATA_ERROR,
	SAVEBLOCKDATA_SUCCESS
};

//LZ4 compression defines
#define SAVEGAMEHEADER_V2                     "SGV2"
#define SAVEGAMEHEADER_V2_ZLIB                "SGVZ"
#define SAVEGAMEHEADER_SIZE_V2                4
#define SAVEGAMEHEADER_SIZE_UNCOMPRESSEDDATA  4
#define SAVEGAMEHEADER_SIZE                   (SAVEGAMEHEADER_SIZE_V2+SAVEGAMEHEADER_SIZE_UNCOMPRESSEDDATA)

#define LZ4_COMPRESSION					  RSG_ORBIS || RSG_DURANGO || RSG_PC


class CSaveGameBuffer
{
	friend class CScriptSavePsoFileTracker;	//	so that it can access FreeSavegameMemoryBuffer()

private:
	//	Private data
	u32 m_MaxBufferSize;

	bool m_bBufferContainsScriptData;

	eTypeOfSavegame m_TypeOfSavegame;

#if SAVEGAME_USES_XML
	parTree *m_pLoadedParTree;	//	For Single Player, has to remain in memory after a load so that its nametable is available to the parTreeNodes in CScriptSaveXmlTrees
	//	For Multiplayer stats, we can delete the whole tree after it has been read because there is no script data.
#endif

	//	Private functions
#if SAVEGAME_USES_XML
	parRTStructure *CreateRTStructure();
#endif

#if SAVEGAME_USES_PSO
	psoBuilderInstance* CreatePsoStructure();
#endif

	static void FreeSavegameMemoryBuffer(u8 *pBuffer);

	//	Private virtual functions
#if SAVEGAME_USES_PSO
	virtual void ReadDataFromPsoFile(psoFile* psofile) = 0;
#endif

#if SAVEGAME_USES_XML
	virtual bool ReadDataFromXmlFile(fiStream* pReadStream) = 0;

	virtual void AfterReadingFromXmlFile() = 0;
#endif
	//	End of private virtual functions

protected:

	u8 *m_BufferContainingContentsOfSavegameFile;

	//	Maybe try to change these members from protected to private
#if SAVEGAME_USES_XML
	parTree *m_pParTreeToBeSaved;	//	created in CreateSaveDataAndCalculateSize and deleted after saving in SaveBlockData
#endif

#if SAVEGAME_USES_PSO
	psoBuilder* m_pPsoBuilderToBeSaved;
#endif

	CRTStructureDataToBeSaved *m_pRTStructureDataToBeSaved;

	u32 m_BufferSize;

	SaveBlockDataStatus m_SaveBlockDataStatus;
	bool m_UseEncryptionForSaveBlockData;

	// Protected functions
#if SAVEGAME_USES_XML
	bool LoadXmlTreeFromStream(fiStream *pStream, parRTStructure &structure);

	void DeleteRootNodeOfTree();
	void DeleteEntireTree();
#endif

#if SAVEGAME_USES_PSO
	template<typename _Type> void LoadSingleObject(const char* name, psoStruct& root);
#endif // SAVEGAME_USES_PSO

public:
	//	Public functions
	CSaveGameBuffer(eTypeOfSavegame typeOfSavegame, u32 MaxBufferSize, bool bContainsScriptData);
	virtual ~CSaveGameBuffer();

	//	Public virtual functions
	virtual void Init(unsigned initMode);
	virtual void Shutdown(unsigned shutdownMode);

	virtual void FreeAllDataToBeSaved(bool bAssertIfDataHasntAlreadyBeenFreed = false);
	//	End of public virtual functions

	bool CompressData(u8* source, u32 sourceSize);
	bool DecompressData(u8** source, u8* dest, u32 destinationSize);

	bool LoadBlockData(const bool useEncryption);

	MemoryCardError	CreateSaveDataAndCalculateSize();

	void FreeBufferContainingContentsOfSavegameFile();

	// Kicks off a thread that runs the SaveBlockData operation
	bool BeginSaveBlockData(const bool useEncryption);

	// This should only be called by the worker thread -- game code should call BeginSaveBlockData to initiate
	// the operation
	bool DoSaveBlockData();

	void EndSaveBlockData();

	SaveBlockDataStatus GetSaveBlockDataStatus() const { return m_SaveBlockDataStatus; }
	void SetSaveBlockDataStatus(SaveBlockDataStatus status) { m_SaveBlockDataStatus = status; }

	MemoryCardError	AllocateDataBuffer();

	u8 *GetBuffer() { return m_BufferContainingContentsOfSavegameFile; }
	void SetBuffer(u8* buffer) { m_BufferContainingContentsOfSavegameFile = buffer; }

	u32 GetBufferSize() { return m_BufferSize; }
	void SetBufferSize(u32 BufferSize);

	void ReleaseBuffer() { m_BufferContainingContentsOfSavegameFile = NULL; }

	u32 GetMaxBufferSize() { return m_MaxBufferSize; }	//	For Free space checks

};


#if SAVEGAME_USES_PSO
template<typename _Type> void CSaveGameBuffer::LoadSingleObject(const char* name, psoStruct& root)
{
	psoMember mem = root.GetMemberByName(atLiteralHashValue(name));
	if (!mem.IsValid())
	{
		return;
	}

	psoStruct str = mem.GetSubStructure();
	if (!str.IsValid())
	{
		return;
	}

	_Type obj;
	psoLoadObject(str, obj);
	obj.PostLoad();
}
#endif // SAVEGAME_USES_PSO


#endif // SAVEGAME_BUFFER_H


