
#include "SaveLoad/savegame_photo_buffer.h"

// Rage headers
#include "string/stringutil.h"
#include "system/memory.h"

// Framework headers
#include "streaming/streamingengine.h"

// Game headers
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_photo_metadata.h"

//	*************** CPhotoBuffer **************************************************************

CPhotoBuffer::CPhotoBuffer()
{
	m_pDataBuffer = NULL;

	m_BufferOffset = 0;

	m_SizeOfAllocatedJpegBuffer = 0;
	m_SizeOfAllocatedJsonBuffer = 0;
	m_SizeOfAllocatedTitleBuffer = 0;
	m_SizeOfAllocatedDescriptionBuffer = 0;
}


CPhotoBuffer::~CPhotoBuffer()
{
	FreeBuffer();
}


bool CPhotoBuffer::AllocateBufferForLoadingLocalPhoto(u32 sizeOfCompletePhotoBuffer)
{
	//	I can't set m_SizeOfAllocatedJpegBuffer or m_SizeOfAllocatedJsonBuffer in here because I don't know what they are
	//	I'll need to read them from the buffer once it has been loaded. That's done by ReadAndVerifyLoadedBuffer()
	return AllocateBuffer(sizeOfCompletePhotoBuffer);
}


bool CPhotoBuffer::AllocateBufferForTakingScreenshot(u32 sizeOfJpegBuffer, u32 SizeOfJsonBuffer)
{
	u32 totalBufferSize = sizeOfJpegBuffer + SizeOfJsonBuffer + ms_DefaultSizeOfTitleBuffer + ms_DefaultSizeOfDescriptionBuffer + ms_NumberOfExtraBytesInBuffer;
	bool bSucceeded = AllocateBuffer(totalBufferSize);
	if (bSucceeded)
	{
		m_SizeOfAllocatedJpegBuffer = sizeOfJpegBuffer;
		m_SizeOfAllocatedJsonBuffer = SizeOfJsonBuffer;
		m_SizeOfAllocatedTitleBuffer = ms_DefaultSizeOfTitleBuffer;
		m_SizeOfAllocatedDescriptionBuffer = ms_DefaultSizeOfDescriptionBuffer;

		FillInValuesForNewPhoto();
	}

	return bSucceeded;
}


bool CPhotoBuffer::AllocateBuffer(u32 sizeOfPhotoBuffer)
{
	if (m_pDataBuffer)
	{
		photoAssertf(0, "CPhotoBuffer::AllocateBuffer - didn't expect buffer to be allocated at this stage");
		FreeBuffer();
	}

//	Do I need to set m_SizeOfAllocatedJpegBuffer to 0 here?

	//	The buffer can't be lower than ms_MaxSizeOfCompleteBuffer because then I'll write off the end in FillInValuesForNewPhoto()
	//	if (photoVerifyf(sizeOfPhotoBuffer >= ms_MaxSizeOfCompleteBuffer, "CPhotoBuffer::AllocateBuffer - size to allocate %u has to be at least %u", sizeOfPhotoBuffer, ms_MaxSizeOfCompleteBuffer))
	{
		MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);
		m_pDataBuffer = (u8*) strStreamingEngine::GetAllocator().Allocate(sizeOfPhotoBuffer, 16, MEMTYPE_RESOURCE_VIRTUAL);
		if (m_pDataBuffer)
		{
			photoDisplayf("CPhotoBuffer::AllocateBuffer - allocated %u bytes", sizeOfPhotoBuffer);

//	Do I need to set m_SizeOfAllocatedJpegBuffer here?

			return true;
		}
	}

	photoDisplayf("CPhotoBuffer::AllocateBuffer - failed to allocate %u bytes", sizeOfPhotoBuffer);
	return false;
}


