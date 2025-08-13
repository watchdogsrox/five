
#include "savegame_buffer.h"


// Rage headers
#include "parser/manager.h"
#include "zlib/lz4.h"
#include "zlib/zlib.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_users_and_devices.h"
#include "SaveLoad/SavegameScriptData/script_save_data.h"
#include "script/script.h"



PARAM(noencryptsave, "Don't encrypt save game files");
PARAM(nocompressionsave, "Don't try to compress the savegame files when saving/loading");

#if !__FINAL
PARAM(dont_save_script_data_to_mp_stats, "Skip the saving of any registered MP script save data to the savegame file for the current character");
PARAM(dont_load_script_data_from_mp_stats, "When the savegame file for an MP character is loaded, any script data within that file will be ignored");
#endif	//	!__FINAL

#if __WIN32PC || RSG_DURANGO
XPARAM(showProfileBypass);
#endif // __WIN32PC || RSG_DURANGO

#if __BANK
bool g_encryptOnSave = true;
#endif

CompileTimeAssert(SAVEGAME_USES_XML || SAVEGAME_USES_PSO); // You have to have at least one of these




// ****************************** CSaveGameBuffer ****************************************************

//	This is probably higher than it needs to be. When loading an XML format PS3 save, CSaveGameBuffer::SetBufferSize() gets
//	called with a size that includes the size of the save icon and the PS3 system files.
//	u32 CSaveGameBuffer::ms_MaxBufferSize = 400000;

CSaveGameBuffer::CSaveGameBuffer(eTypeOfSavegame typeOfSavegame, u32 MaxBufferSize, bool bContainsScriptData)
{
	m_BufferContainingContentsOfSavegameFile = NULL;
#if SAVEGAME_USES_XML
	m_pParTreeToBeSaved = NULL;
#endif
#if SAVEGAME_USES_PSO
	m_pPsoBuilderToBeSaved = NULL;
#endif
	m_BufferSize = 0;

	m_TypeOfSavegame = typeOfSavegame;
	m_MaxBufferSize = MaxBufferSize;
	m_bBufferContainsScriptData = bContainsScriptData;

	m_pRTStructureDataToBeSaved = NULL;

	m_SaveBlockDataStatus = SAVEBLOCKDATA_IDLE;
}

CSaveGameBuffer::~CSaveGameBuffer()
{
	FreeBufferContainingContentsOfSavegameFile();
	FreeAllDataToBeSaved();

	delete m_pRTStructureDataToBeSaved;
	m_pRTStructureDataToBeSaved = NULL;
}

void CSaveGameBuffer::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
#if SAVEGAME_USES_XML
		m_pLoadedParTree = NULL;
#endif
	}
}

void CSaveGameBuffer::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
#if SAVEGAME_USES_XML
	if (m_pLoadedParTree)
	{
		//	LoadTree allocates from the temp allocator but I want to keep the nametable for this tree in memory for the duration of the game
		//	Could that cause problems?
		savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameBuffer::Shutdown - Graeme - I'm not dealing with AutoUseTempMemory properly");

		parManager::AutoUseTempMemory temp;

		delete m_pLoadedParTree;	//	make sure to free any memory used for a previously-loaded parTree
		m_pLoadedParTree = NULL;
	}
#endif // SAVEGAME_USES_XML
}

void CSaveGameBuffer::FreeAllDataToBeSaved(bool ASSERT_ONLY(bAssertIfDataHasntAlreadyBeenFreed))
{
#if SAVEGAME_USES_XML
	if (m_pParTreeToBeSaved)
	{
		savegameAssertf(!bAssertIfDataHasntAlreadyBeenFreed, "CSaveGameBuffer::FreeAllDataToBeSaved - expected m_pParTreeToBeSaved to already have been freed at this stage");
		savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameBuffer::FreeAllDataToBeSaved - Graeme - I'm not dealing with AutoUseTempMemory properly");
		parManager::AutoUseTempMemory temp;

		delete m_pParTreeToBeSaved;
		m_pParTreeToBeSaved = NULL;
	}

	if (savegameVerifyf(m_pRTStructureDataToBeSaved, "CSaveGameBuffer::FreeAllDataToBeSaved - m_pRTStructureDataToBeSaved has not been created"))
	{
		m_pRTStructureDataToBeSaved->DeleteAllRTStructureData();
	}

	//	Moved to CSaveGameBuffer_SinglePlayer::FreeAllDataToBeSaved
	//	CScriptSaveData::DeleteAllRTStructures();
#endif // SAVEGAME_USES_XML

#if SAVEGAME_USES_PSO
	if (savegameVerifyf(m_pRTStructureDataToBeSaved, "CSaveGameBuffer::FreeAllDataToBeSaved - m_pRTStructureDataToBeSaved has not been created"))
	{
		m_pRTStructureDataToBeSaved->DeleteAllRTStructureData();
	}

	if (m_pPsoBuilderToBeSaved)
	{
		savegameAssertf(!bAssertIfDataHasntAlreadyBeenFreed, "CSaveGameBuffer::FreeAllDataToBeSaved - expected m_pPsoBuilderToBeSaved to already have been freed at this stage");
		delete m_pPsoBuilderToBeSaved;
		m_pPsoBuilderToBeSaved = NULL;
	}
#endif
}


