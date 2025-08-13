/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : SurrogateCloudFileProvider.h
// PURPOSE : Handles loading local files and acting as a surrogate for passing them
//			 to systems expecting files from the cloud
//
// NOTES   : File loads are currently handled synchronously on the update thread, since this
//			 is only used in a limited use scenario. Consider making asynchronous if expanding usage.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

//! TODO - Might be better placed in the network project, or in some interim location
#ifndef SCENE_SURROGATE_CLOUD_FILE_PROVIDER_H
#define SCENE_SURROGATE_CLOUD_FILE_PROVIDER_H

// Rage
#include "atl/hashstring.h"
#include "atl/array.h"

// Framework
#include "fwlocalisation/templateString.h"

// Game
#include "Network/Cloud/CloudManager.h"

class CloudListener;

class CSurrogateCloudFileProvider
{
public:
	CSurrogateCloudFileProvider();
	~CSurrogateCloudFileProvider();

	bool Initialize( size_t const maxFileSizeAllowed );
	bool IsInitialized() const;
	void Shutdown();

	//! Interface for registering listeners
	bool AddListener( CloudListener* listener );
	bool AddListener( CloudListener& listener );
	void RemoveListener( CloudListener * listener );
	void RemoveListener( CloudListener & listener );

	//! Interface for actual file requesting and loading
	CloudRequestID RequestLocalFile( char const * const path, char const * const requestName );
	bool IsLocalFileRequested( char const * const requestName );
	bool IsLocalFileRequested( CloudRequestID const requestId );
	void ReleaseLocalFileRequest( char const * const requestName );
	void ReleaseLocalFileRequest( CloudRequestID const requestId );

	void Update();

private: // declarations and variables
	typedef atArray<CloudListener*> CloudListenerCollection;
	CloudListenerCollection m_listeners;

	struct FileRequest
	{
		CloudRequestID	m_requestId;
		PathBuffer		m_path;
	};

	typedef atArray<FileRequest*> FileRequestCollection;
	FileRequestCollection	m_requests;
	size_t					m_maxFileSize;

private: // methods

	void RemoveAllListeners();

	bool AddRequest( char const * const path, CloudRequestID const requestId );

	FileRequest const* GetRequest( u32 const index ) const;
	s32 GetRequestIndex( char const * const requestName ) const;
	s32 GetRequestIndex(  CloudRequestID const requestId ) const;

	void ProcessRequest( FileRequest const& request );

	void DiscardRequest( u32 const index );
	void DiscardAllRequests();
};

#endif // SCENE_SURROGATE_CLOUD_FILE_PROVIDER_H
