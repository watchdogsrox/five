
#ifndef SCENE_DOWNLOADABLE_TEXTURE_MANAGER_H
#define SCENE_DOWNLOADABLE_TEXTURE_MANAGER_H

// Rage
#include "atl/hashstring.h"
#include "atl/map.h"
#include "atl/vector.h"
#include "paging/dictionary.h"
#include "rline/cloud/rlcloud.h"
#include "rline/ugc/rlugccommon.h"
#include "system/bit.h"

// Framework
#include "fwscene/stores/storeslotmanager.h"
#include "fwscene/stores/txdstore.h"
#include "fwtl/LinkList.h"

// Game
#include "Network/Cloud/CloudManager.h"
#include "network/NetworkInterface.h"
#include "renderer/rendertargets.h"
#include "SurrogateCloudFileProvider.h"

namespace rage
{
	class grcTexture;
}

// Helper class to manage TXDs that have been added to the TXD store;
// the streaming system won't know about these, so we need to keep track
// of them and release them when no longer needed
class CDownloadableTextureMemoryManager
{

public:

	CDownloadableTextureMemoryManager() : m_pTxdSlotManager(NULL) {};

	void	Init();
	void	Shutdown();

	int		AddTxdSlot(atFinalHashString txdAndTextureHash, atHashString cloudFileHash, grcTexture* pTexture, void* pTextureMemory);

	bool	SlotIsRegistered(s32 txdSlot, atHashString& cloudFileHash, bool& imageContentChanged) const;

	bool	ImageContentChanged(atHashString cloudFileHash);

	void	Update();

#if __BANK
	void	DumpManagedTxdList(bool toScreen = false) const;
#endif

private:

	struct ManagedTxdSlotEntry 
	{
		ManagedTxdSlotEntry() : m_TxdSlot(-1), m_pBuffer(NULL), m_ImageContentChanged(false) {};

		void Init(int txdSlot, void* pBuffer, atHashString cloudFileHash OUTPUT_ONLY(, const atFinalHashString textureName))
		{
			m_TxdSlot = txdSlot;
			m_pBuffer = pBuffer;
			m_CloudFileHash = cloudFileHash;
			OUTPUT_ONLY(m_TextureName = textureName);
			m_ImageContentChanged = false;
		}

		strLocalIndex	m_TxdSlot;
		void*	m_pBuffer;
		atHashString m_CloudFileHash;
		OUTPUT_ONLY(atFinalHashString m_TextureName);
		
		// Set to true when the image on disk/cloud is no longer the image we have
		// loaded in memory.
		bool m_ImageContentChanged;
	};

	fwStoreSlotManager<fwTxd, fwTxdDef>*						m_pTxdSlotManager;
	fwLinkList<ManagedTxdSlotEntry>								m_ManagedTxdSlots;
	typedef fwLink<ManagedTxdSlotEntry>							ManagedTxdSlotNode;

	static const int	MAX_MANAGED_TEXTURES = 128;
};


typedef int TextureDownloadRequestHandle;

// Descriptor filled in by users of the DownloadableTextureManager to make a texture
// download request
struct CTextureDownloadRequestDesc
{
	CTextureDownloadRequestDesc();

	enum Type 
	{
		INVALID,
		MEMBER_FILE,
		UGC_FILE,
		CREW_FILE,
		TITLE_FILE,
		GLOBAL_FILE,
		DISK_FILE,
		WWW_FILE
	};