#if SAVEGAME_USES_XML
parRTStructure *CSaveGameBuffer::CreateRTStructure()
{
	if (savegameVerifyf(m_pRTStructureDataToBeSaved, "CSaveGameBuffer::CreateRTStructure - m_pRTStructureDataToBeSaved has not been created"))
	{
		return m_pRTStructureDataToBeSaved->CreateRTStructure();
	}

	return NULL;
}
#endif	//	SAVEGAME_USES_XML

#if SAVEGAME_USES_PSO
psoBuilderInstance* CSaveGameBuffer::CreatePsoStructure()
{
	if (savegameVerifyf(m_pRTStructureDataToBeSaved, "CSaveGameBuffer::CreatePsoStructure - m_pRTStructureDataToBeSaved has not been created"))
	{
		m_pPsoBuilderToBeSaved = rage_new psoBuilder;

		m_pPsoBuilderToBeSaved->IncludeChecksum();

		return m_pRTStructureDataToBeSaved->CreatePsoStructure(*m_pPsoBuilderToBeSaved);
	}

	return NULL;
}
#endif	//	SAVEGAME_USES_PSO

bool CSaveGameBuffer::CompressData(u8* source, u32 sourceSize)
{
	bool result = true;

	if (PARAM_nocompressionsave.Get())
		return result;

	//compress savegame
	int compSize = 0;

#if LZ4_COMPRESSION
	u32 destSize = LZ4_compressBound(sourceSize)+SAVEGAMEHEADER_SIZE;	//make sure we have enough space for worst case scenario + 4 byte header
	u32 fallbackDestSize = destSize;									// we don't have a 'better' worst case estimate
#else
	u32 destSize = GetBufferSize()*2;													// original very approximate - make sure we have enough space for worst case scenario
	u32 fallbackDestSize = SAVEGAMEHEADER_SIZE + deflateBound(NULL, GetBufferSize());	// this is a much tighter upper bound, in case of failure of first attempt
#endif

	u8 *dest = nullptr;
	{
#if ENABLE_CHUNKY_ALLOCATOR
		ChunkyWrapper a(*sysMemManager::GetInstance().GetChunkyAllocator(), true, "Savegame");
#endif // ENABLE_CHUNKY_ALLOCATOR
		dest = rage_new u8[destSize];
		if (dest == NULL)
		{
			dest = rage_new u8[fallbackDestSize];
			destSize = fallbackDestSize;
		}
	}

	if(dest)
	{

#if LZ4_COMPRESSION	//lz4 used on next gen
		compSize = LZ4_compress(reinterpret_cast<const char*>(source), reinterpret_cast<char*>(dest), sourceSize);
#else	//zlib used on last gen
		z_stream c_stream;
		memset(&c_stream,0,sizeof(c_stream));
		if (deflateInit2(&c_stream,Z_BEST_COMPRESSION,Z_DEFLATED,-MAX_WBITS,MAX_MEM_LEVEL,Z_DEFAULT_STRATEGY) < 0)
			Quitf("Error in savegame deflateInit");
		MEM_USE_USERDATA(MEMUSERDATA_SAVEGAME);
		c_stream.next_in = source;
		c_stream.avail_in = sourceSize;
		c_stream.next_out = dest;
		c_stream.avail_out = destSize;
		if (deflate(&c_stream, Z_FINISH) != Z_STREAM_END)
			Quitf("Error in savegame deflate");
		deflateEnd(&c_stream);
		compSize = (int) (destSize - c_stream.avail_out);
#endif

		if (0 < compSize)
		{
			sysMemSet(source, 0, sourceSize); //Make sure the buffer is zero-ed, no funny business.
#if LZ4_COMPRESSION
			memcpy(source, SAVEGAMEHEADER_V2, SAVEGAMEHEADER_SIZE_V2);                                //Write header ID.
#else
			memcpy(source, SAVEGAMEHEADER_V2_ZLIB, SAVEGAMEHEADER_SIZE_V2);                           //Write header ID.
#endif

			u32 sourceSizeL = sysEndian::NtoL(sourceSize);

			memcpy(source+SAVEGAMEHEADER_SIZE_V2, &sourceSizeL, SAVEGAMEHEADER_SIZE_UNCOMPRESSEDDATA); //Write header Size of uncompressed data.
			memcpy(source+SAVEGAMEHEADER_SIZE, dest, compSize);                                       //Write data after header.
			SetBufferSize(compSize+SAVEGAMEHEADER_SIZE);                                              //Size is header + compressed data size.
		}
		//0 if the compression fails
		else
		{
			savegameErrorf("CSaveGameBuffer::CompressData() FAILED");
			result = false;
		}

#if ENABLE_CHUNKY_ALLOCATOR
		ChunkyWrapper a(*sysMemManager::GetInstance().GetChunkyAllocator(), false, "Savegame");
#endif // ENABLE_CHUNKY_ALLOCATOR
		delete[] dest;
	}

	return result;
}

