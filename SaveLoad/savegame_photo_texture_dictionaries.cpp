
#include "SaveLoad/savegame_photo_texture_dictionaries.h"



// Framework headers
#include "fwscene/stores/txdstore.h"
#include "fwsys/gameskeleton.h"


// Game headers
#include "SaveLoad/savegame_channel.h"


#if __LOAD_LOCAL_PHOTOS

// ****************************** CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto ************************************************

void CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Init(unsigned initMode)
{
	if (initMode == INIT_SESSION)
	{
		m_UniquePhotoIdToAppendToTxdName.Reset();
	}
}


void CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		photoAssertf(m_UniquePhotoIdToAppendToTxdName.GetIsFree(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Shutdown - Graeme - Do I need to decrement refs on the txd or something?");

		if (!m_UniquePhotoIdToAppendToTxdName.GetIsFree())
		{
// 			find txd that ends with this number
// 				assert that its number of references is 1
// 				decrement ref
// 				do whatever else is required to free the txd
		}
		m_UniquePhotoIdToAppendToTxdName.Reset();
	}
}



bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::IsFree()
{
	if (m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_UNLOADED)
	{
		photoAssertf(m_UniquePhotoIdToAppendToTxdName.GetIsFree(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::IsFree - expected unique id to be 0 for an unloaded texture dictionary but it's %d", m_UniquePhotoIdToAppendToTxdName.GetValue());
		return true;
	}

	return false;
}

bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId(const CSavegamePhotoUniqueId &uniquePhotoId, bool &bAlreadyContainsThisIndex)
{
	bAlreadyContainsThisIndex = false;
	switch (m_StateOfTextureDictionary)
	{
		case STATE_OF_TEXTURE_DICTIONARY_UNLOADED :
			photoAssertf(m_UniquePhotoIdToAppendToTxdName.GetIsFree(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId - STATE_OF_TEXTURE_DICTIONARY_UNLOADED - expected unique id to be 0 for an unloaded texture dictionary but it's %d", m_UniquePhotoIdToAppendToTxdName.GetValue());
			return true;
//			break;

//		STATE_OF_TEXTURE_DICTIONARY_BEGIN_CREATION_OF_IMAGE

		case STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE :
			photoAssertf(!m_UniquePhotoIdToAppendToTxdName.GetIsFree(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId - STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE - expected unique id to be valid");
			photoAssertf(m_UniquePhotoIdToAppendToTxdName != uniquePhotoId, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId - STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE - already exists for unique id %d", m_UniquePhotoIdToAppendToTxdName.GetValue());
			break;

		case STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY :
			photoAssertf(!m_UniquePhotoIdToAppendToTxdName.GetIsFree(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId - STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY - expected unique id to be valid");
			photoAssertf(m_UniquePhotoIdToAppendToTxdName != uniquePhotoId, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId - STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY - already exists for unique id %d", m_UniquePhotoIdToAppendToTxdName.GetValue());
			break;

		case STATE_OF_TEXTURE_DICTIONARY_DONE :
			photoAssertf(!m_UniquePhotoIdToAppendToTxdName.GetIsFree(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId - STATE_OF_TEXTURE_DICTIONARY_DONE - expected unique id to be valid");
			if (m_UniquePhotoIdToAppendToTxdName == uniquePhotoId)
			{
				bAlreadyContainsThisIndex = true;
				return true;
			}
			break;

//		STATE_OF_TEXTURE_DICTIONARY_BEGIN_DELETION,	//	Maybe some more deletion states after this

		case STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR :
			photoAssertf(m_UniquePhotoIdToAppendToTxdName.GetIsFree(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId - STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR - expected unique id to be 0 but it's %d", m_UniquePhotoIdToAppendToTxdName.GetValue());
			photoAssertf(m_UniquePhotoIdToAppendToTxdName != uniquePhotoId, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisUniquePhotoId - STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR - already exists for unique id %d", m_UniquePhotoIdToAppendToTxdName.GetValue());
			m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_UNLOADED;
			return true;
			break;
	}

	return false;
}


bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::HasUniquePhotoId(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	if (m_UniquePhotoIdToAppendToTxdName == uniquePhotoId)
	{
		return true;
	}

	return false;
}

bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::BeginCreatingTextureDictionaryUsingTextureDownloadManager(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	if (photoVerifyf(m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_UNLOADED, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::BeginCreatingTextureDictionaryUsingTextureDownloadManager::BeginCreatingTextureDictionary - expected state to be STATE_OF_TEXTURE_DICTIONARY_UNLOADED, but it's %d", m_StateOfTextureDictionary))
	{
		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE;
		m_UniquePhotoIdToAppendToTxdName = uniquePhotoId;

		photoDisplayf("CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::BeginCreatingTextureDictionaryUsingTextureDownloadManager - called for %d", m_UniquePhotoIdToAppendToTxdName.GetValue());

		return true;
	}

	return false;
}

void CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToFailed()
{
	if (photoVerifyf(m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToFailed - expected state to be STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE, but it's %d", m_StateOfTextureDictionary))
	{
		photoDisplayf("CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToFailed - called for %d", m_UniquePhotoIdToAppendToTxdName.GetValue());

		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR;
		m_UniquePhotoIdToAppendToTxdName.Reset();
	}
}

void CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToComplete()
{
	if (photoVerifyf(m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToComplete - expected state to be STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE, but it's %d", m_StateOfTextureDictionary))
	{
		photoDisplayf("CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToComplete - called for %d", m_UniquePhotoIdToAppendToTxdName.GetValue());

		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_DONE;
	}
}


MemoryCardError CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::GetStatusOfTextureDictionaryCreation()
{
	switch (m_StateOfTextureDictionary)
	{
		case STATE_OF_TEXTURE_DICTIONARY_UNLOADED :
			return MEM_CARD_ERROR;	//	Not sure what to return here. Is it an error to check the Creation status before we've started the Creation?
//			break;

//		case STATE_OF_TEXTURE_DICTIONARY_BEGIN_CREATION_OF_IMAGE :
		case STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE :
		case STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY :
			return MEM_CARD_BUSY;
			//	break;

		case STATE_OF_TEXTURE_DICTIONARY_DONE :
			return MEM_CARD_COMPLETE;
			//	break;

//		case STATE_OF_TEXTURE_DICTIONARY_BEGIN_DELETION :	//	Maybe some more deletion states after this
//			return MEM_CARD_ERROR;	//	Should I return error for the creation state if we're in the process of deleting it?
			//	break;
		case STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR :		//	Will I need a function to reset the state to STATE_OF_TEXTURE_DICTIONARY_UNLOADED from this state?
			return MEM_CARD_ERROR;
			//	break;
	}

	return MEM_CARD_ERROR;
}



bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload()
{
	if ( (m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_UNLOADED)
		|| (m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR) )
	{	//	Nothing to unload
		photoAssertf(m_UniquePhotoIdToAppendToTxdName.GetIsFree(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected unique id to be 0 for an unloaded texture dictionary but it's %d. m_StateOfTextureDictionary = %d", m_UniquePhotoIdToAppendToTxdName.GetValue(), m_StateOfTextureDictionary);
		
		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_UNLOADED;
		m_UniquePhotoIdToAppendToTxdName.Reset();
		
		return true;
	}

	if (photoVerifyf(m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_DONE, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected state to be STATE_OF_TEXTURE_DICTIONARY_DONE but it's %d", m_StateOfTextureDictionary))
	{
		char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
		CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary(m_UniquePhotoIdToAppendToTxdName, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

		// add the texture to the dummy txd:
		strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(textureName));

		// add slot to txd store
		if (photoVerifyf(txdSlot != -1, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected to find a txd with name %s", textureName))
		{
			photoDisplayf("CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - Ref count for %s texture dictionary is %d before removing a ref", textureName, g_TxdStore.GetNumRefs(txdSlot));

//	GSW_Attempt_3
//	It seems that DeleteEntry isn't defined in __FINAL builds so I'll comment this out for now
// 			if (Verifyf(g_TxdStore.IsValidSlot(txdSlot), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - failed to get valid txd with name %s", textureName))
// 			{
// 				fwTxd* pTxd = g_TxdStore.GetSafeFromIndex(txdSlot);
// 				if (Verifyf(pTxd, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - failed to get pointer to txd with name %s", textureName))
// 				{
// 					grcTexture *pTextureInsideTxd = pTxd->Lookup(textureName);
// 					savegameAssertf(pTextureInsideTxd == m_LowQualityGalleryPhoto.GetGrcTexture(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - texture inside txd has address %x but texture for low quality photo has address %x", pTextureInsideTxd, m_LowQualityGalleryPhoto.GetGrcTexture());
// 
// 					pTxd->DeleteEntry(textureName);
// 
// 					savegameAssertf(pTextureInsideTxd->GetRefCount() == 1, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected the ref count for the %s texture to be 1 before calling DecRef() but it's %d", textureName, pTextureInsideTxd->GetRefCount());
// 					pTextureInsideTxd->DecRef();
// 					m_LowQualityGalleryPhoto.RequestFreeMemoryForLowQualityPhoto();
// 				}
// 			}


			g_TxdStore.RemoveRef(txdSlot, REF_OTHER);
//			savegameAssertf(g_TxdStore.FindSlot(textureName) == -1, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected txd with name %s to have been deleted by now", textureName);

//	GSW_Attempt_1
//			m_LowQualityGalleryPhoto.ClearGrcTexturePointerWithoutDeleting();

			m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_UNLOADED;
			m_UniquePhotoIdToAppendToTxdName.Reset();

			return true;
		}
	}
	
	return false;
}


// ****************************** CTextureDictionariesForGalleryPhotos ************************************************

void CTextureDictionariesForGalleryPhotos::Init(unsigned initMode)
{
	for (u32 loop = 0; loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY; loop++)
	{
		m_ArrayOfTextureDictionaries[loop].Init(initMode);
	}
}

void CTextureDictionariesForGalleryPhotos::Shutdown(unsigned shutdownMode)
{
	for (u32 loop = 0; loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY; loop++)
	{
		m_ArrayOfTextureDictionaries[loop].Shutdown(shutdownMode);
	}
}


s32 CTextureDictionariesForGalleryPhotos::FindFreeTextureDictionary(const CSavegamePhotoUniqueId &uniquePhotoId, bool &bAlreadyLoaded)
{
	bAlreadyLoaded = false;

//	Find a free entry in the m_ArrayOfTextureDictionaries array (its index will be -1)
	s32 loop = 0;
	s32 free_index = -1;
	s32 already_loaded_into = -1;
	bool bAlreadyContainsThisIndex = false;
	while (loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY)
	{
		if (m_ArrayOfTextureDictionaries[loop].CanBeUsedByThisUniquePhotoId(uniquePhotoId, bAlreadyContainsThisIndex))
		{
			if (bAlreadyContainsThisIndex)
			{
				photoAssertf(already_loaded_into == -1, "CTextureDictionariesForGalleryPhotos::FindFreeTextureDictionary - more than one slot already contains unique id %d", uniquePhotoId.GetValue());
				already_loaded_into = loop;
			}
			free_index = loop;
		}

		loop++;
	}

	if (already_loaded_into != -1)
	{
		bAlreadyLoaded = true;
		return already_loaded_into;
	}

	if (free_index == -1)
	{
		photoAssertf(0, "CTextureDictionariesForGalleryPhotos::FindFreeTextureDictionary - failed to find a free slot in the array of texture dictionaries");
	}
	else
	{
		m_ArrayOfTextureDictionaries[free_index].BeginCreatingTextureDictionaryUsingTextureDownloadManager(uniquePhotoId);
	}

	return free_index;
}

void CTextureDictionariesForGalleryPhotos::SetTextureDownloadSucceeded(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	s32 found_index = GetTextureDictionaryIndexForThisUniquePhotoId(uniquePhotoId);

	if (found_index == -1)
	{
		photoAssertf(0, "CTextureDictionariesForGalleryPhotos::SetTextureDownloadSucceeded - didn't find an entry in the array for unique id %d", uniquePhotoId.GetValue());
		return;
	}

	m_ArrayOfTextureDictionaries[found_index].SetStatusToComplete();
}


void CTextureDictionariesForGalleryPhotos::SetTextureDownloadFailed(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	s32 found_index = GetTextureDictionaryIndexForThisUniquePhotoId(uniquePhotoId);

	if (found_index == -1)
	{
		photoAssertf(0, "CTextureDictionariesForGalleryPhotos::SetTextureDownloadFailed - didn't find an entry in the array for unique id %d", uniquePhotoId.GetValue());
		return;
	}

	m_ArrayOfTextureDictionaries[found_index].SetStatusToFailed();
}


s32 CTextureDictionariesForGalleryPhotos::GetTextureDictionaryIndexForThisUniquePhotoId(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	s32 loop = 0;
	s32 found_index = -1;
	while (loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY)
	{
		if (m_ArrayOfTextureDictionaries[loop].HasUniquePhotoId(uniquePhotoId))
		{
			photoAssertf(found_index == -1, "CTextureDictionariesForGalleryPhotos::GetTextureDictionaryIndexForThisUniquePhotoId - didn't expect two entries for the same unique photo id %d", uniquePhotoId.GetValue());
			found_index = loop;
		}

		loop++;
	}

	return found_index;
}

MemoryCardError CTextureDictionariesForGalleryPhotos::GetStatusOfTextureDictionaryCreation(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	s32 found_index = GetTextureDictionaryIndexForThisUniquePhotoId(uniquePhotoId);

	if (found_index == -1)
	{
		photoAssertf(0, "CTextureDictionariesForGalleryPhotos::GetStatusOfTextureDictionaryCreation - didn't find an entry in the array for unique photo id %d", uniquePhotoId.GetValue());
		return MEM_CARD_ERROR;
	}

	return m_ArrayOfTextureDictionaries[found_index].GetStatusOfTextureDictionaryCreation();
}


bool CTextureDictionariesForGalleryPhotos::IsThisPhotoLoadedIntoAnyTextureDictionary(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	s32 found_index = GetTextureDictionaryIndexForThisUniquePhotoId(uniquePhotoId);

	if (found_index == -1)
	{
		return false;
	}

	if (m_ArrayOfTextureDictionaries[found_index].GetStatusOfTextureDictionaryCreation() == MEM_CARD_COMPLETE)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTextureDictionariesForGalleryPhotos::AreAnyPhotosLoadedIntoTextureDictionaries()
{
	for (u32 loop = 0; loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY; loop++)
	{
		if (m_ArrayOfTextureDictionaries[loop].IsFree() == false)
		{
			return true;
		}
	}

	return false;
}

MemoryCardError CTextureDictionariesForGalleryPhotos::UnloadPhoto(const CSavegamePhotoUniqueId &uniquePhotoId)
{
	s32 found_index = GetTextureDictionaryIndexForThisUniquePhotoId(uniquePhotoId);

	if (found_index == -1)
	{
		return MEM_CARD_ERROR;
	}

	if (m_ArrayOfTextureDictionaries[found_index].Unload())
	{
		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_ERROR;
}

MemoryCardError CTextureDictionariesForGalleryPhotos::UnloadAllPhotos()
{
	MemoryCardError returnValue = MEM_CARD_COMPLETE;

	for (u32 loop = 0; loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY; loop++)
	{
		if (m_ArrayOfTextureDictionaries[loop].Unload() == false)
		{
			returnValue = MEM_CARD_ERROR;
		}
	}

	//	If returnValue is MEM_CARD_ERROR at this stage then we've had at least one error
	return returnValue;
}

void CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary(const CSavegamePhotoUniqueId &uniquePhotoId, char *pTextureName, u32 LengthOfTextureName)
{
	if (photoVerifyf(LengthOfTextureName >= ms_MaxLengthOfTextureName, "CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary - LengthOfTextureName must be at least %u but it's only %u", ms_MaxLengthOfTextureName, LengthOfTextureName))
	{
		if (photoVerifyf(pTextureName, "CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary - string pointer for texture name is NULL"))
		{
			if (uniquePhotoId.GetIsMaximised())
			{
				formatf(pTextureName, LengthOfTextureName, "PHOTO_TEXTURE_%d_MAX", uniquePhotoId.GetValue());
			}
			else
			{
				formatf(pTextureName, LengthOfTextureName, "PHOTO_TEXTURE_%d", uniquePhotoId.GetValue());
			}
		}
	}
}


#else	//	__LOAD_LOCAL_PHOTOS

// ****************************** CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto ************************************************

void CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Init(unsigned initMode)
{
	if (initMode == INIT_SESSION)
	{
		//	CLowQualityScreenshot m_LowQualityGalleryPhoto;
		m_IndexToAppendToTxdName.SetInvalid();	//	 = -1;
	}
}


void CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		photoAssertf(!m_IndexToAppendToTxdName.IsValid(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Shutdown - Graeme - Do I need to decrement refs on the txd or something?");

		if (m_IndexToAppendToTxdName.IsValid())
		{
			// 			find txd that ends with this number
			// 				assert that its number of references is 1
			// 				decrement ref
			// 				do whatever else is required to free the txd
		}
		m_IndexToAppendToTxdName.SetInvalid();
	}
}



bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::IsFree()
{
	if (m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_UNLOADED)
	{
		photoAssertf(!m_IndexToAppendToTxdName.IsValid(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::IsFree - expected index to be -1 for an unloaded texture dictionary but it's %d", m_IndexToAppendToTxdName.GetIntegerValue());
		return true;
	}

	return false;
}

bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex(const CIndexWithinMergedPhotoList &indexToBeAdded, bool &bAlreadyContainsThisIndex)
{
	bAlreadyContainsThisIndex = false;
	switch (m_StateOfTextureDictionary)
	{
	case STATE_OF_TEXTURE_DICTIONARY_UNLOADED :
		photoAssertf(!m_IndexToAppendToTxdName.IsValid(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex - STATE_OF_TEXTURE_DICTIONARY_UNLOADED - expected index to be -1 for an unloaded texture dictionary but it's %d", m_IndexToAppendToTxdName.GetIntegerValue());
		return true;
		//			break;

		//		STATE_OF_TEXTURE_DICTIONARY_BEGIN_CREATION_OF_IMAGE

	case STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE :
		photoAssertf(m_IndexToAppendToTxdName.IsValid(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex - STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE - expected index to be valid");
		photoAssertf(m_IndexToAppendToTxdName != indexToBeAdded, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex - STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE - already exists for index %d", m_IndexToAppendToTxdName.GetIntegerValue());
		break;

	case STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY :
		photoAssertf(m_IndexToAppendToTxdName.IsValid(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex - STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY - expected index to be valid");
		photoAssertf(m_IndexToAppendToTxdName != indexToBeAdded, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex - STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY - already exists for index %d", m_IndexToAppendToTxdName.GetIntegerValue());
		break;

	case STATE_OF_TEXTURE_DICTIONARY_DONE :
		photoAssertf(m_IndexToAppendToTxdName.IsValid(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex - STATE_OF_TEXTURE_DICTIONARY_DONE - expected index to be valid");
		if (m_IndexToAppendToTxdName == indexToBeAdded)
		{
			bAlreadyContainsThisIndex = true;
			return true;
		}
		break;

		//		STATE_OF_TEXTURE_DICTIONARY_BEGIN_DELETION,	//	Maybe some more deletion states after this

	case STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR :
		photoAssertf(!m_IndexToAppendToTxdName.IsValid(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex - STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR - expected index to be -1 but it's %d", m_IndexToAppendToTxdName.GetIntegerValue());
		photoAssertf(m_IndexToAppendToTxdName != indexToBeAdded, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::CanBeUsedByThisMergedPhotoListIndex - STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR - already exists for index %d", m_IndexToAppendToTxdName.GetIntegerValue());
		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_UNLOADED;
		return true;
		break;
	}

	return false;
}


bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::HasMergedIndex(const CIndexWithinMergedPhotoList &indexToAppendToTxdName)
{
	if (m_IndexToAppendToTxdName == indexToAppendToTxdName)
	{
		return true;
	}

	return false;
}

bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::BeginCreatingTextureDictionaryUsingTextureDownloadManager(const CIndexWithinMergedPhotoList &indexToAppendToTxdName)
{
	if (photoVerifyf(m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_UNLOADED, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::BeginCreatingTextureDictionaryUsingTextureDownloadManager::BeginCreatingTextureDictionary - expected state to be STATE_OF_TEXTURE_DICTIONARY_UNLOADED, but it's %d", m_StateOfTextureDictionary))
	{
		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE;
		m_IndexToAppendToTxdName = indexToAppendToTxdName;

		photoDisplayf("CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::BeginCreatingTextureDictionaryUsingTextureDownloadManager - called for %d", m_IndexToAppendToTxdName.GetIntegerValue());

		return true;
	}

	return false;
}

void CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToFailed()
{
	if (photoVerifyf(m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToFailed - expected state to be STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE, but it's %d", m_StateOfTextureDictionary))
	{
		photoDisplayf("CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToFailed - called for %d", m_IndexToAppendToTxdName.GetIntegerValue());

		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR;
		m_IndexToAppendToTxdName.SetInvalid();
	}
}

void CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToComplete()
{
	if (photoVerifyf(m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToComplete - expected state to be STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE, but it's %d", m_StateOfTextureDictionary))
	{
		photoDisplayf("CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::SetStatusToComplete - called for %d", m_IndexToAppendToTxdName.GetIntegerValue());

		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_DONE;
	}
}


MemoryCardError CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::GetStatusOfTextureDictionaryCreation()
{
	switch (m_StateOfTextureDictionary)
	{
	case STATE_OF_TEXTURE_DICTIONARY_UNLOADED :
		return MEM_CARD_ERROR;	//	Not sure what to return here. Is it an error to check the Creation status before we've started the Creation?
		//			break;

		//		case STATE_OF_TEXTURE_DICTIONARY_BEGIN_CREATION_OF_IMAGE :
	case STATE_OF_TEXTURE_DICTIONARY_WAIT_FOR_CREATION_OF_IMAGE :
	case STATE_OF_TEXTURE_DICTIONARY_CREATE_TEXTURE_DICTIONARY :
		return MEM_CARD_BUSY;
		//	break;

	case STATE_OF_TEXTURE_DICTIONARY_DONE :
		return MEM_CARD_COMPLETE;
		//	break;

		//		case STATE_OF_TEXTURE_DICTIONARY_BEGIN_DELETION :	//	Maybe some more deletion states after this
		//			return MEM_CARD_ERROR;	//	Should I return error for the creation state if we're in the process of deleting it?
		//	break;
	case STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR :		//	Will I need a function to reset the state to STATE_OF_TEXTURE_DICTIONARY_UNLOADED from this state?
		return MEM_CARD_ERROR;
		//	break;
	}

	return MEM_CARD_ERROR;
}



bool CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload()
{
	if ( (m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_UNLOADED)
		|| (m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_HAD_ERROR) )
	{	//	Nothing to unload
		photoAssertf(!m_IndexToAppendToTxdName.IsValid(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected index to be -1 for an unloaded texture dictionary but it's %d. m_StateOfTextureDictionary = %d", m_IndexToAppendToTxdName.GetIntegerValue(), m_StateOfTextureDictionary);

		m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_UNLOADED;
		m_IndexToAppendToTxdName.SetInvalid();

		return true;
	}

	if (photoVerifyf(m_StateOfTextureDictionary == STATE_OF_TEXTURE_DICTIONARY_DONE, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected state to be STATE_OF_TEXTURE_DICTIONARY_DONE but it's %d", m_StateOfTextureDictionary))
	{
		char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
		CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary(m_IndexToAppendToTxdName, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

		// add the texture to the dummy txd:
		strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(textureName));

		// add slot to txd store
		if (photoVerifyf(txdSlot != -1, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected to find a txd with name %s", textureName))
		{
			photoDisplayf("CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - Ref count for %s texture dictionary is %d before removing a ref", textureName, g_TxdStore.GetNumRefs(txdSlot));

			//	GSW_Attempt_3
			//	It seems that DeleteEntry isn't defined in __FINAL builds so I'll comment this out for now
			// 			if (Verifyf(g_TxdStore.IsValidSlot(txdSlot), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - failed to get valid txd with name %s", textureName))
			// 			{
			// 				fwTxd* pTxd = g_TxdStore.GetSafeFromIndex(txdSlot);
			// 				if (Verifyf(pTxd, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - failed to get pointer to txd with name %s", textureName))
			// 				{
			// 					grcTexture *pTextureInsideTxd = pTxd->Lookup(textureName);
			// 					savegameAssertf(pTextureInsideTxd == m_LowQualityGalleryPhoto.GetGrcTexture(), "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - texture inside txd has address %x but texture for low quality photo has address %x", pTextureInsideTxd, m_LowQualityGalleryPhoto.GetGrcTexture());
			// 
			// 					pTxd->DeleteEntry(textureName);
			// 
			// 					savegameAssertf(pTextureInsideTxd->GetRefCount() == 1, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected the ref count for the %s texture to be 1 before calling DecRef() but it's %d", textureName, pTextureInsideTxd->GetRefCount());
			// 					pTextureInsideTxd->DecRef();
			// 					m_LowQualityGalleryPhoto.RequestFreeMemoryForLowQualityPhoto();
			// 				}
			// 			}


			g_TxdStore.RemoveRef(txdSlot, REF_OTHER);
			//			savegameAssertf(g_TxdStore.FindSlot(textureName) == -1, "CTextureDictionariesForGalleryPhotos::TextureDictionaryForGalleryPhoto::Unload - expected txd with name %s to have been deleted by now", textureName);

			//	GSW_Attempt_1
			//			m_LowQualityGalleryPhoto.ClearGrcTexturePointerWithoutDeleting();

			m_StateOfTextureDictionary = STATE_OF_TEXTURE_DICTIONARY_UNLOADED;
			m_IndexToAppendToTxdName.SetInvalid();

			return true;
		}
	}

	return false;
}


// ****************************** CTextureDictionariesForGalleryPhotos ************************************************

void CTextureDictionariesForGalleryPhotos::Init(unsigned initMode)
{
	for (u32 loop = 0; loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY; loop++)
	{
		m_ArrayOfTextureDictionaries[loop].Init(initMode);
	}
}

void CTextureDictionariesForGalleryPhotos::Shutdown(unsigned shutdownMode)
{
	for (u32 loop = 0; loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY; loop++)
	{
		m_ArrayOfTextureDictionaries[loop].Shutdown(shutdownMode);
	}
}


s32 CTextureDictionariesForGalleryPhotos::FindFreeTextureDictionary(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList, bool &bAlreadyLoaded)
{
	bAlreadyLoaded = false;

	//	Find a free entry in the m_ArrayOfTextureDictionaries array (its index will be -1)
	s32 loop = 0;
	s32 free_index = -1;
	s32 already_loaded_into = -1;
	bool bAlreadyContainsThisIndex = false;
	while (loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY)
	{
		if (m_ArrayOfTextureDictionaries[loop].CanBeUsedByThisMergedPhotoListIndex(indexWithinMergedPhotoList, bAlreadyContainsThisIndex))
		{
			if (bAlreadyContainsThisIndex)
			{
				photoAssertf(already_loaded_into == -1, "CTextureDictionariesForGalleryPhotos::FindFreeTextureDictionary - more than one slot already contains index %d", indexWithinMergedPhotoList.GetIntegerValue());
				already_loaded_into = loop;
			}
			free_index = loop;
		}

		loop++;
	}

	if (already_loaded_into != -1)
	{
		bAlreadyLoaded = true;
		return already_loaded_into;
	}

	if (free_index == -1)
	{
		photoAssertf(0, "CTextureDictionariesForGalleryPhotos::FindFreeTextureDictionary - failed to find a free slot in the array of texture dictionaries");
	}
	else
	{
		m_ArrayOfTextureDictionaries[free_index].BeginCreatingTextureDictionaryUsingTextureDownloadManager(indexWithinMergedPhotoList);
	}

	return free_index;
}

void CTextureDictionariesForGalleryPhotos::SetTextureDownloadSucceeded(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	s32 found_index = GetTextureDictionaryIndexForThisMergedPhotoIndex(indexWithinMergedPhotoList);

	if (found_index == -1)
	{
		photoAssertf(0, "CTextureDictionariesForGalleryPhotos::SetTextureDownloadSucceeded - didn't find an entry in the array for merged index %d", indexWithinMergedPhotoList.GetIntegerValue());
		return;
	}

	m_ArrayOfTextureDictionaries[found_index].SetStatusToComplete();
}


void CTextureDictionariesForGalleryPhotos::SetTextureDownloadFailed(const CIndexWithinMergedPhotoList &indexWithinMergedPhotoList)
{
	s32 found_index = GetTextureDictionaryIndexForThisMergedPhotoIndex(indexWithinMergedPhotoList);

	if (found_index == -1)
	{
		photoAssertf(0, "CTextureDictionariesForGalleryPhotos::SetTextureDownloadFailed - didn't find an entry in the array for merged index %d", indexWithinMergedPhotoList.GetIntegerValue());
		return;
	}

	m_ArrayOfTextureDictionaries[found_index].SetStatusToFailed();
}


s32 CTextureDictionariesForGalleryPhotos::GetTextureDictionaryIndexForThisMergedPhotoIndex(const CIndexWithinMergedPhotoList &indexToAppendToTxdName)
{
	s32 loop = 0;
	s32 found_index = -1;
	while (loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY)
	{
		if (m_ArrayOfTextureDictionaries[loop].HasMergedIndex(indexToAppendToTxdName))
		{
			photoAssertf(found_index == -1, "CTextureDictionariesForGalleryPhotos::GetTextureDictionaryIndexForThisMergedPhotoIndex - didn't expect two entries for the same merged index %d", indexToAppendToTxdName.GetIntegerValue());
			found_index = loop;
		}

		loop++;
	}

	return found_index;
}

MemoryCardError CTextureDictionariesForGalleryPhotos::GetStatusOfTextureDictionaryCreation(const CIndexWithinMergedPhotoList &indexToAppendToTxdName)
{
	s32 found_index = GetTextureDictionaryIndexForThisMergedPhotoIndex(indexToAppendToTxdName);

	if (found_index == -1)
	{
		photoAssertf(0, "CTextureDictionariesForGalleryPhotos::GetStatusOfTextureDictionaryCreation - didn't find an entry in the array for merged index %d", indexToAppendToTxdName.GetIntegerValue());
		return MEM_CARD_ERROR;
	}

	return m_ArrayOfTextureDictionaries[found_index].GetStatusOfTextureDictionaryCreation();
}


bool CTextureDictionariesForGalleryPhotos::IsThisPhotoLoadedIntoAnyTextureDictionary(const CIndexWithinMergedPhotoList &indexToAppendToTxdName)
{
	s32 found_index = GetTextureDictionaryIndexForThisMergedPhotoIndex(indexToAppendToTxdName);

	if (found_index == -1)
	{
		return false;
	}

	if (m_ArrayOfTextureDictionaries[found_index].GetStatusOfTextureDictionaryCreation() == MEM_CARD_COMPLETE)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTextureDictionariesForGalleryPhotos::AreAnyPhotosLoadedIntoTextureDictionaries()
{
	for (u32 loop = 0; loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY; loop++)
	{
		if (m_ArrayOfTextureDictionaries[loop].IsFree() == false)
		{
			return true;
		}
	}

	return false;
}

MemoryCardError CTextureDictionariesForGalleryPhotos::UnloadPhoto(const CIndexWithinMergedPhotoList &indexToAppendToTxdName)
{
	s32 found_index = GetTextureDictionaryIndexForThisMergedPhotoIndex(indexToAppendToTxdName);

	if (found_index == -1)
	{
		return MEM_CARD_ERROR;
	}

	if (m_ArrayOfTextureDictionaries[found_index].Unload())
	{
		return MEM_CARD_COMPLETE;
	}

	return MEM_CARD_ERROR;
}

MemoryCardError CTextureDictionariesForGalleryPhotos::UnloadAllPhotos()
{
	MemoryCardError returnValue = MEM_CARD_COMPLETE;

	for (u32 loop = 0; loop < MAX_TEXTURE_DICTIONARIES_FOR_GALLERY; loop++)
	{
		if (m_ArrayOfTextureDictionaries[loop].Unload() == false)
		{
			returnValue = MEM_CARD_ERROR;
		}
	}

	//	If returnValue is MEM_CARD_ERROR at this stage then we've had at least one error
	return returnValue;
}

void CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary(const CIndexWithinMergedPhotoList &photoIndex, char *pTextureName, u32 LengthOfTextureName)
{
	if (photoVerifyf(LengthOfTextureName >= ms_MaxLengthOfTextureName, "CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary - LengthOfTextureName must be at least %u but it's only %u", ms_MaxLengthOfTextureName, LengthOfTextureName))
	{
		if (photoVerifyf(pTextureName, "CTextureDictionariesForGalleryPhotos::GetNameOfPhotoTextureDictionary - string pointer for texture name is NULL"))
		{
			if (photoIndex.GetIsMaximised())
			{
				formatf(pTextureName, LengthOfTextureName, "PHOTO_TEXTURE_%d_MAX", photoIndex.GetIntegerValue());
			}
			else
			{
				formatf(pTextureName, LengthOfTextureName, "PHOTO_TEXTURE_%d", photoIndex.GetIntegerValue());
			}
		}
	}
}

#endif	//	__LOAD_LOCAL_PHOTOS