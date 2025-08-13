#include "Frontend/Store/StoreTextureManager.h"
#include "Frontend/Store/StoreUIChannel.h"
#include "Frontend/Store/StoreScreenMgr.h"



#include "renderer/RenderThread.h"
#include "scene/DownloadableTextureManager.h"
#include "system/memory.h"

FRONTEND_STORE_OPTIMISATIONS()

const int COMMERCE_TEXTURE_SIZE = 256 * 1024;

const int FRAMES_TO_DELAY_REF_REMOVAL = 3;
const int INITIAL_REQUEST_HASH_ARRAY_SIZE = 20;

cStoreTextureManager::cStoreTextureManager() :
    m_pMemoryAllocator(NULL),
    m_CurrentState(STATE_NEUTRAL),
    m_CurrentRequestHandle(CTextureDownloadRequest::INVALID_HANDLE)
{
    m_RequestedTextureHashArray.Reset();
    m_RequestedTextureHashArray.Reserve(INITIAL_REQUEST_HASH_ARRAY_SIZE);
}

cStoreTextureManager::~cStoreTextureManager()
{
    Shutdown();
}

void cStoreTextureManager::Init()
{
    m_RequestedImagesQueue.Reset();
    m_CurrentTextureTitle.Reset();
    m_CurrentState = STATE_NEUTRAL;
    m_RequestedTextureHashArray.Reset();
    m_RequestedTextureHashArray.Reserve(INITIAL_REQUEST_HASH_ARRAY_SIZE);
}

void cStoreTextureManager::Shutdown()
{
    m_RequestedImagesQueue.Reset();
    m_CurrentTextureTitle.Reset();

    m_CurrentState = STATE_NEUTRAL;
    FreeAllTextures();

    if ( m_CurrentRequestHandle != CTextureDownloadRequest::INVALID_HANDLE )
    {
        DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_CurrentRequestHandle );
        m_CurrentRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;
    }
}

void cStoreTextureManager::Update()
{
    switch( m_CurrentState )
    {
    case(STATE_NEUTRAL):
        UpdateNeutralState();
        break;
    case(STATE_DOWNLOADING):
        UpdateDownloadingState();
        break;
    case(STATE_ERROR):
        UpdateErrorState();
        break;
    default:
        storeUIAssertf(false,"Unhandled state in m_CurrentState");
    }

    UpdateGarbage();
}

bool cStoreTextureManager::RequestTexture( const atString& aTexturePath )
{
    atString strippedPath = aTexturePath;
    strippedPath.Replace("cloud:/","");
    
    //Check to see if this texture is either on the way or queued.
    if ( m_RequestedImagesQueue.Find( strippedPath ) || m_CurrentTextureTitle == strippedPath )
    {
        return false;
    }

    cStoreTexture **existingStoreTexture = m_TextureMap.Access( strippedPath );
    if ( existingStoreTexture )
    {
        //This has been downloaded and registered already.
        (*existingStoreTexture)->IncRefCount();
        return true;
    }
    
    //Add to the DL list.
    storeUIDebugf3("Added %s to the cStoreTextureManager download queue.",strippedPath.c_str());
    AddToDownloadQueue( strippedPath );

    return false;
}

void cStoreTextureManager::AddToDownloadQueue( const char *aTexturePath )
{
    atString texturePathString(aTexturePath);
    if ( m_RequestedImagesQueue.Find( texturePathString ) )
    {
        return;
    }

    if ( m_RequestedImagesQueue.GetCount() == MAX_QUEUED_IMAGES )
    {
        storeUIErrorf("Downloadable image queue is full.");
        return;
    }

    storeUIDebugf3("Adding %s to the requested image queue.", aTexturePath );

    m_RequestedImagesQueue.Push( texturePathString );
}

void cStoreTextureManager::UpdateNeutralState()
{
   //If the queue is not empty, start a download and go to downloading state.
    if ( m_RequestedImagesQueue.GetCount() > 0 )
    {
        m_CurrentTextureTitle = m_RequestedImagesQueue.Pop();
        if ( StartTextureDownload( m_CurrentTextureTitle ) )
        {
            m_CurrentState = STATE_DOWNLOADING;
        }
    }   
}

void cStoreTextureManager::UpdateDownloadingState()
{
    storeUIAssert( m_CurrentRequestHandle != CTextureDownloadRequest::INVALID_HANDLE );

    if ( DOWNLOADABLETEXTUREMGR.HasRequestFailed(m_CurrentRequestHandle) )
    {
        storeUIErrorf( "Fetch of path %s failed.", m_CurrentTextureTitle.c_str() );
        m_CurrentState = STATE_NEUTRAL;
    }
    else if ( DOWNLOADABLETEXTUREMGR.IsRequestReady( m_CurrentRequestHandle ) )
    {
        if ( !CreateTexture() )
        {
            //This means we have been asked to wait a frame.
            return;
        }
        m_CurrentTextureTitle.Reset();

        DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_CurrentRequestHandle );
        m_CurrentRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;

        m_CurrentState = STATE_NEUTRAL;
    }
}

