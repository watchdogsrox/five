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
#include "SaveLoad/savegame_photo_async_buffer.h"

// framework
#include "streaming/streamingengine.h"

// game
#include "renderer/rendertargets.h"
#include "SaveLoad/savegame_channel.h"
#include "Frontend/ui_channel.h"

CSavegamePhotoAsyncBuffer::CSavegamePhotoAsyncBuffer()
	: m_buffer( NULL )
	, m_width( 0 )
	, m_height( 0 )
	, m_populated( false )
{

}

CSavegamePhotoAsyncBuffer::CSavegamePhotoAsyncBuffer( CSavegamePhotoAsyncBuffer const& other )
	: m_buffer( NULL )
	, m_width( 0 )
	, m_height( 0 )
	, m_populated( false )
{
	CopyFromOther( other );
}

CSavegamePhotoAsyncBuffer& CSavegamePhotoAsyncBuffer::operator=( const CSavegamePhotoAsyncBuffer& rhs )
{
	if (this != &rhs)
	{
		CopyFromOther( rhs );
	}

	return *this;
}

CSavegamePhotoAsyncBuffer::~CSavegamePhotoAsyncBuffer()
{
	Shutdown();
}

bool CSavegamePhotoAsyncBuffer::Initialize( rage::u32 width, rage::u32 height )
{
	bool success = false;

	if( Verifyf( width > 0 && height > 0, "CSavegamePhotoAsyncBuffer::Initialize - Invalid photo width and height!" ) )
	{
		u32 actualWidth = width;
		u32 actualHeight = height;

		VideoResManager::ConstrainScreenshotDimensions( actualWidth, actualHeight );
		size_t const sizeNeeded = CalculateBufferSize( actualWidth, actualHeight );

		MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);
		m_buffer = (u8*) strStreamingEngine::GetAllocator().Allocate( sizeNeeded, 16, MEMTYPE_RESOURCE_VIRTUAL );
		if ( Verifyf( m_buffer, "CSavegamePhotoAsyncBuffer::Initialize - Unable to allocate buffer of size %" SIZETFMT "u", sizeNeeded ) )
		{
			photoDisplayf("CSavegamePhotoAsyncBuffer::Initialize - allocated %" SIZETFMT "u bytes", sizeNeeded );
			m_width = actualWidth;
			m_height = actualHeight;
			m_populated = false;
			success = true;
		}
	}

	return success;
}

void CSavegamePhotoAsyncBuffer::Shutdown()
{
	if ( m_buffer )
	{
		MEM_USE_USERDATA(MEMUSERDATA_SCREENSHOT);

		strStreamingEngine::GetAllocator().Free(m_buffer);
		m_buffer = NULL;

		photoDisplayf("CSavegamePhotoAsyncBuffer::Shutdown - buffer has been freed");
	}

	m_width = 0;
	m_height = 0;
	m_populated = false;
}

size_t CSavegamePhotoAsyncBuffer::GetBufferSize() const
{
	return CalculateBufferSize( m_width, m_height );
}

size_t CSavegamePhotoAsyncBuffer::CalculateBufferSize( rage::u32 const width, rage::u32 const height )
{
	u32 actualWidth = width;
	u32 actualHeight = height;

	VideoResManager::ConstrainScreenshotDimensions( actualWidth, actualHeight );
	return actualWidth * actualHeight * 4;
}

void CSavegamePhotoAsyncBuffer::CopyFromOther( CSavegamePhotoAsyncBuffer const& other )
{
	if( IsInitialized() )
	{
		Shutdown();
	}

	if( other.IsInitialized() )
	{
		Initialize( other.GetWidth(), other.GetHeight() );
		if( uiVerifyf( IsInitialized(), "CSavegamePhotoAsyncBuffer::CopyFromOther - Unable to initialize!" ) )
		{
			m_populated = other.m_populated;
			size_t const c_bufferSize = GetBufferSize();
			sysMemCpy( m_buffer, other.m_buffer, c_bufferSize );
		}
	}
}