void CPhotoBuffer::FreeBuffer()
{
	if (m_pDataBuffer)
	{
		MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);

		m_SizeOfAllocatedJpegBuffer = 0;
		m_SizeOfAllocatedJsonBuffer = 0;
		m_SizeOfAllocatedTitleBuffer = 0;
		m_SizeOfAllocatedDescriptionBuffer = 0;

		strStreamingEngine::GetAllocator().Free(m_pDataBuffer);
		m_pDataBuffer = NULL;

		photoDisplayf("CPhotoBuffer::FreeBuffer - buffer has been freed");
	}
}


void CPhotoBuffer::ResetBufferOffset()
{
	m_BufferOffset = 0;
}

void CPhotoBuffer::ForwardBufferOffset(u32 forwardBy)
{
	m_BufferOffset += forwardBy;
}

void CPhotoBuffer::WriteValueToBuffer(u32 valueToWrite)
{
	u32 *pAddressToWriteTo = (u32*) &m_pDataBuffer[m_BufferOffset];
	*pAddressToWriteTo = valueToWrite;
	m_BufferOffset += 4;
}

u32 CPhotoBuffer::ReadValueFromBuffer()
{
	u32 *pAddressToReadFrom = (u32*) &m_pDataBuffer[m_BufferOffset];
	m_BufferOffset += 4;

	return *pAddressToReadFrom;
}

void CPhotoBuffer::WriteStringToBuffer(const char *pStringToWrite, const u32 numberOfCharacters)
{
	char *pCharacterToWrite = (char*) &m_pDataBuffer[m_BufferOffset];
	for (u32 loop = 0; loop < numberOfCharacters; loop++)
	{
		*pCharacterToWrite = pStringToWrite[loop];
		pCharacterToWrite++;
	}

	m_BufferOffset += numberOfCharacters;
}

bool CPhotoBuffer::ReadStringFromBuffer(const char *pStringToRead, const u32 numberOfCharacters)
{
	char *pCharacterToRead = (char*) &m_pDataBuffer[m_BufferOffset];
	for (u32 loop = 0; loop < numberOfCharacters; loop++)
	{
		if (*pCharacterToRead != pStringToRead[loop])
		{
			photoAssertf(0, "CPhotoBuffer::ReadStringFromBuffer - expected character to be '%c' (reading %s)", pStringToRead[loop], pStringToRead);
			return false;
		}
		pCharacterToRead++;
	}
	
	m_BufferOffset += numberOfCharacters;

	return true;
}

bool CPhotoBuffer::VerifyCurrentOffset(u32 expectedOffset, const char* OUTPUT_ONLY(nameOfOffset))
{
	if (expectedOffset != m_BufferOffset)
	{
		photoDisplayf("CPhotoBuffer::VerifyCurrentOffset - Graeme has made a mistake when calculating the offset of %s within the photo buffer. expected offset (%u) != current offset (%u)", nameOfOffset, expectedOffset, m_BufferOffset);
		photoAssertf(0, "CPhotoBuffer::VerifyCurrentOffset - Graeme has made a mistake when calculating the offset of %s within the photo buffer. expected offset (%u) != current offset (%u)", nameOfOffset, expectedOffset, m_BufferOffset);
		return false;
	}

	return true;
}