	atFinalHashString					m_TxtAndTextureHash;	// Name for the texture - if ignored m_CloudFilePath will be used
	Type								m_Type;					// File scope in the cloud, required to process the request via the appropriate API
	const char*							m_CloudFilePath;		// Path to the file in the cloud (Content ID when UGC)
	u32									m_BufferPresize;		// Size for the buffer where the downloaded data will be stored
	int									m_GamerIndex;			// Local gamer index - can be ignored for any Type other than MEMBER_FILE or CREW_FILE
	unsigned							m_CloudRequestFlags;	// Flags to pass when requesting cloud file
	bool								m_OnlyCacheAndVerify;	// When true we only download and cache the image but we don't create the texture
	rlCloudMemberId 					m_CloudMemberId;		// Cloud member id - can be ignored for any Type other than MEMBER_FILE or CREW_FILE
	rlCloudOnlineService				m_CloudOnlineService;	// Cloud Online Service - usually can be left as default of RL_CLOUD_ONLINE_SERVICE_NATIVE
	VideoResManager::eDownscaleFactor	m_JPEGScalingFactor;	// JPEG-only: allows specifying a scaling factor - i.e. 1/m_JPEGScalingDenom - for the texture that will be
																//  created from JPEG data. 
	bool								m_JPEGEncodeAsDXT;		// JPEG-only: allows specifying whether the JPEG should be encoded as a DXT1 texture, rather than uncompressed RGBA8.
    
    // UGC specific
    int                 m_nFileID;          // File ID for the UGC content
    int                 m_nFileVersion;     // Version for the UGC content
    rlScLanguage        m_nLanguage;        // Language for the UGC content
}; 

// Descriptor filled in by users of the DownloadableTextureManager to make a texture
// decoding request from a blob of data also provided by the user
struct CTextureDecodeRequestDesc
{
	CTextureDecodeRequestDesc();

	enum Type 
	{
		UNKNOWN,
		JPEG_BUFFER,
		DDS_BUFFER,
	};

	atFinalHashString					m_TxtAndTextureHash;	// Name for the texture - cannot be null
	Type								m_Type;					// Type of file in the source buffer (JPEG_BUFFER or  DDS_BUFFER)
	void*								m_BufferPtr;			// Buffer with the data blob (memory is owned by the user and must guarantee it will be valid until the request completes).
	u32									m_BufferSize;			// Size for the buffer
	VideoResManager::eDownscaleFactor	m_JPEGScalingFactor;	// JPEG-only: allows specifying a scaling factor - i.e. 1/m_JPEGScalingDenom - for the texture that will be
																//  created from JPEG data. 
	bool								m_JPEGEncodeAsDXT;		// JPEG-only: allows specifying whether the JPEG should be encoded as a DXT1 texture, rather than uncompressed RGBA8.
}; 


// Internal structure used by the DownloadableTextureManager to keep track of ongoing requests
#define IN_PROCESS_FLAG_ACQUIRED	(1U)
#define IN_PROCESS_FLAG_FREE		(0U)

struct CTextureDownloadRequest 
{
	CTextureDownloadRequest();
	void Reset();

	enum Status
	{
		TRY_AGAIN_LATER,		// There are no free request entries to process the download (try again later - no actual request was issued)
		REQUEST_FAILED,			// The request has failed (no actual request was issued)

		IN_PROGRESS,			// This request is being handled by the CloudManager
		WAITING_ON_DECODING,	// The texture data downloaded ok and is ready to be decoded into a grcTexture

		DECODE_FAILED,			// Something went wrong during the conversion to a grcTexture
		DOWNLOAD_FAILED,		// The request was ok, but the download failed

		DECODE_SUCCESSFUL,		// Decoding complete

		READY_FOR_USER,			// Request has completed successfully, waiting on user to release it

		FREE_TO_REUSE,			// This request is available to be reused
	};
	Status m_Status;

	enum Type
	{
		INVALID_FILE,
		JPEG_FILE,
		DDS_FILE,
		RESOURCE_FILE,
	};
	Type m_FileType;

	// Request id provided by the cloud manager (-1 for an invalid id)
	CloudRequestID	m_RequestId;

	// Hash names for the texture and TXD
	atFinalHashString	m_TxdAndTextureHash;

	// A hash to distinguish the actual image. Can be a relative path
	atHashString		m_CloudFileHash;

