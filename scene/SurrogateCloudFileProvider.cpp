/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SurrogateCloudFileProvider.cpp
// PURPOSE : Handles loading local files and acting as a surrogate for passing them
//			 to systems expecting files from the cloud
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

// rage
#include "file/asset.h"
#include "file/stream.h"
#include "net/http.h"

// Game
#include "scene_channel.h"
#include "SurrogateCloudFileProvider.h"

SCENE_OPTIMISATIONS();

CSurrogateCloudFileProvider::CSurrogateCloudFileProvider()
	: m_listeners()
	, m_requests()
	, m_maxFileSize( 0 )
{

}

CSurrogateCloudFileProvider::~CSurrogateCloudFileProvider()
{
	Shutdown();
}

bool CSurrogateCloudFileProvider::Initialize( size_t const maxFileSizeAllowed )
{
	Assertf( maxFileSizeAllowed > 0, "CSurrogateCloudFileProvider::Initialize - Zero-sized maximum file means nothing can be loaded" );
	m_maxFileSize = maxFileSizeAllowed;
	return IsInitialized();
}

bool CSurrogateCloudFileProvider::IsInitialized() const
{
	return m_maxFileSize > 0;
}

void CSurrogateCloudFileProvider::Shutdown()
{
	DiscardAllRequests();
	RemoveAllListeners();
	m_maxFileSize = 0;
}

bool CSurrogateCloudFileProvider::AddListener( CloudListener* listener )
{
	bool success = false;

	if( m_listeners.Find( listener ) == -1 )
	{
		m_listeners.PushAndGrow( listener );
		success = true;
	}
	else
	{
		sceneWarningf( "CSurrogateCloudFileProvider::AddListener - Attempting to double-add listner. Logic error?" );
	}

	return success;
}

bool CSurrogateCloudFileProvider::AddListener( CloudListener& listener )
{
	return AddListener( &listener );
}

void CSurrogateCloudFileProvider::RemoveListener( CloudListener * listener )
{
	int const index = m_listeners.Find( listener );
	if( index >= 0 )
	{
		CloudListenerCollection::iterator itr = m_listeners.begin() + index;
		m_listeners.erase( itr );
	}
}

void CSurrogateCloudFileProvider::RemoveListener( CloudListener & listener )
{
	RemoveListener( &listener );
}

CloudRequestID CSurrogateCloudFileProvider::RequestLocalFile( char const * const path, char const * const requestName )
{
	CloudRequestID result = -1;

	if( Verifyf( path, "CSurrogateCloudFileProvider::RequestLocalFile - NULL path specified" ) &&
		Verifyf( requestName, "CSurrogateCloudFileProvider::RequestLocalFile - NULL name specified" ) )
	{
		CloudRequestID pendingRequestId = atStringHash( requestName );

		result = IsLocalFileRequested( requestName ) ? pendingRequestId :
				AddRequest( path, pendingRequestId ) ? pendingRequestId : result;
	}

	return result;
}

bool CSurrogateCloudFileProvider::IsLocalFileRequested( char const * const requestName )
{
	bool const c_result = GetRequestIndex( requestName ) >= 0;
	return c_result;
}

bool CSurrogateCloudFileProvider::IsLocalFileRequested( CloudRequestID const requestId )
{
	bool const c_result = GetRequestIndex( requestId ) >= 0;
	return c_result;
}

void CSurrogateCloudFileProvider::ReleaseLocalFileRequest( char const * const requestName )
{
	if( Verifyf( requestName, "CSurrogateCloudFileProvider::ReleaseLocalFileRequest - NULL name specified" ) )
	{
		s32 const c_requestIndex = GetRequestIndex( requestName );
		if( c_requestIndex >= 0 )
		{
			DiscardRequest( (u32)c_requestIndex );
		}	
	}
}

void CSurrogateCloudFileProvider::ReleaseLocalFileRequest( CloudRequestID const requestId )
{
	s32 const c_requestIndex = GetRequestIndex( requestId );
	if( c_requestIndex >= 0 )
	{
		DiscardRequest( (u32)c_requestIndex );
	}	
}

void CSurrogateCloudFileProvider::Update()
{
	if( !IsInitialized() || m_listeners.size() == 0 )
		return;

	// TODO - Throttle this value if we find we are requesting too many files in a single frame
	size_t const c_perFrameFileThreshold = m_requests.size();

	for( size_t index = 0; index < c_perFrameFileThreshold; ++index )
	{
		FileRequest const* request = GetRequest( 0 );
		if( request )
		{
			ProcessRequest( *request );
		}

		DiscardRequest( 0 );
	}
}