void CPhotoBuffer::FillInValuesForNewPhoto()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::FillInValuesForNewPhoto - m_pDataBuffer hasn't been allocated");
		return;
	}

	if (m_SizeOfAllocatedJpegBuffer == 0)
	{
		photoAssertf(0, "CPhotoBuffer::FillInValuesForNewPhoto - didn't expect m_SizeOfAllocatedJpegBuffer to be 0");
		return;
	}

	if (m_SizeOfAllocatedJsonBuffer == 0)
	{
		photoAssertf(0, "CPhotoBuffer::FillInValuesForNewPhoto - didn't expect m_SizeOfAllocatedJsonBuffer to be 0");
		return;
	}

	if (m_SizeOfAllocatedTitleBuffer == 0)
	{
		photoAssertf(0, "CPhotoBuffer::FillInValuesForNewPhoto - didn't expect m_SizeOfAllocatedTitleBuffer to be 0");
		return;
	}

	if (m_SizeOfAllocatedDescriptionBuffer == 0)
	{
		photoAssertf(0, "CPhotoBuffer::FillInValuesForNewPhoto - didn't expect m_SizeOfAllocatedDescriptionBuffer to be 0");
		return;
	}

	u32 offsetToStartOfJsonSection = (m_SizeOfAllocatedJpegBuffer+28);
	u32 offsetToStartOfTitleSection = (offsetToStartOfJsonSection + m_SizeOfAllocatedJsonBuffer + 8);
	u32 offsetToStartOfDescriptionSection = (offsetToStartOfTitleSection + m_SizeOfAllocatedTitleBuffer + 8);


	ResetBufferOffset();

	// 4 bytes - total buffer size
	WriteValueToBuffer(GetSizeOfCompleteBuffer());

	// 4 bytes - the offset (from the start of the buffer) to the start of the JSON data
	WriteValueToBuffer(offsetToStartOfJsonSection);

	// 4 bytes - the offset (from the start of the buffer) to the start of the TITL data
	WriteValueToBuffer(offsetToStartOfTitleSection);

	// 4 bytes - the offset (from the start of the buffer) to the start of the DESC data
	WriteValueToBuffer(offsetToStartOfDescriptionSection);


	// 4 bytes - 'J', 'P', 'E', 'G'
	WriteStringToBuffer("JPEG", 4);

	// 4 bytes - the size of the allocated JPEG buffer
	WriteValueToBuffer(m_SizeOfAllocatedJpegBuffer);

	// 4 bytes containing the exact JPEG size will be filled in here when the screenshot is taken
	ForwardBufferOffset(4);

	// Then the JPEG data
	ForwardBufferOffset(m_SizeOfAllocatedJpegBuffer);


	// JSON section
	// 4 bytes - 'J', 'S', 'O', 'N' (the second 4 bytes points to here)
	WriteStringToBuffer("JSON", 4);

	// 4 bytes - the size of the allocated JSON buffer
	WriteValueToBuffer(m_SizeOfAllocatedJsonBuffer);

	// Then the JSON data
	ForwardBufferOffset(m_SizeOfAllocatedJsonBuffer);


	// Title section
	// 4 bytes - 'T', 'I', 'T', 'L' (the third 4 bytes points to here)
	WriteStringToBuffer("TITL", 4);

	// 4 bytes - the max size of the Title string in bytes
	WriteValueToBuffer(m_SizeOfAllocatedTitleBuffer);

	// Then a buffer of the max size for the Title string
	ForwardBufferOffset(m_SizeOfAllocatedTitleBuffer);


	// Description section
	// 4 bytes - 'D', 'E', 'S', 'C' (the fourth 4 bytes points to here)
	WriteStringToBuffer("DESC", 4);

	// 4 bytes - the max size of the Description string in bytes
	WriteValueToBuffer(m_SizeOfAllocatedDescriptionBuffer);

	// Then a buffer of the max size for the Description string
	ForwardBufferOffset(m_SizeOfAllocatedDescriptionBuffer);


	// 4 bytes - 'J', 'E', 'N', 'D'
	WriteStringToBuffer("JEND", 4);

	VerifyCurrentOffset(GetSizeOfCompleteBuffer(), "End of Buffer");
}