	strLocalIndex		m_TxdSlot;

	// Temporary storage for the downloaded data. This memory is managed by CDownloadableTextureManager
	void*			m_pSrcBuffer;
	unsigned		m_SrcBufferSize;

	// Texture storage. Only used on unified memory consoles. This memory is allocated by CDownloadableTextureManager; it gets deallocated
	// if the texture decoding fails; otherwise, the memory is handed over to the TXD store.
	void*			m_pDstBuffer;
	unsigned		m_DstBufferSize;

	// Temporary storage for decoding jpeg data.
	void*			m_pScratchBuffer;
	unsigned		m_ScratchBufferSize;

	// Only valid if CDecodeTextureFromBlobWorkJob completes successfully (decoding stage).
	// This texture will eventually be placed and released to the TXD store; CDownloadableTextureManager
	// does manage the texture once it's been placed in the TXD store.
	grcTexture*		m_pDecodedTexture;

	// Cached texture data
	u32				m_ImgFormat;
	u32				m_Width;
	u32				m_Height;
	u32				m_NumMips;
	u32				m_PixelDataSize;
	u32				m_PixelDataOffset;

	u32				m_JPEGScalingFactor;

	// This flag is always accessed/modified using atomic operations. R/W operations on a CTextureDownloadRequest instance are only
	// expected on the main thread and a worker thread running a CDecodeTextureFromBlobWorkJob work item.
	// Before scheduling a CDecodeTextureFromBlobWorkJob, this flag will be set to IN_PROCESS_FLAG_ACQUIRED 
	// (expected to have a value of IN_PROCESS_FLAG_FREE), and the job will set it to IN_PROCESS_FLAG_FREE once it's done with it 
	// (expecting a value of IN_PROCESS_FLAG_ACQUIRED). The main thread will not modify a CTextureDownloadRequest if m_InProcessFlag 
	// has a value other than IN_PROCESS_FLAG_FREE
	unsigned int	m_InProcessFlag;										

	TextureDownloadRequestHandle m_Handle;

	int				m_CloudEventResultCode;

	// Allow a request to be marked for release at any time, regardless of its current state
	bool			m_bUserReleased;

	// When true we only download and cache the image but we don't create the texture
	bool			m_OnlyCacheAndVerify;

	// For decoding-only requests, the source buffer is provided by the user, so we cannot delete this memory
	bool			m_bSrcBufferOwnedByUser;

	// JPEG-only: allows specifying whether the JPEG should be encoded as a DXT1 texture, rather than uncompressed RGBA8.
	bool			m_bJPEGEncodeAsDXT;
#if __BANK
	u32				m_timeMsTakenDownloading;
	u32				m_timeMsTakenPreparingForDecoding;
	float			m_timeMsTakenDecoding;
#endif

	static const int INVALID_HANDLE;
};

// Manager that deals with download requests for textures stored in the cloud. If a download request is successful, the user
// can then access the texture via the TXD store.
class CDownloadableTextureManager : public CloudListener
{
public:
	static const int MAX_DOWNLOAD_REQUESTS = 64;

	friend class CDecodeTextureFromBlobWorkJob;

	CDownloadableTextureManager();
	~CDownloadableTextureManager();

	/** PURPOSE: Request a texture download from the cloud. 
	 *
	 *  RETURNS: The status of the download request. Any return value other than IN_PROGRESS means the request failed and wasn't issued.
	 *			 If the request failed, handle will be set to CTextureDownloadRequest::INVALID_HANDLE
	 *  PARAMS:
	 *   handle - (output variable) Download request handle used to query the status of the request and retrieve any downloaded data.
	 *   requestDesc - Descriptor for the download request filled in by the caller
	 */
	CTextureDownloadRequest::Status RequestTextureDownload(TextureDownloadRequestHandle& handle, const CTextureDownloadRequestDesc& requestDesc);