bool CSaveGameBuffer::DecompressData(u8** source, u8* dest, u32 destinationSize)
{
	bool result = true;

	if (PARAM_nocompressionsave.Get())
		return result;

	//distinguish between the two types of compression we can use
	bool hasLZ4Header = !strncmp((reinterpret_cast<const char*>(*source)), SAVEGAMEHEADER_V2, SAVEGAMEHEADER_SIZE_V2);
	bool hasZlibHeader = !strncmp((reinterpret_cast<const char*>(*source)), SAVEGAMEHEADER_V2_ZLIB, SAVEGAMEHEADER_SIZE_V2);
	u8* saveDataStart = hasLZ4Header||hasZlibHeader ? *source + SAVEGAMEHEADER_SIZE : *source;

	if (m_TypeOfSavegame == SAVEGAME_MULTIPLAYER_CHARACTER && (hasLZ4Header||hasZlibHeader))
	{
		//decompress savegame
		if(savegameVerifyf(dest != NULL, "NULL destination buffer."))
		{
			u32 uncompressedSize = destinationSize;
			int returnValue = 0;

			u32 sourceSizeL = 0;
			memcpy(&sourceSizeL, *source+SAVEGAMEHEADER_SIZE_V2, SAVEGAMEHEADER_SIZE_UNCOMPRESSEDDATA);	//Get the size of the data uncompressed
			uncompressedSize = sysEndian::LtoN(sourceSizeL);

			//Hack for previous saves where the size was in Little Endian... DO WE CARE???
			// WE ONLY NEED THIS IN DEV FOR MAYBE UNTIL NEXT YEAR WHEN THE BUILDS HAVE SETTLED...
#if !__FINAL
			if(uncompressedSize > destinationSize)
				uncompressedSize = sourceSizeL;
#endif // !__FINAL

			savegameAssertf(uncompressedSize <= destinationSize, "Size of the destination buffer '%u' is not big enough for the uncompressed data '%u'", destinationSize, uncompressedSize);
			if(uncompressedSize > destinationSize)
			{
				savegameErrorf("Size of the destination buffer '%u' is not big enough for the uncompressed data '%u'", destinationSize, uncompressedSize);
				return false;
			}

			if(hasLZ4Header)
			{
				returnValue = LZ4_decompress_fast((const char*) saveDataStart, (char*) dest, uncompressedSize);
			}
			else if(hasZlibHeader)
			{
				z_stream c_stream;
				memset(&c_stream,0,sizeof(c_stream));
				if (inflateInit2(&c_stream,-MAX_WBITS) < 0)
				{
					savegameErrorf("Savegame InflateInit failed.");
					inflateEnd(&c_stream);
					return false;
				}
				c_stream.next_in = saveDataStart;
				c_stream.avail_in = GetBufferSize()-SAVEGAMEHEADER_SIZE;
				c_stream.next_out = dest;
				c_stream.avail_out = destinationSize;
				if (inflate(&c_stream, Z_FINISH) < 0)
				{
					savegameErrorf("Failed to inflate savegame data.");
					inflateEnd(&c_stream);
					return false;
				}
				inflateEnd(&c_stream);
				returnValue = (int) (destinationSize - c_stream.avail_out); //TOTAL - REMAINING
				uncompressedSize = returnValue; //Thats the total size Uncompressed.
			}

			if(returnValue<=0)
			{
				savegameErrorf("CSaveGameBuffer::DecompressData() FAILED");
				result = false;
			}
			else
			{
				SetBuffer(dest);
				SetBufferSize(uncompressedSize);
			}
		}
	}

	return result;
}