bool CPhotoBuffer::ReadAndVerifyLoadedBuffer()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::ReadAndVerifyLoadedBuffer - m_pDataBuffer hasn't been allocated");
		return false;
	}

	ResetBufferOffset();

	// 4 bytes - total buffer size
	u32 totalBufferSize = ReadValueFromBuffer();

	// 4 bytes - the offset (from the start of the buffer) to the start of the JSON data
	u32 offsetToJsonSection = ReadValueFromBuffer();

	// 4 bytes - the offset (from the start of the buffer) to the start of the TITL data
	u32 offsetToTitleSection = ReadValueFromBuffer();

	// 4 bytes - the offset (from the start of the buffer) to the start of the DESC data
	u32 offsetToDescriptionSection = ReadValueFromBuffer();


	// JPEG section
	// 4 bytes - 'J', 'P', 'E', 'G'
	if (!ReadStringFromBuffer("JPEG", 4))
	{
		return false;
	}

	// 4 bytes - the size of the allocated JPEG buffer
	m_SizeOfAllocatedJpegBuffer = ReadValueFromBuffer();

	// 4 bytes - the exact number of bytes used for the JPEG data
	u32 sizeOfUsedJpegData = ReadValueFromBuffer();

	if (sizeOfUsedJpegData > m_SizeOfAllocatedJpegBuffer)
	{
		photoAssertf(0, "CPhotoBuffer::ReadAndVerifyLoadedBuffer - exact size of JPEG data in file is %u. Expected it to be %u or less", sizeOfUsedJpegData, m_SizeOfAllocatedJpegBuffer);
		return false;
	}

	// Then the JPEG data
	ForwardBufferOffset(m_SizeOfAllocatedJpegBuffer);


	// JSON section
	if (!VerifyCurrentOffset(offsetToJsonSection, "offsetToJsonSection"))
	{
		return false;
	}

	// 4 bytes - 'J', 'S', 'O', 'N' (the second 4 bytes points to here)
	if (!ReadStringFromBuffer("JSON", 4))
	{
		return false;
	}

	// 4 bytes - the size of the allocated JSON buffer
	m_SizeOfAllocatedJsonBuffer = ReadValueFromBuffer();

	// Then the JSON data
	ForwardBufferOffset(m_SizeOfAllocatedJsonBuffer);


	// Title section
	if (!VerifyCurrentOffset(offsetToTitleSection, "offsetToTitleSection"))
	{
		return false;
	}

	// 4 bytes - 'T', 'I', 'T', 'L' (the third 4 bytes points to here)
	if (!ReadStringFromBuffer("TITL", 4))
	{
		return false;
	}

	// 4 bytes - the max size of the Title string in bytes
	m_SizeOfAllocatedTitleBuffer = ReadValueFromBuffer();

	// Then a buffer of the max size for the Title string
	ForwardBufferOffset(m_SizeOfAllocatedTitleBuffer);


	// Description section
	if (!VerifyCurrentOffset(offsetToDescriptionSection, "offsetToDescriptionSection"))
	{
		return false;
	}

	// 4 bytes - 'D', 'E', 'S', 'C' (the fourth 4 bytes points to here)
	if (!ReadStringFromBuffer("DESC", 4))
	{
		return false;
	}

	// 4 bytes - the max size of the Description string in bytes
	m_SizeOfAllocatedDescriptionBuffer = ReadValueFromBuffer();

	// Then a buffer of the max size for the Description string
	ForwardBufferOffset(m_SizeOfAllocatedDescriptionBuffer);


	// 4 bytes - 'J', 'E', 'N', 'D'
	if (!ReadStringFromBuffer("JEND", 4))
	{
		return false;
	}

	if (!VerifyCurrentOffset(totalBufferSize, "End of Buffer"))
	{
		return false;
	}

	photoDisplayf("CPhotoBuffer::ReadAndVerifyLoadedBuffer - succeeded");
	return true;
}


u32 CPhotoBuffer::GetSizeOfAllocatedJsonBuffer()
{
	photoAssertf(m_SizeOfAllocatedJsonBuffer != 0, "CPhotoBuffer::GetSizeOfAllocatedJsonBuffer - didn't expect m_SizeOfAllocatedJsonBuffer to be 0");
	return m_SizeOfAllocatedJsonBuffer;
}