	/** PURPOSE: Request a texture to be decoded from a data blob provided by the user. 
	 *
	 *  RETURNS: The status of the request. Any return value other than IN_PROGRESS means the request failed and wasn't issued.
	 *			 If the request failed, handle will be set to CTextureDownloadRequest::INVALID_HANDLE
	 *  PARAMS:
	 *   handle - (output variable) Download request handle used to query the status of the request and retrieve any downloaded data.
	 *   requestDesc - Descriptor for the download request filled in by the caller
	 */
	CTextureDownloadRequest::Status RequestTextureDecode(TextureDownloadRequestHandle& handle, const CTextureDecodeRequestDesc& requestDesc);

    /** PURPOSE: Query whether a request is pending
	 *  RETURNS: True if the request for the texture is in progress - false otherwise (including if the handle is invalid or doesn't exist)
	 *  PARAMS:
	 *   handle - Download request handle used to query the status of the request and retrieve any downloaded data.
	 */
	bool IsAnyRequestPendingRelease() const;

	/** PURPOSE: Query whether a request is pending
	 *  RETURNS: True if the request for the texture is in progress - false otherwise (including if the handle is invalid or doesn't exist)
	 *  PARAMS:
	 *   handle - Download request handle used to query the status of the request and retrieve any downloaded data.
	 */
	bool IsRequestPending(TextureDownloadRequestHandle handle) const;

	/** PURPOSE: Query whether a request is ready
	 *  RETURNS: True if texture is ready - false otherwise (including if the handle is invalid or doesn't exist)
	 *  PARAMS:
	 *   handle - Download request handle used to query the status of the request and retrieve any downloaded data.
	 */
	bool IsRequestReady(TextureDownloadRequestHandle handle) const;

	/** PURPOSE: Query whether a request has failed 
	 *  RETURNS: True if request failed - false otherwise
	 *  PARAMS:
	 *   handle - Download request handle used to query the status of the request and retrieve any downloaded data.
	 */
	bool HasRequestFailed(TextureDownloadRequestHandle handle) const;

	/** PURPOSE: Get the slot from the TXD store where the texture associated with the request lives
	 *  RETURNS: A valid txd slot if the request completed successfully; -1 otherwise
	 *  PARAMS:
	 *   handle - Download request handle used to query the status of the request and retrieve any downloaded data.
	 */
	strLocalIndex	GetRequestTxdSlot(TextureDownloadRequestHandle handle) const;

	/** PURPOSE: Release a texture request previously initiated by the user. This can be called anytime, 
	 *			 but the request will not be released immediately.
	 *			 
	 *  PARAMS:
	 *   handle - Download request handle used to query the status of the request and retrieve any downloaded data.
	 */
	void ReleaseTextureDownloadRequest(TextureDownloadRequestHandle handle);

	/** PURPOSE: Query the http result code for a download request - a download request will only get either none (eg:
	 *			 the request is cancelled early) or a single update from the CloudManager, once it's finished.
	 *           
	 *			 
	 *  RETURNS: -1 if the request handle was not found 
	 *			  0 if the CloudManager has not returned any update on the request
	 *			    a netHttpStatusCodes otherwise
	 *   handle - Download request handle used to query the status of the request and retrieve any downloaded data.
	 */
	int GetTextureDownloadRequestResultCode(TextureDownloadRequestHandle handle) const;

	// CloudListener callback
	void OnCloudEvent(const CloudEvent* pEvent);

	// Housekeeping of ongoing requests
	void Update();

	static void InitClass();

	static void ShutdownClass();

	static CDownloadableTextureManager &GetInstance()	{ FastAssert(sm_Instance); return *sm_Instance; }

#if __ASSERT
	static void CheckTXDStoreCapacity();
	static bool IsThisMainThread();
	static void SetMainThreadHack(bool isMainThread) {sm_hackMainThread = isMainThread;}
#endif

#if __BANK
	void DumpManagedTxdList(bool toScreen) const;
#endif

private:

	// Get an already existing CTextureDownloadRequest (returns false if not found and pRequest doesn't get set)
	bool FindDownloadRequestByTxdAndTextureHashName(atFinalHashString txdName, CTextureDownloadRequest*& pRequest, bool allowUserReleased);

	// Get an already existing CTextureDownloadRequest (returns false if not found and pRequest doesn't get set)
	bool FindDownloadRequestByHandle(TextureDownloadRequestHandle handle, CTextureDownloadRequest*& pRequest);

	// Get an already existing CTextureDownloadRequest (returns false if not found and pRequest doesn't get set)
	bool FindDownloadRequestByHandle(TextureDownloadRequestHandle handle, const CTextureDownloadRequest*& pRequest) const;

	// Get an already existing CTextureDownloadRequest (returns false if not found and pRequest doesn't get set)
	bool FindDownloadRequestByRequestId(CloudRequestID cloudRequestId, CTextureDownloadRequest*& pRequest);

	// Get a free CTextureDownloadRequest (returns false if there's no free ones and pRequest doesn't get set)
	TextureDownloadRequestHandle FindFreeDownloadRequest(CTextureDownloadRequest*& pRequest);

	// Get a txd already registered in the store 
	bool FindRequestedTextureInTxdStore(const CTextureDownloadRequest* pRequest, s32& txdSlot, atHashString& cloudFileHash, bool& contentChanged) const;

	// Release download request. Returns false when the request is still in use
	bool RemoveDownloadRequest(CTextureDownloadRequest* pRequest, bool bReleaseTextureMemory);

	// Computes file type, memory requirements and tries allocating texture memory for it
	// If successful, the request will be queued for decoding
	bool PrepareDownloadRequestForDecoding(CTextureDownloadRequest* pRequest);

	// Used for the decoding stage; if successful, it will output a grcTexture from the data cached in pRequest
	bool CreateTextureFromBlob(CTextureDownloadRequest* pRequest, grcTexture*& pOutTexture);

	// Processes a released request - will only release the entry if it is in a failed or complete state,
	// otherwise it will try again in subsequent updates
	bool ProcessReleasedDownloadRequest(CTextureDownloadRequest* pRequest);

	// A grcTexture has been generated - hand it over to the TXD store
	bool ProcessDecodedDownloadRequest(CTextureDownloadRequest* pRequest);

	// A request was issued but failed at some point (download, decoding)
	bool ProcessFailedDownloadRequest(CTextureDownloadRequest* pRequest);

	bool IsFailedState(CTextureDownloadRequest::Status const status) const { return (status == CTextureDownloadRequest::DECODE_FAILED || status == CTextureDownloadRequest::DOWNLOAD_FAILED); }

	bool IsPendingUseState( CTextureDownloadRequest::Status const status ) const { return status == CTextureDownloadRequest::FREE_TO_REUSE; }

	bool IsInProgressState( CTextureDownloadRequest::Status const status ) const { return status == CTextureDownloadRequest::IN_PROGRESS; }
#if __ASSERT
	static bool sm_hackMainThread;
#endif


	// Pool for download requests
	atVector<CTextureDownloadRequest, MAX_DOWNLOAD_REQUESTS>	m_DownloadRequests;

	// Txd store helper


	// Texture memory manager
	CDownloadableTextureMemoryManager							m_TextureMemoryManager;

	// Provides local files in a manner we can digest like cloud files
	CSurrogateCloudFileProvider									m_localFileProvider;

	static CDownloadableTextureManager *sm_Instance;

};

#define DOWNLOADABLETEXTUREMGR	CDownloadableTextureManager::GetInstance()

#if __BANK
class CDownloadableTextureManagerDebug 
{

public:

	CDownloadableTextureManagerDebug() {};

	static void		InitClass();
	static void		ShutdownClass();

	void			Update();
	void			Render();