bool CSaveGameBuffer::LoadBlockData(const bool useEncryption)
{
#if __BANK
	bool bCheckPlayerID = true;
	if (CGenericGameStorage::ms_bLoadFromPC)
	{
		bCheckPlayerID = false;
	}
	CGenericGameStorage::ms_bLoadFromPC = false;
#endif	//	__BANK

#if __WIN32PC || RSG_DURANGO
	// We do not yet support user accounts/live/social club etc. so we are faking it for now.
	Assertf(!PARAM_showProfileBypass.Get(), "User Profiles is not yet supported on PC. This WILL need update once Social Club is added and the valid user check must NOT be skipped! For now we will not check player id!");

#if !__BANK
	// TODO: Once Social Club has been added remove the skip player check from PC and Durango versions.
	bool
#endif // !__BANK
		bCheckPlayerID = false;
#endif //  __WIN32PC || RSG_DURANGO

#if __BANK || __WIN32PC || RSG_DURANGO
	// TODO: Once Social Club has been added remove the skip player check from PC and Durango versions.
	if (bCheckPlayerID)
#endif	//	__BANK || __WIN32PC || RSG_DURANGO
	{
		if (CSavegameUsers::HasPlayerJustSignedOut())
		{
			savegameDisplayf("CSaveGameBuffer::LoadBlockData - Player has just signed out");
			return false;
		}

		{	//	If this game data has been loaded from a storage device then check that the player has not
			//	signed out and then signed back in using a different profile
			if (!NetworkInterface::GetActiveGamerInfo())
			{
				CSavegameUsers::ClearUniqueIDOfGamerWhoStartedTheLoad();
				savegameDisplayf("CSaveGameBuffer::LoadBlockData - Player is signed out");
				return false;
			}
			else if (NetworkInterface::GetActiveGamerInfo()->GetGamerId() != CSavegameUsers::GetUniqueIDOfGamerWhoStartedTheLoad())
			{
				savegameDisplayf("CSaveGameBuffer::LoadBlockData - Player has signed out and signed in using a different profile since the load started");
				return false;
			}
		}
	}

#if __BANK
	if (PARAM_noencryptsave.Get())
	{
		CGenericGameStorage::ms_bDecryptOnLoading = false;
	}

	if (CGenericGameStorage::ms_bDecryptOnLoading)
#endif
	{
		if (useEncryption)
		{
			// decrypt the buffer
			AES aes(AES_KEY_ID_SAVEGAME);
			aes.Decrypt(GetBuffer(), GetBufferSize());
		}
	}

	u8* buffer     = GetBuffer();
	u32 bufferSize = GetBufferSize();

#if SAVEGAME_USES_PSO
	u32 destSize = m_MaxBufferSize;
	u8 *uncompressed = rage_new u8[destSize];
	if (!savegameVerify(uncompressed))
		return false;

	if(!DecompressData(&buffer, uncompressed, destSize))
	{
		delete[] uncompressed;
		return false; // Decompression for LZ4 failed
	}

	//Update the buffer pointer and size because it might have changed during decompression.
	buffer = GetBuffer();
	bufferSize = GetBufferSize();
#endif // SAVEGAME_USES_PSO

	char memFileName[RAGE_MAX_PATH];
	fiDevice::MakeMemoryFileName(memFileName, RAGE_MAX_PATH, buffer, bufferSize, false, "");

	const int inputFormat = PARSER.FindInputFormat(*(reinterpret_cast<u32*>(buffer)));
	if (inputFormat == parManager::INVALID)
	{
		savegameErrorf("CSaveGameBuffer::LoadBlockData - PARSER.FindInputFormat()='parManager::INVALID'. memFileName='%s'", memFileName);
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FILE_CORRUPT);
		USES_PSO_ONLY( delete[] uncompressed; )
			return false;
	}
#if SAVEGAME_USES_PSO
	else if (inputFormat == parManager::PSO) 
	{
		savegameDebugf1("CSaveGameBuffer::LoadBlockData - Loading PSO-formatted save file");
		psoFile* psofile = NULL;

		psoLoadFileStatus result;
		psofile = psoLoadFile(memFileName, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::REQUIRE_CHECKSUM), &result);

		if (psofile)
		{
			ReadDataFromPsoFile(psofile);
			delete[] uncompressed;
			return true;
		}
		else
		{
			savegameErrorf("CSaveGameBuffer::LoadBlockData - Couldn't load PSO save file %s", memFileName);

			//To avoid interfering with cloud loads, local loads call AllowReasonForFailureToBeSet(false) so that the following line has no effect
			CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FILE_CORRUPT);
			delete[] uncompressed;
			return false;
		}
	}
#endif // SAVEGAME_USES_PSO
#if SAVEGAME_USES_XML
	else if (inputFormat == parManager::XML) 
	{
		fiStream* readStream = ASSET.Open(memFileName, "");

		bool result = ReadDataFromXmlFile(readStream);

		savegameDebugf2("CSaveGameBuffer::LoadBlockData - Done loading from stream '%s'", readStream->GetName());

		readStream->Close();

		return result;
	}
#endif // SAVEGAME_USES_XML
	else
	{
		savegameErrorf("CSaveGameBuffer::LoadBlockData - PARSER.FindInputFormat()='parManager::%d' is Not Supported. memFileName='%s'", inputFormat, memFileName);
		CGenericGameStorage::SetReasonForFailureToLoadSavegame(LOAD_FAILED_REASON_FILE_CORRUPT);
		USES_PSO_ONLY( delete[] uncompressed; )
			return false;
	}
}


#define Align16(x) (((x)+15)&~15)

static sysIpcThreadId s_ThreadId = sysIpcThreadIdInvalid;

