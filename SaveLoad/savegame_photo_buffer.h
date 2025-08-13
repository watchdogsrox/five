
#ifndef SAVEGAME_PHOTO_BUFFER_H
#define SAVEGAME_PHOTO_BUFFER_H

// Forward declarations
class CSavegamePhotoMetadata;


//	Local photos are saved as a single file. So I need a continuous buffer containing the JPEG and JSON data.
//		4 bytes - total buffer size
//		4 bytes - the offset (from the start of the buffer) to the start of the JSON data
//		4 bytes - the offset (from the start of the buffer) to the start of the TITL data
//		4 bytes - the offset (from the start of the buffer) to the start of the DESC data

//		4 bytes - 'J', 'P', 'E', 'G'
//		4 bytes - the size of the allocated JPEG buffer
//		4 bytes - the exact number of bytes used for the JPEG data
//		Then the JPEG data

//		4 bytes - 'J', 'S', 'O', 'N' (the second 4 bytes points to here)
//		4 bytes - the size of the allocated JSON buffer
//		Then the JSON data

//		4 bytes - 'T', 'I', 'T', 'L' (the third 4 bytes points to here)
//		4 bytes - the max size of the Title string in bytes
//		Then a buffer of the max size for the Title string

//		4 bytes - 'D', 'E', 'S', 'C' (the fourth 4 bytes points to here)
//		4 bytes - the max size of the Description string in bytes
//		Then a buffer of the max size for the Description string

//		4 bytes - 'J', 'E', 'N', 'D'
//	The cloud photos will be saved as a JPEG file and a JSON file. I'll just use the two sections of the continuous buffer to create the two files.
class CPhotoBuffer
{
public:
	CPhotoBuffer();
	~CPhotoBuffer();

	bool AllocateBufferForLoadingLocalPhoto(u32 sizeOfCompletePhotoBuffer);
	bool AllocateAndFillBufferWhenLoadingCloudPhoto(u8 *pData, u32 sizeOfData);

	bool AllocateBufferForTakingScreenshot(u32 sizeOfJpegBuffer = ms_DefaultSizeOfJpegBuffer, u32 SizeOfJsonBuffer = ms_DefaultSizeOfJsonBuffer);

#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	bool ReAllocateMemoryForIncreasedJpegSize(u32 sizeOfNewJpegBuffer);
#endif	//	__WIN32PC || RSG_DURANGO || RSG_ORBIS

	void FreeBuffer();

	bool BufferIsAllocated() { return (m_pDataBuffer != NULL); }

	// Verifies the contents of the loaded local photo file. Also sets m_SizeOfAllocatedJpegBuffer, m_SizeOfAllocatedJsonBuffer, m_SizeOfAllocatedTitleBuffer and m_SizeOfAllocatedDescriptionBuffer
	bool ReadAndVerifyLoadedBuffer();

	const u8 *GetCompleteBuffer() const;
	u32 GetSizeOfCompleteBuffer() const;
	static u32 GetDefaultSizeOfCompleteBuffer() { return ms_DefaultSizeOfCompleteBuffer; }
	u8 *GetCompleteBufferForLoading();

	u8 *GetJpegBuffer();
	u32 GetSizeOfAllocatedJpegBuffer();
	static u32 GetDefaultSizeOfJpegBuffer() { return ms_DefaultSizeOfJpegBuffer; }

	//	Returns the number of bytes used to store the JPEG data. This has to be <= the size returned by GetSizeOfAllocatedJpegBuffer()
	u32 GetExactSizeOfJpegData();

	//	This is called when the screenshot is taken to update the exact size of the newly created JPEG
	u32 *GetPointerToVariableForStoringExactSizeOfJpegData();

	char *GetJsonBuffer();
	u32 GetSizeOfAllocatedJsonBuffer();
	static u32 GetDefaultSizeOfJsonBuffer() { return ms_DefaultSizeOfJsonBuffer; }

	bool SetMetadataString(const CSavegamePhotoMetadata &metadata);
	bool CorrectAkamaiTag();

	bool SetTitleString(const char *pTitle);
	const char *GetTitleString();

	bool SetDescriptionString(const char *pDescription);
	const char *GetDescriptionString();

private:

	bool AllocateBuffer(u32 sizeOfCompletePhotoBuffer);

	void FillInValuesForNewPhoto();

	void ResetBufferOffset();
	void ForwardBufferOffset(u32 forwardBy);
	void WriteValueToBuffer(u32 valueToWrite);
	u32 ReadValueFromBuffer();
	void WriteStringToBuffer(const char *pStringToWrite, const u32 numberOfCharacters);
	bool ReadStringFromBuffer(const char *pStringToRead, const u32 numberOfCharacters);
	bool VerifyCurrentOffset(u32 expectedOffset, const char* OUTPUT_ONLY(nameOfOffset));

	char *GetTitleBuffer();
	char *GetDescriptionBuffer();

private:

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	// give next gen more memory due to higher res.
	static const u32 ms_DefaultSizeOfJpegBuffer = (512 * 1024);
#else
	//	100K for jpeg
	static const u32 ms_DefaultSizeOfJpegBuffer = (100 * 1024);
#endif
	//	+ 3K for json string
	static const u32 ms_DefaultSizeOfJsonBuffer = (3 * 1024);
	
	static const u32 ms_DefaultSizeOfTitleBuffer = 256;
	static const u32 ms_DefaultSizeOfDescriptionBuffer = 256;

	static const u32 ms_NumberOfExtraBytesInBuffer = 56;	//	For sizes, offsets and markers
	static const u32 ms_DefaultSizeOfCompleteBuffer = (ms_DefaultSizeOfJpegBuffer + ms_DefaultSizeOfJsonBuffer + ms_DefaultSizeOfTitleBuffer + ms_DefaultSizeOfDescriptionBuffer + ms_NumberOfExtraBytesInBuffer);

	static const u32 ms_OffsetToVariableForStoringExactSizeOfJpegData = 24;

	u32 m_SizeOfAllocatedJpegBuffer;
	u32 m_SizeOfAllocatedJsonBuffer;
	u32 m_SizeOfAllocatedTitleBuffer;
	u32 m_SizeOfAllocatedDescriptionBuffer;

	u32 m_BufferOffset;

	u8 *m_pDataBuffer;
};


#endif	//	SAVEGAME_PHOTO_BUFFER_H