	void			Init();
	void			AddWidgets(rage::bkBank& bank);

	void			OnScopeTypeSelected();
	void			OnLoadTexture();
	void			OnReleaseTexture();

	bool			SaveDownloadedFilesToDisk() const { return m_bStoreDownloadsToDisk; }

	static CDownloadableTextureManagerDebug &GetInstance()	{ FastAssert(sm_Instance); return *sm_Instance; }

private:

	TextureDownloadRequestHandle				m_curRequestHandle;
	strLocalIndex								m_curRequestTxdSlot;
	grcTexture*									m_pCurTexture;

	bkCombo*									m_pScopeTypeCombo;
	int											m_scopeTypeComboIdx;
	char										m_newFileName[128];

	bool										m_bDumpManagedTxdList;
	bool										m_bPrintOnScreenManagedTxdList;
	bool										m_bStoreDownloadsToDisk;
	bool										m_bTestDanglingReference;
	bool										m_bDanglingReferenceAdded;
	bool										m_bRemoveDanglingReference;

	static CDownloadableTextureManagerDebug *	sm_Instance;
	static const char*							sm_pScopeTypeStr[4];
};

#define DOWNLOADABLETEXTUREMGRDEBUG CDownloadableTextureManagerDebug::GetInstance()

#endif // __BANK


//////////////////////////////////////////////////////////////////////////

typedef int ScriptTextureDownloadHandle;
typedef atHashString atTxdHashString;

// Entry for downloadable texture requested from script
struct CScriptDownloadableTextureEntry
{
	ScriptTextureDownloadHandle		m_Handle;
	atFinalHashString				m_TextureName;
	atTxdHashString					m_TextureHash;
	TextureDownloadRequestHandle	m_RequestHandle;
#if !__FINAL
	atNonFinalHashString			m_scriptHash;
#endif
	strLocalIndex					m_TxdSlot;
	s32								m_RefCount;
	bool							m_bTxdRefAdded;

	CScriptDownloadableTextureEntry() { Reset(); }

	void Init(ScriptTextureDownloadHandle handle, TextureDownloadRequestHandle requestHandle, const char* pTextureName, s32 txdSlot, s32 refCount, bool bTxdRefAdded);
	void Reset();
		
};

// Helper class to manage and simplify downloadable texture requests made from script.
// It is the only interface to the DTM for scripts. It takes care of correctly handling 
// the reference count for TXDs and avoids submitting download requests for textures that
// are already in memory.
class CScriptDownloadableTextureManager
{

public:

	CScriptDownloadableTextureManager();
	~CScriptDownloadableTextureManager();


	/** PURPOSE: Request a texture download from the cloud from a member location. If the request is successful
	 *			 the manager will automatically add a ref to the TXD on behalf of the script
	 *
	 *  RETURNS: A handle to the texture request that can be used to query its state; it will return
	 *			 INVALID_HANDLE (NULL in script) if the request failed.
	 *
	 *  PARAMS:
	 *   textureName - Name used both for the texture *and* its txd
	 *   cloudFilePath - Relative path to the file in the cloud
	 *	 bUseCacheWithoutCloudChecks - Forces the request to avoid server checks for potentially cached textures.
	 */
	ScriptTextureDownloadHandle	RequestMemberTexture(rlGamerHandle gamerHandle, const char* cloudFilePath, const char* textureName, bool bUseCacheWithoutCloudChecks);

     /** PURPOSE: Request a texture download from the cloud from a title location. If the request is successful
	 *			 the manager will automatically add a ref to the TXD on behalf of the script
	 *
	 *  RETURNS: A handle to the texture request that can be used to query its state; it will return
	 *			 INVALID_HANDLE (NULL in script) if the request failed.
	 *
	 *  PARAMS:
	 *   textureName - Name used both for the texture *and* its txd
	 *   cloudFilePath - Relative path to the file in the cloud
	 *	 bUseCacheWithoutCloudChecks - Forces the request to avoid server checks for potentially cached textures.
	 */
	ScriptTextureDownloadHandle	RequestTitleTexture(const char* cloudFilePath, const char* textureName, bool bUseCacheWithoutCloudChecks);