MemoryCardError CSaveGameBuffer::CreateSaveDataAndCalculateSize()
{
	//don't allow the player to save when in animal form
	if(CTheScripts::GetPlayerIsInAnimalForm() && (m_TypeOfSavegame == SAVEGAME_SINGLE_PLAYER))
	{
		savegameErrorf("CSaveGameBuffer::CreateSaveDataAndCalculateSize - CTheScripts::GetPlayerIsInAnimalForm() is true so return MEM_CARD_ERROR");
		return MEM_CARD_ERROR;
	}

	bool saveAsPso = true;
#if __BANK
	//	This is a bit messy.
	//	If the SaveToPC widget is ticked then use the state of the SaveAsXML widget.
	//	If the SaveToPC widget is not ticked then use the DefaultFormat.
	g_encryptOnSave = !PARAM_noencryptsave.Get();
	if (CGenericGameStorage::ms_bSaveToPC)
	{
		if (!CGenericGameStorage::ms_bSaveAsPso)
		{
			g_encryptOnSave = false;
		}
	}
	saveAsPso = CGenericGameStorage::ms_bSaveAsPso;
#endif	//	__BANK

	if (saveAsPso)
	{
#if SAVEGAME_USES_PSO
		//	Free up any data that might have been left behind from a previous save
		FreeAllDataToBeSaved(true);

		psoBuilderInstance* rootInstance = CreatePsoStructure();

		psoBuilder* builder = m_pPsoBuilderToBeSaved;

		if (m_bBufferContainsScriptData)
		{
#if !__FINAL
			if ( (m_TypeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (PARAM_dont_save_script_data_to_mp_stats.Get() == false) )
#endif	//	!__FINAL
			{
#if __BANK
				if (CGenericGameStorage::ms_bSaveScriptVariables)
#endif	//	__BANK
				{
					CScriptSaveData::AddActiveScriptDataToPsoToBeSaved(m_TypeOfSavegame,*builder, rootInstance);
					//CScriptSaveData::AddInactiveScriptDataToPsoToBeSaved(m_TypeOfSavegame,*builder, rootInstance);
				}
			}
		}

		builder->FinishBuilding();

		u32 psoFileSize = builder->GetFileSize();

		psoSizeDebugf1("CSaveGameBuffer::CreateSaveDataAndCalculateSize :: SIZE is '%d'", psoFileSize);

		SetBufferSize(Align16(psoFileSize)); // need alignment for encryption


#if __BANK
		if (m_TypeOfSavegame == SAVEGAME_MULTIPLAYER_SLOT_CONTAINING_MP_SCRIPT_DATA)
		{
			savegameDisplayf("CSaveGameBuffer::CreateSaveDataAndCalculateSize - start of list of values of script variables that have just been written to an MP savegame");
			CScriptSaveData::DisplayCurrentValuesOfSavedScriptVariables(m_TypeOfSavegame);
			savegameDisplayf("CSaveGameBuffer::CreateSaveDataAndCalculateSize - end of list of values of script variables that have just been written to an MP savegame");
		}
#endif	//	__BANK

#endif // SAVEGAME_USES_PSO
		return MEM_CARD_COMPLETE;
	}
	else
	{
#if SAVEGAME_USES_XML
		parRTStructure *pRTS = CreateRTStructure();

		if (m_pParTreeToBeSaved)
		{
			savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameBuffer::CreateSaveDataAndCalculateSize - Graeme - I'm not dealing with AutoUseTempMemory properly");
			parManager::AutoUseTempMemory temp;

			delete m_pParTreeToBeSaved;	//	make sure to free any memory used for a previously-allocated parTree that wasn't saved and deleted properly
		}

		fiSafeStream SafeStream(ASSET.Create("count:", ""));

		bool prevHeaderSetting = PARSER.Settings().SetFlag(parSettings::WRITE_XML_HEADER, false);
		bool prevNameSetting = PARSER.Settings().SetFlag(parSettings::ENSURE_NAMES_IN_ALL_BUILDS, true);

		{
			savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameBuffer::CreateSaveDataAndCalculateSize - Graeme - I'm not dealing with AutoUseTempMemory properly");
			parManager::AutoUseTempMemory temp;

			m_pParTreeToBeSaved = rage_new parTree;
			m_pParTreeToBeSaved->SetRoot(PARSER.BuildTreeNodeFromStructure(*pRTS, NULL));

			if (m_bBufferContainsScriptData)
			{
#if !__FINAL
				if ( (m_TypeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (PARAM_dont_save_script_data_to_mp_stats.Get() == false) )
#endif	//	!__FINAL
				{
					if (savegameVerifyf(CScriptSaveData::MainHasRegisteredScriptData(m_TypeOfSavegame), "CSaveGameBuffer::CreateSaveDataAndCalculateSize - expected Main script to have already registered save data before saving in xml format"))
					{
						//	Create a child tree node to contain all the save data for scripts
						parTreeNode *pRootOfTreeToBeSaved = m_pParTreeToBeSaved->GetRoot();
						if (savegameVerifyf(pRootOfTreeToBeSaved, "CSaveGameBuffer::CreateSaveDataAndCalculateSize - the main tree does not have a root node"))
						{
							parTreeNode *pScriptDataNode = rage_new parTreeNode(CSaveGameData::GetNameOfMainScriptTreeNode(), true);
							if (savegameVerifyf(pScriptDataNode, "CSaveGameBuffer::CreateSaveDataAndCalculateSize - failed to create tree node for script data"))
							{
								pScriptDataNode->AppendAsChildOf(pRootOfTreeToBeSaved);

								CScriptSaveData::AddActiveScriptDataToTreeToBeSaved(m_TypeOfSavegame, m_pParTreeToBeSaved, -1);

// I don't think I need to call AddInactiveScriptDataToTreeToBeSaved
// I think "inactive script data" is an old idea that we don't use.
// Loaded script data would stay around until its associated script had started and triggered deserialization by registering its save data.
//								CScriptSaveData::AddInactiveScriptDataToTreeToBeSaved(m_TypeOfSavegame, m_pParTreeToBeSaved, -1);
							}
						}
					}

					if(CScriptSaveData::GetNumOfScriptStructs(m_TypeOfSavegame)>0)
					{ // If at least one DLC script has registered script save data
						parTreeNode *pRootOfTreeToBeSaved = m_pParTreeToBeSaved->GetRoot();
						if (savegameVerifyf(pRootOfTreeToBeSaved, "CSaveGameBuffer::CreateSaveDataAndCalculateSize - the main tree does not have a root node"))
						{
							// Create a node called "DLCScriptSaveData". This node will have multiple child nodes - one for each DLC script that has script save data.
							parTreeNode *pDLCScriptDataRoot = rage_new parTreeNode(CSaveGameData::GetNameOfDLCScriptTreeNode(),true);
							if (savegameVerifyf(pDLCScriptDataRoot, "CSaveGameBuffer::CreateSaveDataAndCalculateSize - failed to create tree node for all DLC script data"))
							{
								pDLCScriptDataRoot->AppendAsChildOf(pRootOfTreeToBeSaved);

								for(int i = 0; i<CScriptSaveData::GetNumOfScriptStructs();++i)
								{
									if (savegameVerifyf(CScriptSaveData::DLCHasRegisteredScriptData(m_TypeOfSavegame,i), "CSaveGameBuffer::CreateSaveDataAndCalculateSize - expected DLC script to have already registered save data before saving in xml format"))
									{
										// Create a node for this DLC script. Add the new node as a child of the "DLCScriptSaveData" node.
										parTreeNode *pScriptDataNode = rage_new parTreeNode(CScriptSaveData::GetNameOfScriptStruct(i), true);
										if (savegameVerifyf(pScriptDataNode, "CSaveGameBuffer::CreateSaveDataAndCalculateSize - failed to create tree node for script data %s", CScriptSaveData::GetNameOfScriptStruct(i)))
										{
											pScriptDataNode->AppendAsChildOf(pDLCScriptDataRoot);

											CScriptSaveData::AddActiveScriptDataToTreeToBeSaved(m_TypeOfSavegame, m_pParTreeToBeSaved, i);

// See my comment above the other AddInactiveScriptDataToTreeToBeSaved call
//											CScriptSaveData::AddInactiveScriptDataToTreeToBeSaved(m_TypeOfSavegame, m_pParTreeToBeSaved, i);
										}
									}
								}
							} // if (savegameVerifyf(pDLCScriptDataRoot
						} // if (savegameVerifyf(pRootOfTreeToBeSaved
					} // if (CScriptSaveData::GetNumOfScriptStructs(SAVEGAME_SINGLE_PLAYER) > 0)
				}
			}

			if (savegameVerifyf(SafeStream, "CSaveGameBuffer::CreateSaveDataAndCalculateSize - failed to create stream for calculating file size"))
			{
				PARSER.SaveTree(SafeStream, m_pParTreeToBeSaved, parManager::XML, NULL);
				SafeStream->Flush(); // don't forget this!

				SetBufferSize(Align16(SafeStream->Size())); // need alignment for encryption
			}
		}

		PARSER.Settings().SetFlag(parSettings::ENSURE_NAMES_IN_ALL_BUILDS, prevNameSetting);
		PARSER.Settings().SetFlag(parSettings::WRITE_XML_HEADER, prevHeaderSetting);
#endif // SAVEGAME_USES_XML
		return MEM_CARD_COMPLETE;
	}
}

void CSaveGameBuffer::FreeSavegameMemoryBuffer(u8 *pBuffer)
{	
	if (CNetwork::GetNetworkHeap()->IsValidPointer(pBuffer))
	{		
		sysMemAutoUseNetworkMemory mem;
		delete [] pBuffer;
	}
	else
	{
		sysMemAutoUseFragCacheMemory mem;
		delete [] pBuffer;
	}	
}

void CSaveGameBuffer::FreeBufferContainingContentsOfSavegameFile()
{
	if (m_BufferContainingContentsOfSavegameFile)
	{
		FreeSavegameMemoryBuffer(m_BufferContainingContentsOfSavegameFile);
		m_BufferContainingContentsOfSavegameFile = NULL;
	}
}

DECLARE_THREAD_FUNC(SaveBlockDataThreadFunc)
{
	PF_PUSH_MARKER("SaveBlockData");

	CSaveGameBuffer* pSaveGameBuffer = (CSaveGameBuffer*) ptr;
	bool bSuccess = pSaveGameBuffer->DoSaveBlockData();
	if(bSuccess)
	{
		pSaveGameBuffer->SetSaveBlockDataStatus(SAVEBLOCKDATA_SUCCESS);
	}
	else
	{
		pSaveGameBuffer->SetSaveBlockDataStatus(SAVEBLOCKDATA_ERROR);
	}

#if !__FINAL
	unsigned current, maxi;
	if (sysIpcEstimateStackUsage(current,maxi))
		savegameDisplayf("Save game thread used %u of %u bytes stack space.",current,maxi);
#endif

	PF_POP_MARKER();
}

bool CSaveGameBuffer::BeginSaveBlockData(const bool useEncryption)
{
	if (s_ThreadId != sysIpcThreadIdInvalid) 
	{
		Errorf("BeginSaveBlockData started twice. Previous thread not finished, can't start.");
		return false;
	}

	FatalAssertf(m_SaveBlockDataStatus == SAVEBLOCKDATA_IDLE, "CSaveGameBuffer::BeginSaveBlockData called twice");
	m_SaveBlockDataStatus = SAVEBLOCKDATA_PENDING;
	m_UseEncryptionForSaveBlockData = useEncryption;

	s_ThreadId = sysIpcCreateThread(SaveBlockDataThreadFunc, this, 16384, PRIO_BELOW_NORMAL, "SaveBlockDataThread", 0, "SaveBlockDataThread");
	return s_ThreadId != sysIpcThreadIdInvalid;

}

bool CSaveGameBuffer::DoSaveBlockData()
{
#if SAVEGAME_USES_XML
	if (m_pParTreeToBeSaved)
	{
		savegameAssertf(!CGenericGameStorage::ms_bSaveAsPso, "CSaveGameBuffer::SaveBlockData - Wanted to save as a PSO but found a parTree!");

		char memFileName[RAGE_MAX_PATH];
		fiDevice::MakeMemoryFileName(memFileName, RAGE_MAX_PATH, GetBuffer(), GetBufferSize(), false, "");
		fiSafeStream SafeStream(ASSET.Create(memFileName, ""));

		bool prevSetting = PARSER.Settings().SetFlag(parSettings::WRITE_XML_HEADER, false);

		{
			//		savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameData::SaveBlockData - Graeme - I'm not dealing with AutoUseTempMemory properly");
			//		parManager::AutoUseTempMemory temp;

			if (savegameVerifyf(SafeStream, "CSaveGameBuffer::SaveBlockData - failed to create stream for saving the file"))
			{
				PARSER.SaveTree(SafeStream, m_pParTreeToBeSaved, parManager::XML, NULL);
				SafeStream->Flush(); // Make sure everything's copied to the buffer before encryption
			}

		}

		PARSER.Settings().SetFlag(parSettings::WRITE_XML_HEADER, prevSetting);

		savegameDebugf2("CSaveGameBuffer::SaveBlockData - Done saving to stream '%s'", SafeStream->GetName());
	}
#else
	if (0) { }
#endif // SAVEGAME_USES_XML

	else
	{
#if SAVEGAME_USES_PSO
		savegameAssertf(m_pPsoBuilderToBeSaved, "CSaveGameBuffer::SaveBlockData - Couldn't find a PSO builder object to save");

		m_pPsoBuilderToBeSaved->SaveToBuffer(reinterpret_cast<char*>(GetBuffer()), GetBufferSize());

		savegameDebugf2("CSaveGameBuffer::SaveBlockData - Done saving PSO to buffer");
#endif // SAVEGAME_USES_PSO
	}

	//Compress data
	if(m_TypeOfSavegame == SAVEGAME_MULTIPLAYER_CHARACTER)
	{
		savegameDisplayf("CSaveGameBuffer::DoSaveBlockData - size before compression = %u", GetBufferSize());
		if(!CompressData(GetBuffer(), GetBufferSize()))
		{
			// Compression FAILED
			return false;
		}
		savegameDisplayf("CSaveGameBuffer::DoSaveBlockData - size after compression = %u", GetBufferSize());

#if __ASSERT
		const u32 WARNING_THRESHOLD_FOR_MP_SAVEGAME_SIZE = ( MAX_CLOUD_BUFFER_SIZE - (10*1024));
		savegameAssertf(GetBufferSize() <= WARNING_THRESHOLD_FOR_MP_SAVEGAME_SIZE, "CSaveGameBuffer::DoSaveBlockData - size after compression = %u. Just warning that this is above %u so that we can keep track of the growth of the MP Save over time.", 
			GetBufferSize(), WARNING_THRESHOLD_FOR_MP_SAVEGAME_SIZE);
#endif	//	__ASSERT
	}

	//Now encrypt the buffer
#if __BANK
	if (g_encryptOnSave)
#endif
	{
		if (m_UseEncryptionForSaveBlockData)
		{
			AES aes(AES_KEY_ID_SAVEGAME);
			aes.Encrypt(GetBuffer(), GetBufferSize());

			savegameDisplayf("CSaveGameBuffer::DoSaveBlockData - size after encryption = %u", GetBufferSize());
		}
	}

	return true;
}

#if RSG_ORBIS || RSG_PC || RSG_DURANGO
#define MEMORY_CHECK_LIMIT (m_MaxBufferSize)
#else
#define MEMORY_CHECK_LIMIT (256 << 10)
#endif

void CSaveGameBuffer::EndSaveBlockData()
{
	if (s_ThreadId != sysIpcThreadIdInvalid)
	{
		sysIpcWaitThreadExit(s_ThreadId);
		s_ThreadId = sysIpcThreadIdInvalid;
	}

	SetSaveBlockDataStatus(SAVEBLOCKDATA_IDLE);
}


MemoryCardError CSaveGameBuffer::AllocateDataBuffer()
{
	if (m_BufferContainingContentsOfSavegameFile)
	{
		savegameAssertf(0, "CSaveGameBuffer::AllocateDataBuffer - buffer has already been allocated");
		return MEM_CARD_ERROR;
	}

	u32 BufferSize = MEMORY_CHECK_LIMIT;
	savegameDisplayf("CSaveGameBuffer::AllocateDataBuffer - Allocating %d bytes for save game file", BufferSize);

	if (BufferSize == 0)
	{
		savegameAssertf(0, "CSaveGameBuffer::AllocateDataBuffer - something has gone wrong. BufferSize is 0");
		return MEM_CARD_ERROR;
	}


	{
		if (!CNetwork::IsNetworkOpen())
		{
			savegameAssertf(BufferSize <= (MEMORY_CHECK_LIMIT), "Single player savegame data is too large: %dKB. Memory doesn't grow on tress, you know...", BufferSize / 1024);

			// SP: Use network heap
			sysMemAutoUseNetworkMemory mem;
			m_BufferContainingContentsOfSavegameFile = (u8*) rage_aligned_new(16) u8[BufferSize];
			savegameAssertf(m_BufferContainingContentsOfSavegameFile, "Unable to allocate %u bytes for SaveGame (from network heap)!", BufferSize);
		}
		else
		{
			savegameAssertf(BufferSize <= (MEMORY_CHECK_LIMIT), "Multiplayer savegame data is too large: %dKB. Memory doesn't grow on tress, you know...", BufferSize / 1024);

			// MP: Use frag/game heap
			MEM_USE_USERDATA(MEMUSERDATA_SAVEGAME);
			sysMemAutoUseFragCacheMemory mem;
			m_BufferContainingContentsOfSavegameFile = (u8*) rage_aligned_new(16) u8[BufferSize];
		}				
	}

	if (m_BufferContainingContentsOfSavegameFile)
	{
		memset(m_BufferContainingContentsOfSavegameFile, 0, BufferSize);
		return MEM_CARD_COMPLETE;
	}
	else
	{
		savegameDisplayf("CSaveGameBuffer::AllocateDataBuffer - failed to allocate %d bytes so try again next frame", BufferSize);
		return MEM_CARD_BUSY;
	}
}


void CSaveGameBuffer::SetBufferSize(u32 BufferSize)
{
	if (BufferSize > m_MaxBufferSize)
	{
		savegameAssertf(0, "CSaveGameBuffer::SetBufferSize - new buffer size %d is larger than the expected maximum %d - let Graeme know", BufferSize, m_MaxBufferSize);
		m_MaxBufferSize = BufferSize;
	}
	m_BufferSize = BufferSize;
}


#if SAVEGAME_USES_XML
bool CSaveGameBuffer::LoadXmlTreeFromStream(fiStream *pStream, parRTStructure &structure)
{
	//	LoadTree allocates from the temp allocator but I want to keep the nametable for this tree in memory for the duration of the game
	//	Could that cause problems?
	savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameBuffer::LoadXmlTreeFromStream - Graeme - I'm not dealing with AutoUseTempMemory properly");

	if (m_pLoadedParTree)
	{
		savegameAssertf(0, "CSaveGameBuffer::LoadXmlTreeFromStream - expected any previously-loaded parTree to have been deleted before now");

		savegameAssertf(!PARSER.Settings().GetFlag(parSettings::USE_TEMPORARY_HEAP), "CSaveGameBuffer::LoadXmlTreeFromStream - Graeme - I'm not dealing with AutoUseTempMemory properly");
		parManager::AutoUseTempMemory temp;
		delete m_pLoadedParTree;	//	make sure to free any memory used for a previously-loaded parTree
		m_pLoadedParTree = NULL;
	}
	m_pLoadedParTree = PARSER.LoadTree(pStream, NULL);
	savegameAssertf(m_pLoadedParTree && m_pLoadedParTree->GetRoot(), "CSaveGameBuffer::LoadXmlTreeFromStream - Couldn't load from file '%s'. File was empty or incorrect format", pStream->GetName());
	if (!m_pLoadedParTree)
	{
		return false;
	}

	if (m_bBufferContainsScriptData)
	{
#if !__FINAL
		if ( (m_TypeOfSavegame == SAVEGAME_SINGLE_PLAYER) || (PARAM_dont_load_script_data_from_mp_stats.Get() == false) )
#endif	//	!__FINAL
		{
			CScriptSaveData::ReadScriptTreesFromLoadedTree(m_TypeOfSavegame, m_pLoadedParTree);
		}
	}

	bool result = PARSER.LoadFromStructure(m_pLoadedParTree->GetRoot(), structure, NULL, false);
	if (result)
	{
		AfterReadingFromXmlFile();
	}

	return result;
}

void CSaveGameBuffer::DeleteRootNodeOfTree()
{
	parManager::AutoUseTempMemory temp;

	delete m_pLoadedParTree->GetRoot();
	m_pLoadedParTree->SetRoot(NULL);
}

void CSaveGameBuffer::DeleteEntireTree()
{
	parManager::AutoUseTempMemory temp;

	delete m_pLoadedParTree;
	m_pLoadedParTree = NULL;
}
#endif	//	SAVEGAME_USES_XML