u32 CPhotoBuffer::GetSizeOfAllocatedJpegBuffer()
{
	photoAssertf(m_SizeOfAllocatedJpegBuffer != 0, "CPhotoBuffer::GetSizeOfAllocatedJpegBuffer - didn't expect m_SizeOfAllocatedJpegBuffer to be 0");
	return m_SizeOfAllocatedJpegBuffer;
}

u32 CPhotoBuffer::GetSizeOfCompleteBuffer() const
{
	photoAssertf(m_SizeOfAllocatedJsonBuffer != 0, "CPhotoBuffer::GetSizeOfCompleteBuffer - didn't expect m_SizeOfAllocatedJsonBuffer to be 0");
	photoAssertf(m_SizeOfAllocatedJpegBuffer != 0, "CPhotoBuffer::GetSizeOfCompleteBuffer - didn't expect m_SizeOfAllocatedJpegBuffer to be 0");
	photoAssertf(m_SizeOfAllocatedTitleBuffer != 0, "CPhotoBuffer::GetSizeOfCompleteBuffer - didn't expect m_SizeOfAllocatedTitleBuffer to be 0");
	photoAssertf(m_SizeOfAllocatedDescriptionBuffer != 0, "CPhotoBuffer::GetSizeOfCompleteBuffer - didn't expect m_SizeOfAllocatedDescriptionBuffer to be 0");

	return (m_SizeOfAllocatedJpegBuffer + m_SizeOfAllocatedJsonBuffer + m_SizeOfAllocatedTitleBuffer + m_SizeOfAllocatedDescriptionBuffer + ms_NumberOfExtraBytesInBuffer);
}


u32 *CPhotoBuffer::GetPointerToVariableForStoringExactSizeOfJpegData()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::GetPointerToVariableForStoringExactSizeOfJpegData - m_pDataBuffer hasn't been allocated");
		return NULL;
	}

	return (u32*) &m_pDataBuffer[ms_OffsetToVariableForStoringExactSizeOfJpegData];
}


u32 CPhotoBuffer::GetExactSizeOfJpegData()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::GetExactSizeOfJpegData - m_pDataBuffer hasn't been allocated");
		return 0;
	}

	u32 *pExactSizeOfJpegData = (u32*) &m_pDataBuffer[ms_OffsetToVariableForStoringExactSizeOfJpegData];
	photoAssertf(*pExactSizeOfJpegData != 0, "CPhotoBuffer::GetExactSizeOfJpegData - exact size hasn't been set yet");

	return *pExactSizeOfJpegData;
}


const u8 *CPhotoBuffer::GetCompleteBuffer() const
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::GetCompleteBuffer - m_pDataBuffer hasn't been allocated");
		return NULL;
	}

	return &m_pDataBuffer[0];
}

u8 *CPhotoBuffer::GetCompleteBufferForLoading()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::GetCompleteBufferForLoading - m_pDataBuffer hasn't been allocated");
		return NULL;
	}

	return &m_pDataBuffer[0];
}


u8 *CPhotoBuffer::GetJpegBuffer()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::GetJsonBuffer - m_pDataBuffer hasn't been allocated");
		return NULL;
	}

	return &m_pDataBuffer[ms_OffsetToVariableForStoringExactSizeOfJpegData+4];
}


char *CPhotoBuffer::GetJsonBuffer()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::GetJsonBuffer - m_pDataBuffer hasn't been allocated");
		return NULL;
	}

	u32 *pOffsetToJsonSection = (u32*) &m_pDataBuffer[4];

	//	Have to step forward 8 bytes to skip the 'J', 'S', 'O', 'N' marker and the u32 for the size of the allocated JSON buffer
	return (char*) &m_pDataBuffer[*pOffsetToJsonSection + 8];
}

char *CPhotoBuffer::GetTitleBuffer()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::GetTitleBuffer - m_pDataBuffer hasn't been allocated");
		return NULL;
	}

	u32 *pOffsetToTitleSection = (u32*) &m_pDataBuffer[8];

	//	Have to step forward 8 bytes to skip the 'T', 'I', 'T', 'L' marker and the u32 for the size of the allocated Title buffer
	return (char*) &m_pDataBuffer[*pOffsetToTitleSection + 8];
}

