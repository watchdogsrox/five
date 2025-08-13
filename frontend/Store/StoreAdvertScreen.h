#ifndef __STORE_ADVERT_SCREEN_H__
#define __STORE_ADVERT_SCREEN_H__

#include "frontend/Scaleform/ScaleFormMgr.h"


// Rage includes
#include "atl/string.h"
#include "data/growbuffer.h"
#include "data/sax.h"
#include "net/status.h"

class cStoreAdvert
{
public:
    void Init() {}

    atString &GetId() { return m_Id; }
    atString &GetTitle() { return m_Title; }
    atString &GetText() { return m_Text; }
    atString &GetImagePath() { return m_ImagePath; }
    atString &GetLinkedProductId() { return m_LinkedProductId; }

    const atString &GetId() const { return m_Id; }
    const atString &GetTitle() const { return m_Title; }
    const atString &GetText() const { return m_Text; }
    const atString &GetImagePath() const { return m_ImagePath; }
    const atString &GetLinkedProductId() const { return m_LinkedProductId; }

private:
    atString m_Id;
    atString m_Title;
    atString m_Text;
    atString m_ImagePath;
    atString m_LinkedProductId;
};

class cStoreAdvertData
{
public:
    cStoreAdvertData();
    virtual ~cStoreAdvertData();

    void Init( int localUserIndex, const char* skuFolder );
    void Shutdown();

    void Update();

    bool StartDataFetch();
    void AbortDataFetch();
    const bool IsInDataFetchState() const;

    void ResetData();

    void SetLocalUserIndex(int localUserIndex) { m_LocalGamerIndex = localUserIndex; }

    bool HasValidData() const { return m_bHasValidData; }
    int GetNumAdvertDataRecords() const;

    const cStoreAdvert* GetAdvertRecord( int adIndex ) const;

protected:
    class cStoreAdSaxReader : public datSaxReader
    {
    public:
        cStoreAdSaxReader(): datSaxReader()
            , m_pAdvertData( NULL )
            , m_State(STATE_EXPECT_ROOT)
            , m_pCurrentAdvert(NULL)
        {
            ResetCurrentData();
        }

        void Start( cStoreAdvertData* pStoreAdData);
    private:
        virtual void startElement(const char* url, const char* localName, const char* qName);
        virtual void endElement(const char* url, const char* localName, const char* qName);
        virtual void attribute(const char* tagName, const char* attrName, const char* attrVal);
        virtual void characters(const char* ch, const int start, const int length);

        void startProductSubElement( const char* url, const char* localName, const char* qName );
        void startCategorySubElement( const char* url, const char* localName, const char* qName );

        void ResetCurrentData();

        enum State
        {
            STATE_EXPECT_ROOT,
            STATE_EXPECT_ADVERT,
            STATE_EXPECT_ADVERT_SUB_ELEMENT,
            STATE_READING_ADVERT_TITLE,
            STATE_READING_ADVERT_TEXT,
            STATE_READING_ADVERT_IMAGE_PATH,
            STATE_READING_ADVERT_LINKED_PRODUCT_ID
        } m_State;

        cStoreAdvert* m_pCurrentAdvert;

        cStoreAdvertData* m_pAdvertData;
    };

private:

    //Parser state functions
    void UpdateParserWaitingState();
    void UpdateParserStartupState();
    void UpdateParserReceivingState();
    void UpdateParserCleanupState();
    void UpdateParserLocalFileState();
    


    enum eParserState
    {
        AD_DATA_XML_WAITING,
        AD_DATA_XML_STARTUP,
        AD_DATA_XML_RECEIVING,
        AD_DATA_XML_LOCAL_FILE,
        AD_DATA_XML_CLEANUP,
        AD_DATA_XML_NUM_STATES
    };

    eParserState m_ParserState;

    bool m_bInitialised;

    int m_LocalGamerIndex;

    bool m_bHasValidData;
    bool m_bIsInErrorState;
    bool m_bDataRefreshPending;

    datGrowBuffer m_GrowBuffer;

    netStatus m_AdDataXMLFetchStatus;

    fiStream* m_pLocalFileStream;

    atString m_SkuDirString;

    cStoreAdSaxReader m_saxReader;

    atArray<cStoreAdvert*> m_pAdvertDataArray;
};

class cStoreAdvertScreen
{
public:
    cStoreAdvertScreen();
    virtual ~cStoreAdvertScreen();

    void Init();
    void Shutdown();
    void Update();
    void Render();
    
    void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

    void DumpDebugInfo();

    bool IsLoaded() { return m_IsLoaded; }

private:
    
    bool AreAllAssetsActive();
    void UpdateInput();

    void RequestImages();
    void UpdateImageRequests();
    void ReleaseLoadedImages();
    
    void SetupAdverts();
    void SetupButtons();
    void SetupAd( int aAdIndex );
    enum
    {
        NUM_COLUMNS = 3   
    };

    enum eAdvertScreenMode
    {
        ADVERT_SCREEN_MODE_SINGLE,
        ADVERT_SCREEN_MODE_TRIPLE,
        ADVERT_SCREEN_MODE_NUM
    };

    enum eAdScreenInputVals
    {
        DPADUP = 8,
        DPADDOWN = 9,
        DPADLEFT = 10,
        DPADRIGHT = 11
    };

    enum eButtonState
    {
        ONLY_CANCEL,
        CANCEL_AND_CONFIRM
    };


    enum eAdImageState
    {
        AD_IMAGE_STATE_NONE,
        AD_IMAGE_STATE_REQUESTED,
        AD_IMAGE_STATE_LOADED,
        AD_IMAGE_STATE_NUM
    };

    // eAdvertScreenMode m_ScreenMode;

    eAdImageState m_ImageStates[ NUM_COLUMNS ];

    eButtonState m_ButtonState;


    s32 m_MovieId;
    s32 m_ButtonMovieId;

    bool m_bLoadingAssets;
    bool m_bPendingAdvertSetup;

    bool m_IsActive;
    bool m_IsLoaded;

    bool m_DisplayingErrorDialog;

    int m_ColumnRequestIndex;
    int m_CurrentColumn;
};

#endif //__STORE_ADVERT_SCREEN_H__