    /** PURPOSE: Request a texture download from the cloud from a UGC location. If the request is successful
	 *			 the manager will automatically add a ref to the TXD on behalf of the script
	 *
	 *  RETURNS: A handle to the texture request that can be used to query its state; it will return
	 *			 INVALID_HANDLE (NULL in script) if the request failed.
	 *
	 *  PARAMS:
	 *   textureName - Name used both for the texture *and* its txd
	 *	 bUseCacheWithoutCloudChecks - Forces the request to avoid server checks for potentially cached textures.
	 */
	ScriptTextureDownloadHandle	RequestUgcTexture(const char* szContentID, int nFileID, int nFileVersion, const rlScLanguage nLanguage, const char* textureName, bool bUseCacheWithoutCloudChecks);

	/** PURPOSE: Releases a texture
	 *  PARAMS:
	 *   handle - Handle to the texture request, if it's INVALID_HANDLE the function does nothing
	 */
	void ReleaseTexture(ScriptTextureDownloadHandle handle);

	/** PURPOSE: Queries whether a request has failed
	 *  RETURNS: true if the request failed, false otherwise (also if handle is INVALID_HANDLE)
	 *			
	 *  PARAMS:
	 *   handle - Handle to the texture request
	 */
	bool HasTextureFailed(ScriptTextureDownloadHandle handle) const;

	/** PURPOSE: Retrieves the texture/txd name for a request
	 *  RETURNS: NULL if the handle is invalid or the texture is not ready
	 *			
	 *  PARAMS:
	 *   handle - Handle to the texture request
	 */
	const char* GetTextureName(ScriptTextureDownloadHandle handle) const;

	/** PURPOSE: Checks if a texture with the given name exists in the array of downloaded textures
	 *  RETURNS: TRUE if the texture exists in the array
	 *			
	 *  PARAMS:
	 *   handle - Name of the texture to look for
	 */
	bool IsNameOfATexture(const char* pTextureName);

	void Init();
	void Shutdown();
	void Update();
	
	static CScriptDownloadableTextureManager &GetInstance()	{ FastAssert(sm_Instance); return *sm_Instance; }

	static void InitClass();
	static void ShutdownClass();

	static const int INVALID_HANDLE = 0;

	/** PURPOSE: Prints the list of currently managed script downloadable textures
	 */
#if !__FINAL
	void PrintTextures(bool onScreen);
#endif

private:

	ScriptTextureDownloadHandle	RequestTexture(const CTextureDownloadRequestDesc& requestDesc, const char* textureName);

	void ReleaseAllTextures();
	void ProcessPendingTextures();
	void ProcessReleasedTextures();

	bool IsHandleValid(ScriptTextureDownloadHandle handle) const { return ((int)handle != INVALID_HANDLE); }

	bool FindTextureByName(const char* pTextureName, CScriptDownloadableTextureEntry*& pEntry);
	bool FindTextureByHandle(ScriptTextureDownloadHandle handle, CScriptDownloadableTextureEntry*& pEntry);
	bool FindTextureByHandle(ScriptTextureDownloadHandle handle, const CScriptDownloadableTextureEntry*& pEntry) const;

	fwLinkList<CScriptDownloadableTextureEntry>					m_TextureList;
	typedef fwLink<CScriptDownloadableTextureEntry>				ScriptTextureNode;

	static const int	MAX_SCRIPT_TEXTURES = 16;

	static CScriptDownloadableTextureManager *sm_Instance;

};

#define SCRIPTDOWNLOADABLETEXTUREMGR	CScriptDownloadableTextureManager::GetInstance()


#endif // SCENE_DOWNLOADABLE_TEXTURE_MANAGER_H