char *CPhotoBuffer::GetDescriptionBuffer()
{
	if (m_pDataBuffer == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::GetDescriptionBuffer - m_pDataBuffer hasn't been allocated");
		return NULL;
	}

	u32 *pOffsetToDescriptionSection = (u32*) &m_pDataBuffer[12];

	//	Have to step forward 8 bytes to skip the 'D', 'E', 'S', 'C' marker and the u32 for the size of the allocated Description buffer
	return (char*) &m_pDataBuffer[*pOffsetToDescriptionSection + 8];
}


bool CPhotoBuffer::AllocateAndFillBufferWhenLoadingCloudPhoto(u8 *pData, u32 sizeOfData)
{
	if (pData == NULL)
	{
		photoAssertf(0, "CPhotoBuffer::AllocateAndFillBufferWhenLoadingCloudPhoto - pData is NULL");
		return false;
	}

	if ((sizeOfData == 0) || (sizeOfData > ms_DefaultSizeOfJpegBuffer))
	{
		photoAssertf(0, "CPhotoBuffer::AllocateAndFillBufferWhenLoadingCloudPhoto - expected buffer size to be > 0 and <= %u but it's %u", ms_DefaultSizeOfJpegBuffer, sizeOfData);
		return false;
	}

	if (BufferIsAllocated())
	{
		photoAssertf(0, "CPhotoBuffer::AllocateAndFillBufferWhenLoadingCloudPhoto - expected JPEG buffer to be NULL at this point");
		FreeBuffer();
	}

//	I think AllocateAndFillBufferWhenLoadingCloudPhoto gets called when loading a jpeg from the cloud
//	so allocate a complete buffer and then write to the jpeg portion
//	And then fill in the exact size?

	//	AllocateJpegBuffer(sizeOfData);

	//	Not sure if I need to set this
	//	m_bLastScreenShotStored = false;

//	AllocateBufferForLoading or AllocateBuffer? Neither of them adds the buffer for json and extra fields
	if (AllocateBufferForTakingScreenshot(sizeOfData))
	{
// #if __BANK
// 		m_bPhotoMemoryIsAllocated = true;
// #endif	//	__BANK

//		m_ExactSizeOfJpegData = sizeOfData;
		u32 *pExactSizeField = GetPointerToVariableForStoringExactSizeOfJpegData();
		if (photoVerifyf(pExactSizeField, "CPhotoBuffer::AllocateAndFillBufferWhenLoadingCloudPhoto - failed to get pointer to the field for storing the exact JPEG size"))
		{
			*pExactSizeField = sizeOfData;
			photoDisplayf("CPhotoBuffer::AllocateAndFillBufferWhenLoadingCloudPhoto - exact size field has been set to %u", *pExactSizeField);
		}

		sysMemCpy(GetJpegBuffer(), pData, sizeOfData);
		return true;
	}

	photoAssertf(0, "CPhotoBuffer::AllocateAndFillBufferWhenLoadingCloudPhoto - failed to allocate JPEG Buffer");
	return false;
}

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
bool CPhotoBuffer::ReAllocateMemoryForIncreasedJpegSize(u32 sizeOfNewJpegBuffer)
{
	FreeBuffer();

	if (AllocateBufferForTakingScreenshot(sizeOfNewJpegBuffer))
	{
		return true;
	}

// 	if (JpegBufferIsAllocated())
// 	{
// #if __BANK
// 		m_bPhotoMemoryIsAllocated = true;
// #endif	//	__BANK
// 
// 		return true;
// 	}

	//	Allocation failed. It's up to the caller whether to try again.
	return false;
}
#endif	//	__WIN32PC || RSG_DURANGO || RSG_ORBIS