void CSurrogateCloudFileProvider::RemoveAllListeners()
{
	m_listeners.clear();
}

bool CSurrogateCloudFileProvider::AddRequest( char const * const path, CloudRequestID const requestId )
{
	bool success = false;

	FileRequest* newRequest = rage_new FileRequest();
	if( Verifyf( newRequest, "CSurrogateCloudFileProvider::AddRequest - Unable to allocate file request" ) )
	{
		newRequest->m_path.Set( path );
		newRequest->m_requestId = requestId;
		m_requests.PushAndGrow( newRequest );

		success = true;
	}

	return success;
}

CSurrogateCloudFileProvider::FileRequest const* CSurrogateCloudFileProvider::GetRequest( u32 const index ) const
{
	bool const c_validIndex =  index < m_requests.size();
	FileRequest* request = c_validIndex ? m_requests[ index ] : NULL;
	return request;
}

s32 CSurrogateCloudFileProvider::GetRequestIndex( char const * const requestName ) const
{
	s32 result = -1;

	if( Verifyf( requestName, "CSurrogateCloudFileProvider::GetRequestIndex - NULL name specified" ) )
	{
		CloudRequestID pendingRequestId = atStringHash( requestName );
		result = GetRequestIndex( pendingRequestId );
	}

	return result;
}

s32 CSurrogateCloudFileProvider::GetRequestIndex( CloudRequestID const requestId ) const
{
	s32 result = -1;

	int const c_requestCount = m_requests.GetCount();
	for( int index = 0; index < c_requestCount; ++index )
	{
		FileRequest const* request = GetRequest( index );
		if( request && request->m_requestId == requestId )
		{
			result = index;
			break;
		}
	}

	return result;
}

void CSurrogateCloudFileProvider::ProcessRequest( FileRequest const& request )
{
	int const c_listenerCount = m_listeners.GetCount();
	if( c_listenerCount > 0 )
	{
		char const * const filePath = request.m_path.getBuffer();
		bool failed = true;

		fiStream* fileStream = ASSET.Open( filePath, "", true );
		if( fileStream )
		{
			int const c_fileSize = fileStream->Size();

			if( c_fileSize <= m_maxFileSize )
			{
				u8* fileBuffer = rage_new u8[ c_fileSize ];
				if( Verifyf( fileBuffer, "CSurrogateCloudFileProvider::ProcessRequest - Unable to allocate memory buffer for file" ) )
				{
					int const c_bytesRead = fileStream->Read( fileBuffer, c_fileSize );
					if( Verifyf( c_bytesRead == c_fileSize, "CSurrogateCloudFileProvider::ProcessRequest - Attempted to read %d bytes, only got %d", c_fileSize, c_bytesRead ) )
					{
						CloudEvent dummyEvent;
						dummyEvent.OnRequestFinished( request.m_requestId, true, NET_HTTPSTATUS_OK, fileBuffer, (unsigned)c_bytesRead, filePath, 0, false );

						Displayf( "CSurrogateCloudFileProvider::ProcessRequest - Processing request %d, for file %s", request.m_requestId, filePath );

						for( int index = 0; index < c_listenerCount; ++index )
						{
							CloudListener* listener = m_listeners[ index ];
							if( listener )
							{
								listener->OnCloudEvent( &dummyEvent );
							}
						}

						failed = false;
					}

					delete[] fileBuffer;
				}
			}

			fileStream->Close();
		}

		if( failed )
		{
			CloudEvent dummyEvent;
			dummyEvent.OnRequestFinished( request.m_requestId, false, NET_HTTPSTATUS_NOT_FOUND, NULL, 0, filePath, 0, false );

			for( int index = 0; index < c_listenerCount; ++index )
			{
				CloudListener* listener = m_listeners[ index ];
				if( listener )
				{
					listener->OnCloudEvent( &dummyEvent );
				}
			}
		}
	}
}

void CSurrogateCloudFileProvider::DiscardRequest( u32 const index )
{
	bool const c_validIndex =  index < m_requests.size();

	//! Get the request and destroy it
	CSurrogateCloudFileProvider::FileRequest* request = c_validIndex ? m_requests[ index ] : NULL;

	//! Remove the request from the queue
	if( c_validIndex )
	{		
		m_requests.erase( m_requests.begin() + index );
	}

	if( request )
	{
		delete request;
	}
}

void CSurrogateCloudFileProvider::DiscardAllRequests()
{
	while( m_requests.size() > 0 )
	{
		DiscardRequest( 0 );
	}

	m_requests.clear();
}
