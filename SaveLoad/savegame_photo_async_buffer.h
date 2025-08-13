/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CSavegamePhotoAsyncBuffer.h
// PURPOSE : Class that represents a buffer used for asynchronous photo saving
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef SAVEGAME_PHOTO_ASYNC_BUFFER_H
#define SAVEGAME_PHOTO_ASYNC_BUFFER_H

// rage
#include "system/memory.h"

class CPhotoManager;

class CSavegamePhotoAsyncBuffer
{
public: // methods
	CSavegamePhotoAsyncBuffer();

	CSavegamePhotoAsyncBuffer( CSavegamePhotoAsyncBuffer const& other );
	CSavegamePhotoAsyncBuffer& operator=(const CSavegamePhotoAsyncBuffer& rhs);

	~CSavegamePhotoAsyncBuffer();

	bool Initialize( rage::u32 const width, rage::u32 const height );
	inline bool IsInitialized() const { return m_buffer != NULL && m_width > 0 && m_height > 0; }
	void Shutdown();

	inline rage::u8* GetRawBuffer() { return m_buffer; }
	size_t GetBufferSize() const;

	void SetPopulated( bool const populated ) { m_populated = populated; }
	inline bool IsPopulated() const { return m_populated; }

	inline rage::u32 GetWidth() const { return m_width; }
	inline rage::u32 GetHeight() const { return m_height; }
	inline rage::u32 GetPitch() const { return (u32)(GetBufferSize() / GetHeight()); }

	static size_t CalculateBufferSize( rage::u32 const width, rage::u32 const height );

private: // declarations and variables
	rage::u8*	m_buffer;
	rage::u32	m_width;
	rage::u32   m_height;

	bool		m_populated;

private: // methods
	void CopyFromOther( CSavegamePhotoAsyncBuffer const& other );
};

#endif	//	SAVEGAME_PHOTO_ASYNC_BUFFER_H