bool CPhotoBuffer::SetMetadataString(const CSavegamePhotoMetadata &metadata)
{
	char *pJsonBuffer = GetJsonBuffer();

	if (pJsonBuffer)
	{
		return metadata.WriteMetadataToString(pJsonBuffer, m_SizeOfAllocatedJsonBuffer, false);
	}

	return false;	
}

bool CPhotoBuffer::CorrectAkamaiTag()
{
	const bool wantedAkamai = CSavegamePhotoMetadata::IsAkamaiEnabled();
	unsigned backupSize = 0;
	char* backup = nullptr;
	char* jsonBuffer = nullptr;

	rtry
	{
		jsonBuffer = GetJsonBuffer();
		rverifyall(jsonBuffer);
		backupSize = (unsigned)strlen(jsonBuffer) + 1;

		CSavegamePhotoMetadata meta;
		rverifyall(meta.ReadMetadataFromString(jsonBuffer));

		rcheck(meta.GetAkamai() != wantedAkamai, tagok, );

		photoDebugf1("The akamai tag does not match. Changing from %s to %s", LogBool(meta.GetAkamai()), LogBool(wantedAkamai));

		backup = (char*)alloca(backupSize);
		rverify(backup != nullptr, catchall, photoErrorf("Failed to allocate %u bytes for backup buffer", backupSize));
		sysMemSet(backup, 0, backupSize);
		safecpy(backup, jsonBuffer, backupSize);

		meta.SetAkamai(wantedAkamai);

		// Needed to load the buffer sizes which are needed in SetMetadataString
		rverify(ReadAndVerifyLoadedBuffer(), writefail, photoErrorf("ReadAndVerifyLoadedBuffer failed"));

		rverify(SetMetadataString(meta), writefail, photoErrorf("Failed to update the akamai tag"));

		return true;
	}
	rcatch(writefail)
	{
		if (photoVerifyf(backupSize > 0 && backup != nullptr && jsonBuffer != nullptr, "Unexpected, buffer should not be null. The meta may be garbage at this point"))
		{
			sysMemCpy(jsonBuffer, backup, backupSize);
		}

		return false;
	}
	rcatch(tagok)
	{
		return true;
	}
	rcatchall
	{
	}

	return false;
}

bool CPhotoBuffer::SetTitleString(const char *pTitle)
{
	char *pTitleBuffer = GetTitleBuffer();

	if (pTitleBuffer)
	{
		if (photoVerifyf(strlen(pTitle) < m_SizeOfAllocatedTitleBuffer, "CPhotoBuffer::SetTitleString - length of string is %d. Maximum size of string is %d", (int) strlen(pTitle), m_SizeOfAllocatedTitleBuffer))
		{
			safecpy(pTitleBuffer, pTitle, m_SizeOfAllocatedTitleBuffer);
			return true;
		}
	}

	return false;
}

const char *CPhotoBuffer::GetTitleString()
{
	char *pTitleBuffer = GetTitleBuffer();

	if (pTitleBuffer)
	{
		return pTitleBuffer;
	}

	return "";
}

bool CPhotoBuffer::SetDescriptionString(const char *pDescription)
{
	char *pDescBuffer = GetDescriptionBuffer();

	if (pDescBuffer)
	{
		if (photoVerifyf(strlen(pDescription) < m_SizeOfAllocatedDescriptionBuffer, "CPhotoBuffer::SetDescriptionString - length of string is %d. Maximum size of string is %d", (int) strlen(pDescription), m_SizeOfAllocatedDescriptionBuffer))
		{
			safecpy(pDescBuffer, pDescription, m_SizeOfAllocatedDescriptionBuffer);
			return true;
		}
	}

	return false;
}

const char *CPhotoBuffer::GetDescriptionString()
{
	char *pDescBuffer = GetDescriptionBuffer();

	if (pDescBuffer)
	{
		return pDescBuffer;
	}

	return "";
}