void cStoreTextureManager::UpdateErrorState()
{
    //An error has occurred. Check whether it is terminal, and report then set back to neutral as appropriate
}

int cStoreTextureManager::GetSizeOfRequestedTexture( const char* /*aTexturePath*/ )
{
    //Do this in a dumb way at first. May smarten this up moving forward.
    return COMMERCE_TEXTURE_SIZE;
}

bool cStoreTextureManager::StartTextureDownload( const char *aPath )
{
    int sizeInBytes = GetSizeOfRequestedTexture( aPath );

    CTextureDownloadRequestDesc::Type requestType = CTextureDownloadRequestDesc::TITLE_FILE;

    atString strippedName(aPath);
    ConvertPathToTextureName( strippedName );

    // Fill in the descriptor for our request
    CTextureDownloadRequestDesc requestDesc;
    requestDesc.m_Type = requestType;
    requestDesc.m_GamerIndex		= -1;
    requestDesc.m_CloudFilePath		= aPath;
    requestDesc.m_BufferPresize		= sizeInBytes;
    requestDesc.m_TxtAndTextureHash	= strippedName.c_str();

    if ( m_RequestedTextureHashArray.Find( atStringHash(aPath)) == -1 )
    {
        m_RequestedTextureHashArray.Append() = atStringHash(aPath);
    }
    else
    {
        //Previously requested
        requestDesc.m_CloudRequestFlags = eRequest_CacheForceCache; //TODO_ANDI: not right
    }

    // Issue the request
    TextureDownloadRequestHandle handle;
    CTextureDownloadRequest::Status retVal;
    retVal = DOWNLOADABLETEXTUREMGR.RequestTextureDownload(handle, requestDesc);

    // Check the return value
    if (retVal != CTextureDownloadRequest::IN_PROGRESS && retVal != CTextureDownloadRequest::READY_FOR_USER )
    {
        storeUIWarningf("cStoreTextureManager::StartTextureDownload: request failed before being issued");
        return false;
    }

    // Cache the handle for querying the state of the request
    m_CurrentRequestHandle = handle;

    return true;
}


bool cStoreTextureManager::CreateTexture()
{
    
    cStoreTexture* storeTexture = rage_new cStoreTexture( m_CurrentTextureTitle );

    storeUIAssert(DOWNLOADABLETEXTUREMGR.IsRequestReady(m_CurrentRequestHandle));

    int txdSlot = DOWNLOADABLETEXTUREMGR.GetRequestTxdSlot(m_CurrentRequestHandle).Get();

    if ( !storeTexture->InitialiseTexture(txdSlot) )
    {
        //handle the "try next frame" state.
        return false;
    }

    m_TextureMap.Insert( m_CurrentTextureTitle, storeTexture );

    return true;
}

void cStoreTextureManager::ConvertPathToTextureName( atString &aPath )
{    

    aPath.Replace("cloud:/","");
    aPath.Replace("/","__");
    aPath.Uppercase();
    aPath.Replace(".DDS","");
}

void cStoreTextureManager::ReleaseTexture( const atString& aTexturePath )
{
    atString strippedPath = aTexturePath;
    strippedPath.Replace("cloud:/","");

    //See if the texture is already in the map
    cStoreTexture **retrievedTexture = m_TextureMap.Access(strippedPath);

    cStoreTexture *storeTexture;

    if (retrievedTexture == NULL)
    {
        //No surch texture
        storeUIErrorf("Requested the release of a texture that does not exist: %s", strippedPath.c_str());
        return;
    }
    
    
    storeTexture = *retrievedTexture;
    
    storeTexture->DecRefCount();

    if ( storeTexture->GetRefCount() <= 0 )
    {
        storeUIDebugf3( "Destroying %s with a ref count of %d", storeTexture->GetTextureName().c_str(), storeTexture->GetRefCount() );
        storeTexture->Destroy();
        m_TextureMap.Delete(strippedPath);
        delete storeTexture;
    }
}

void cStoreTextureManager::PutInGarbage( int txdSlot )
{
    // Defer deletion for three frames. This seems to be the agreed safe time from drawListMgr.
    TextureGarbageNode* pGarbageNode = rage_new TextureGarbageNode( TextureGarbageEntry( txdSlot, FRAMES_TO_DELAY_REF_REMOVAL ) );
    m_TextureGarbage.Append( *pGarbageNode );
}

void cStoreTextureManager::UpdateGarbage()
{
    // Loop over the garbage map and look for entries that need to be deleted
    TextureGarbageNode* pGarbageNode = m_TextureGarbage.GetHead();
    while ( NULL != pGarbageNode )
    {
        TextureGarbageNode* pTempNode = pGarbageNode;
        pGarbageNode = pGarbageNode->GetNext();

        if ( pTempNode->Data.DestroyDeferred() )
        {
            m_TextureGarbage.Remove( *pTempNode );
            delete pTempNode;
        }
    }
}

