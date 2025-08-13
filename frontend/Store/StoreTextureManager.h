#ifndef __STORE_TEXTURE_MGR_H__
#define __STORE_TEXTURE_MGR_H__

#include "atl/queue.h"
#include "atl/slist.h"
#include "atl/array.h"
#include "string/string.h"

#include "data/growbuffer.h"

#include "scene/DownloadableTextureManager.h"

namespace rage
{
    class sysMemAllocator;
}


class cStoreTextureManager
{
public:
    cStoreTextureManager();
    virtual ~cStoreTextureManager();

    void Init();
    void Shutdown();

    void Update();

    bool RequestTexture( const atString& aTexturePath );
    void ReleaseTexture( const atString& aTexturePath );
    void CancelRequest( const char* aTexturePath );
    
    void SetMemoryAllocator( rage::sysMemAllocator* aNewMemAllocator ) { m_pMemoryAllocator = aNewMemAllocator; }

    static void ConvertPathToTextureName( atString &aPath );

    void UpdateGarbage();

    void FreeAllTextures();

private:

    class cStoreTexture
    {
    public:
        cStoreTexture( atString textureName );
        virtual ~cStoreTexture();

        bool InitialiseTexture( int downloadedTxdSlot );
        const atString GetTextureName() { return m_TextureName; }

        bool IsJustCreated() { return m_JustCreated; }

        void Destroy();

        void IncRefCount() { m_JustCreated = false; m_RefCount++; }
        void DecRefCount() { m_RefCount--; }

        int GetRefCount() { return m_RefCount; }

        bool ReadyForDelete();
    private:
        bool RegisterAsTxd();
        void UnregisterAsTxd();

        atString m_TextureName;
        strLocalIndex m_TextureDictionarySlot;
        int m_RefCount;

        //Flag to handle the state where the texture has just been created but has not been referenced.
        bool m_JustCreated;
    };

    atMap<atString,cStoreTexture*> m_TextureMap;

    enum eTextureManagerState
    {
        STATE_NEUTRAL,
        STATE_DOWNLOADING,
        STATE_ERROR,
        NUM_STATES
    };

    eTextureManagerState m_CurrentState;

    void AddToDownloadQueue( const char* aTexturePath );
    void UpdateNeutralState();
    void UpdateDownloadingState();
    void UpdateErrorState();

    int GetSizeOfRequestedTexture( const char *aTexturePath );
    bool StartTextureDownload( const char *aPath );
    bool CreateTexture();
    
    static const int MAX_QUEUED_IMAGES = 10;
    atQueue<atString, MAX_QUEUED_IMAGES> m_RequestedImagesQueue;

    atString m_CurrentTextureTitle;

    sysMemAllocator *m_pMemoryAllocator;

    
    //Garbage collection. 
    // Entry in our list of garbage (stuff waiting to be deleted). Completely ripped from M. Krehans emblem manager.

    void PutInGarbage( int txdSlot );
    
    
    struct TextureGarbageEntry
    {
    public:
        TextureGarbageEntry( int txdSlot,	u32 nNumFrames) : m_TxdSlot(txdSlot), m_nNumFrames(nNumFrames)	{};
        TextureGarbageEntry() : m_TxdSlot(-1), m_nNumFrames(0) {}; // Don't allow default constructor

        // Destroy this entry if it's time, other wise do nothing
        bool DestroyDeferred();

        // Destroy this entry now, only call this if you know what you're doing
        void DestroyImmediate();
        
        bool operator==(const TextureGarbageEntry& rhs) const
        {
            return m_TxdSlot == rhs.m_TxdSlot;
        }

        int GetTextureDictionarySlot() { return m_TxdSlot.Get(); }


    private:

        strLocalIndex m_TxdSlot;
        u32 m_nNumFrames;
    };

    typedef atSList<TextureGarbageEntry> TextureGarbageList;
    typedef atSNode<TextureGarbageEntry> TextureGarbageNode;
    TextureGarbageList m_TextureGarbage;

    TextureDownloadRequestHandle m_CurrentRequestHandle;

    atArray<u32> m_RequestedTextureHashArray;

};

#endif //__STORE_TEXTURE_MGR_H__