void cStoreTextureManager::FreeAllTextures()
{
    // Flush the render thread, we don't want to delete textures out from under it
    gRenderThreadInterface.Flush();

    // Destroy any textures we had
    atMap<atString,cStoreTexture*>::Iterator storeTextureIter = m_TextureMap.CreateIterator();
    for (storeTextureIter.Start(); !storeTextureIter.AtEnd(); ++storeTextureIter)
    {
        cStoreTexture* pTexture = storeTextureIter.GetData();
        delete pTexture;
    }

    m_TextureMap.Kill();

    // Destroy any textures that were sitting in the garbage
    TextureGarbageNode* pTextureNode = m_TextureGarbage.GetHead();
    while ( NULL != pTextureNode )
    {
        pTextureNode->Data.DestroyImmediate();
        pTextureNode = pTextureNode->GetNext();
    }
    m_TextureGarbage.DeleteAll();
}

void cStoreTextureManager::CancelRequest( const char* aTexturePath )
{
    atString strippedPath(aTexturePath);
    strippedPath.Replace("cloud:/","");

    int findIndex = -1;

    

    if ( m_RequestedImagesQueue.Find( strippedPath, &findIndex ) )
    {
        m_RequestedImagesQueue.Delete(findIndex);
    }

    if ( m_CurrentTextureTitle == strippedPath )
    {
        DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest(m_CurrentRequestHandle);
        m_CurrentRequestHandle = CTextureDownloadRequest::INVALID_HANDLE;

        storeUIErrorf( "Fetch of path %s canceled.", m_CurrentTextureTitle.c_str() );
        m_CurrentState = STATE_NEUTRAL;
    }
}

cStoreTextureManager::cStoreTexture::cStoreTexture( atString textureName ):
    m_TextureDictionarySlot(-1),
    m_TextureName(textureName),
    m_RefCount( 0 ),
    m_JustCreated(true)
{
    
}

cStoreTextureManager::cStoreTexture::~cStoreTexture()
{
    Destroy();
}

bool cStoreTextureManager::cStoreTexture::InitialiseTexture( int downloadedTxdSlot )
{
    m_TextureDictionarySlot = downloadedTxdSlot;

    RegisterAsTxd();

    return true;
}

void cStoreTextureManager::cStoreTexture::Destroy()
{
    if ( m_TextureDictionarySlot.Get() != -1 )
    {
        storeUIAssert(cStoreScreenMgr::GetTextureManager());
        cStoreScreenMgr::GetTextureManager()->PutInGarbage( m_TextureDictionarySlot.Get() );
        m_TextureDictionarySlot = strLocalIndex(-1);
    }
}

bool cStoreTextureManager::cStoreTexture::RegisterAsTxd()
{
    if(!storeUIVerifyf(m_TextureDictionarySlot != -1 , "Invalid TXD to register slot for emblem"))
    {
        return false;
    }


    atString strippedName = m_TextureName;
    cStoreTextureManager::ConvertPathToTextureName( strippedName );

    strLocalIndex txdSlot = m_TextureDictionarySlot;

    if (storeUIVerifyf(g_TxdStore.IsValidSlot(txdSlot), "cStoreTextureManager::cStoreTexture::RegisterAsTxd() - failed to get valid txd for slot %d", txdSlot.Get()))
    {
        fwTxd* txd = g_TxdStore.GetSafeFromIndex(txdSlot); 

        if (txd)
        {
            g_TxdStore.AddRef(txdSlot, REF_OTHER);

            storeUIDebugf3("Added Texture '%s' to TXD '%s' at slot %d", strippedName.c_str(), strippedName.c_str(), txdSlot.Get());

            return true;
        }
    }

    return false;
}

void cStoreTextureManager::cStoreTexture::UnregisterAsTxd()
{
    //This is now handled by the garbage collector

    /*
    //check to see if the slot is already there.
    if (m_TextureDictionarySlot != -1)
    {
        g_TxdStore.RemoveRef(m_TextureDictionarySlot, REF_OTHER);
    }

    m_TextureDictionarySlot = -1;
    */
    
}

bool cStoreTextureManager::TextureGarbageEntry::DestroyDeferred()
{

    --m_nNumFrames;
    if ( m_nNumFrames <= 0 )
    {
        storeUIDisplayf( "Deleting texture slot %d in cStoreTextureManager::TextureGarbageEntry::DestroyDeferred\n", m_TxdSlot.Get());

        DestroyImmediate();
        return true;
    }

    return false;
}

void cStoreTextureManager::TextureGarbageEntry::DestroyImmediate()
{
    if ( m_TxdSlot.Get() )
    {
        storeUIDisplayf( "Deleting texture <%d> (actually just decreasing ref) in cStoreTextureManager::TextureGarbageEntry::DestroyImmediate\n", m_TxdSlot.Get() );
        
        g_TxdStore.RemoveRef(m_TxdSlot, REF_OTHER);

        m_TxdSlot = -1;
    }
}