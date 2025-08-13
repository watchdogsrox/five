
#include "Frontend/Store/StoreAdvertScreen.h"

#include "Frontend/BusySpinner.h"
#include "Frontend/Store/StoreMainScreen.h"
#include "Frontend/Store/StoreScreenMgr.h"
#include "Frontend/Store/StoreUIChannel.h"
#include "Frontend/Store/StoreTextureManager.h"
#include "Frontend/WarningScreen.h"
#include "Network/Live/livemanager.h"
#include "network/commerce/CommerceManager.h"
#include "system/controlMgr.h"
#include "system/pad.h"


#include "text/TextConversion.h"

#if RSG_PC
#include "rline/rlpc.h"
#endif // RSG_PC

#define STOREMENU_FILENAME	"pause_menu_store_content"  
#define BUTTONMENU_FILENAME "store_instructional_buttons"
 
PARAM(adDataLocalFile, "Local store advert data file");

FRONTEND_STORE_OPTIMISATIONS()

cStoreAdvertScreen::cStoreAdvertScreen() :
    // m_ScreenMode(ADVERT_SCREEN_MODE_SINGLE),
    m_MovieId(-1),
    m_ButtonMovieId(-1),
    m_bLoadingAssets(false),
    m_bPendingAdvertSetup(false),
    m_ButtonState(ONLY_CANCEL),
    m_IsActive(false),
    m_IsLoaded(false),
    m_DisplayingErrorDialog(false),
    m_ColumnRequestIndex(0),
    m_CurrentColumn(0)
{
    for ( int i = 0; i < NUM_COLUMNS; i++ )
    {
        m_ImageStates[i] = AD_IMAGE_STATE_NONE;
    }
}

cStoreAdvertScreen::~cStoreAdvertScreen()
{
    Shutdown();
}

void cStoreAdvertScreen::Init()
{
    m_bPendingAdvertSetup = false;
    m_IsActive = true;
 
    if ( CScaleformMgr::FindMovieByFilename(BUTTONMENU_FILENAME) == -1 )
    {
        m_ButtonMovieId = CScaleformMgr::CreateMovie(BUTTONMENU_FILENAME, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
		CBusySpinner::RegisterInstructionalButtonMovie(m_ButtonMovieId);  // register this "instructional button" movie with the spinner system
    }
 

    Vector2 pos(cStoreScreenMgr::GetMovieXPos(), cStoreScreenMgr::GetMovieYPos());
    Vector2 size(cStoreScreenMgr::GetMovieWidth(), cStoreScreenMgr::GetMovieHeight());

    m_MovieId = CScaleformMgr::CreateMovie(STOREMENU_FILENAME, pos, size);

    m_bLoadingAssets = true;
    m_IsLoaded = false;

    for ( int i = 0; i < NUM_COLUMNS; i++ )
    {
        m_ImageStates[i] = AD_IMAGE_STATE_NONE;
    }

    m_DisplayingErrorDialog = false;

    m_ColumnRequestIndex = 0;
    m_CurrentColumn = 0;

}

void cStoreAdvertScreen::Shutdown()
{
    if ( m_MovieId != -1 )
    {
        if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
        {
            CScaleformMgr::EndMethod();
        }

        CScaleformMgr::RequestRemoveMovie(m_MovieId);
    }

    if ( m_ButtonMovieId != -1 )
    {
		CBusySpinner::UnregisterInstructionalButtonMovie(m_ButtonMovieId);
		CScaleformMgr::RequestRemoveMovie(m_ButtonMovieId);
    }

    ReleaseLoadedImages();

    m_MovieId = -1;
	m_ButtonMovieId = -1;
    m_IsActive = false;
    m_IsLoaded = false;
}

void cStoreAdvertScreen::Update()
{
    if ( m_bLoadingAssets )
    {
        if (AreAllAssetsActive())
        {
            if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "MENU_STATE"))
            {
                s32 menuStateParam = 0; //0 is the value we pass for the ad screen.
                CScaleformMgr::AddParamInt(menuStateParam);
                CScaleformMgr::EndMethod();
            }

            m_bLoadingAssets = false;
            
        }

        return;
   } 

    //We let asset loading proceed if the screen is not active, but otherwise we do not want to interfere with other screens.
    if (!m_IsActive)
    {
        ReleaseLoadedImages();
        return;
    }

    if (m_DisplayingErrorDialog)
    {

        CWarningScreen::SetMessage(WARNING_MESSAGE_STANDARD, "CMRC_GOTOSTORE");
        if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, true, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE) ||
            CPauseMenu::CheckInput(FRONTEND_INPUT_BACK, true, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE) )
        {
            m_DisplayingErrorDialog = false;

            if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
            {
                CScaleformMgr::EndMethod();
            }

            cStoreScreenMgr::RequestExit();
        }
    }
    else
    {
        UpdateInput();
    }
    
    if ( m_bPendingAdvertSetup && cStoreScreenMgr::GetAdvertData()->HasValidData() )
    {
        m_bPendingAdvertSetup = false;
        SetupAdverts();
        RequestImages();
        m_IsLoaded = true;
    }

    UpdateImageRequests();

    //The current button help state does not reflect the current data state, re-setup the buttons.
    if ( ( m_ButtonState == ONLY_CANCEL && CLiveManager::GetCommerceMgr().HasValidData() )
        || ( m_ButtonState == CANCEL_AND_CONFIRM && !CLiveManager::GetCommerceMgr().HasValidData() ) )
    {
        
        SetupButtons();
    }

    if ( m_ColumnRequestIndex != 0 )
    {
        if (CScaleformMgr::IsReturnValueSet(m_ColumnRequestIndex))
        {
            m_CurrentColumn = CScaleformMgr::GetReturnValueInt(m_ColumnRequestIndex);
            m_ColumnRequestIndex = 0;
        }
    }
}

void cStoreAdvertScreen::SetupAdverts()
{
    
    int numAdverts = 0;

    if ( cStoreScreenMgr::GetAdvertData()->HasValidData() )
    {
        numAdverts = cStoreScreenMgr::GetAdvertData()->GetNumAdvertDataRecords();
    }

    if ( numAdverts > NUM_COLUMNS )
    {
        numAdverts = NUM_COLUMNS;
    }
    

    storeUIDebugf3("Setting up adverts with %d records." ,numAdverts);

    for ( s32 iAds = 0; iAds < numAdverts; iAds++ )
    {
        SetupAd( iAds );
    }

    

    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_HEADER_COLOUR"))
    {
        CRGBA headerColour = CHudColour::GetRGBA(HUD_COLOUR_BLUE);
        CScaleformMgr::AddParamInt(headerColour.GetRed());
        CScaleformMgr::AddParamInt(headerColour.GetGreen());
        CScaleformMgr::AddParamInt(headerColour.GetBlue());
        CScaleformMgr::EndMethod();   
    }

    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_COLUMN_TITLE"))
    {
        CScaleformMgr::AddParamInt(3);
        CScaleformMgr::AddParamString("ADVERTISEMENTS");
        CScaleformMgr::EndMethod();   
    }

    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_SHOP_LOGO"))
    {
        CScaleformMgr::AddParamInt(cStoreScreenMgr::GetBranding());
        CScaleformMgr::EndMethod();   
    }

    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SHOW_SHOP_LOGO"))
    {
        CScaleformMgr::AddParamBool( cStoreScreenMgr::GetBranding() != 0 );
        CScaleformMgr::EndMethod();   
    }

#if __PPU
    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SHOW_PLAYSTATION_LOGO"))
    {
        CScaleformMgr::AddParamBool(true);
        CScaleformMgr::EndMethod();   
    }  
#endif

    SetupButtons();

    //Finally highlight column
    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "HIGHLIGHT_COLUMN"))
    {
        CScaleformMgr::AddParamInt(0);
        CScaleformMgr::EndMethod();
    }

   
}

void cStoreAdvertScreen::SetupButtons()
{
    //Setup the instructional buttons

    if ( m_ButtonMovieId >= 0 )
    {
        if ( CScaleformMgr::BeginMethod(m_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT_EMPTY"))
        {
            CScaleformMgr::EndMethod();
        }

        if ( CScaleformMgr::BeginMethod(m_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_CLEAR_SPACE"))
        {
            CScaleformMgr::AddParamInt(200);
            CScaleformMgr::EndMethod();
        }

        if ( CScaleformMgr::BeginMethod(m_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
        {
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamInt(31);
            CScaleformMgr::AddParamString(TheText.Get("CMRC_BACK"));
            CScaleformMgr::EndMethod();
        }

        if ( CLiveManager::GetCommerceMgr().HasValidData() )
        {
            if ( CScaleformMgr::BeginMethod(m_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
            {
                CScaleformMgr::AddParamInt(1);
                CScaleformMgr::AddParamInt(30);
                CScaleformMgr::AddParamString(TheText.Get("CMRC_SELECT"));
                CScaleformMgr::EndMethod();
            }

            if ( CScaleformMgr::BeginMethod(m_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_DATA_SLOT"))
            {
                CScaleformMgr::AddParamInt(2);
                CScaleformMgr::AddParamInt(33);
                CScaleformMgr::AddParamString(TheText.Get("CMRC_CONT"));
                CScaleformMgr::EndMethod();
            }

            m_ButtonState = CANCEL_AND_CONFIRM;
        }
        else
        {
            m_ButtonState = ONLY_CANCEL;
            
        }
        
        



        if ( CScaleformMgr::BeginMethod(m_ButtonMovieId, SF_BASE_CLASS_GENERIC, "DRAW_INSTRUCTIONAL_BUTTONS"))
        {
            //CScaleformMgr::AddParamInt(1);
            CScaleformMgr::EndMethod();
        }

        if ( CScaleformMgr::BeginMethod(m_ButtonMovieId, SF_BASE_CLASS_GENERIC, "SET_BACKGROUND_COLOUR"))
        {
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamInt(0);
            CScaleformMgr::AddParamInt(80);
            CScaleformMgr::EndMethod();
        }

    }
}

void cStoreAdvertScreen::UpdateInput()
{
    if (!AreAllAssetsActive())
        return;


    if (CPauseMenu::CheckInput(FRONTEND_INPUT_UP,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT"))
    {
        CScaleformMgr::AddParamInt(DPADUP);
        CScaleformMgr::EndMethod();
    }

    if (CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT"))
    {
        CScaleformMgr::AddParamInt(DPADDOWN);
        CScaleformMgr::EndMethod();
    }

    if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT"))
    {
        CScaleformMgr::AddParamInt(DPADLEFT);
        CScaleformMgr::EndMethod();

        if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "GET_CURRENT_COLUMN"))
        {
            m_ColumnRequestIndex = CScaleformMgr::EndMethodReturnValue(m_ColumnRequestIndex);
        }
    }

    if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT"))
    {
        CScaleformMgr::AddParamInt(DPADRIGHT);
        CScaleformMgr::EndMethod();

        if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "GET_CURRENT_COLUMN"))
        {
            m_ColumnRequestIndex = CScaleformMgr::EndMethodReturnValue(m_ColumnRequestIndex);
        }
    }

    if (CPauseMenu::CheckInput(FRONTEND_INPUT_BACK,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT_CIRCLE"))
    {
        CScaleformMgr::EndMethod();

        if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SHUTDOWN_MOVIE"))
        {
            CScaleformMgr::EndMethod();
        }

        cStoreScreenMgr::RequestExit();
    }

    bool goToMainStoreScreen = false;

    if ( CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT_CROSS") )
    {
        if ( cStoreScreenMgr::GetAdvertData() && cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(m_CurrentColumn)->GetLinkedProductId().GetLength() > 0 )
        {
            cStoreScreenMgr::SetInitialItemID( cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(m_CurrentColumn)->GetLinkedProductId() );
        }

        goToMainStoreScreen = true;
    }

    if ( CPauseMenu::CheckInput(FRONTEND_INPUT_Y,false,CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE) && CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_INPUT_EVENT_CROSS") )
    {
        goToMainStoreScreen = true;
    }

    if ( goToMainStoreScreen )
    {
        CScaleformMgr::EndMethod();

        

        if (CLiveManager::GetCommerceMgr().HasValidData())
        {

            //Clear the columns.
            for (s32 iClearCols=0; iClearCols < NUM_COLUMNS; iClearCols++)
            {
                if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT_EMPTY"))
                {
                    CScaleformMgr::AddParamInt(iClearCols);
                    CScaleformMgr::EndMethod();
                }
            }

            //We pass the movies to main screen initialisation here.
            cStoreScreenMgr::GetStoreMainScreen()->Init( m_MovieId, m_ButtonMovieId );

            //At this point we have handed off responsibility for shutting down the movies to the main store screen
            m_MovieId = -1;
            m_ButtonMovieId = -1;

            //Having handed off, we do not want to remain active.
            m_IsActive = false;
            
        }
     
    }
    
}

void cStoreAdvertScreen::Render()
{  
	float fTimeStep = fwTimer::GetSystemTimeStep();
	if ( m_MovieId >= 0)
	{
		CScaleformMgr::RenderMovie( m_MovieId, fTimeStep, true );
	}

#if RSG_PC
	// hide instructional buttons if SCUI is showing ...hints are confusing
	if (!g_rlPc.IsUiShowing())
#endif
	{
		if ( m_ButtonMovieId >= 0 )
		{
			CScaleformMgr::RenderMovie( m_ButtonMovieId, fTimeStep, true );
		}
	}
    
}

bool cStoreAdvertScreen::AreAllAssetsActive()
{
    bool retVal = ( m_MovieId != -1 && CScaleformMgr::IsMovieActive(m_MovieId));
    
    if ( m_ButtonMovieId != -1 && !CScaleformMgr::IsMovieActive(m_ButtonMovieId) )
    {
        retVal = false;
    }

    return retVal;
    
}

void cStoreAdvertScreen::CheckIncomingFunctions( atHashWithStringBank methodName, const GFxValue* /*args*/ )
{
    if ( !m_IsActive )
    {
        return;
    }

    if (methodName == ATSTRINGHASH("LOAD_PAUSE_MENU_STORE_CONTENT",0x879bc589))
    {
        storeUIDebugf3("Store Advert screen callback recieved.\n");
    
        m_bPendingAdvertSetup = true;

        return;
    }
}

void cStoreAdvertScreen::DumpDebugInfo()
{
}

void cStoreAdvertScreen::UpdateImageRequests()
{
    storeUIAssert( cStoreScreenMgr::GetTextureManager() );

    if ( cStoreScreenMgr::GetTextureManager() == NULL )
    {
        return;
    }

    for (int iAds = 0; iAds < NUM_COLUMNS; iAds++)
    {
        if ( m_ImageStates[ iAds ] == AD_IMAGE_STATE_REQUESTED )
        {
            if ( cStoreScreenMgr::GetTextureManager()->RequestTexture( cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(iAds)->GetImagePath() ) ) 
            {
                m_ImageStates[ iAds ] = AD_IMAGE_STATE_LOADED;
                SetupAd( iAds );
            }
        }
    }
}

void cStoreAdvertScreen::RequestImages()
{
    storeUIAssert( cStoreScreenMgr::GetTextureManager() );

    if ( cStoreScreenMgr::GetTextureManager() == NULL )
    {
        return;
    }

    for (int iAds = 0; iAds < NUM_COLUMNS; iAds++)
    {
        storeUIAssertf( m_ImageStates[iAds] == AD_IMAGE_STATE_NONE, "Image is either already loaded or already requested" );
        if ( m_ImageStates[iAds] == AD_IMAGE_STATE_LOADED )
        {
            cStoreScreenMgr::GetTextureManager()->ReleaseTexture( cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(iAds)->GetImagePath() );    
        }

        if ( m_ImageStates[iAds] == AD_IMAGE_STATE_REQUESTED )
        {
            continue;
        }

        if ( cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(iAds)->GetImagePath().GetLength() > 0 )
        {
            m_ImageStates[iAds] = AD_IMAGE_STATE_REQUESTED;
        }
        
    }
}

void cStoreAdvertScreen::SetupAd( int aAdIndex )
{
    storeUIAssertf(aAdIndex >= 0 && aAdIndex <= NUM_COLUMNS, "Invalid column index");
    if ( aAdIndex < 0 || aAdIndex >= NUM_COLUMNS )
    {
        return;
    }

    //Clear the existing center data slot of items.
    if ( CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT_EMPTY"))
    {
        CScaleformMgr::AddParamInt(aAdIndex);
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
    {
        atString cloudStrippedPath;
        
        if ( m_ImageStates[aAdIndex] == AD_IMAGE_STATE_LOADED )
        {
            cloudStrippedPath = cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(aAdIndex)->GetImagePath();
            cStoreTextureManager::ConvertPathToTextureName(cloudStrippedPath);
        }
        else
        {
            cloudStrippedPath = "";
        }

        CScaleformMgr::AddParamInt(aAdIndex);
        CScaleformMgr::AddParamInt(0);
        CScaleformMgr::AddParamString( cloudStrippedPath );
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
    {
        CScaleformMgr::AddParamInt(aAdIndex);
        CScaleformMgr::AddParamInt(1);
        CScaleformMgr::AddParamString( cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(aAdIndex)->GetTitle() );
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "SET_DATA_SLOT"))
    {
        CScaleformMgr::AddParamInt(aAdIndex);
        CScaleformMgr::AddParamInt(2);
        CScaleformMgr::AddParamString( cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(aAdIndex)->GetText() );
        CScaleformMgr::EndMethod();
    }

    if (CScaleformMgr::BeginMethod(m_MovieId, SF_BASE_CLASS_STORE, "DISPLAY_DATA_SLOT"))
    {
        CScaleformMgr::AddParamInt(aAdIndex);
        CScaleformMgr::EndMethod();
    }        
}

void cStoreAdvertScreen::ReleaseLoadedImages()
{
    //We are not active, unload assets or cancel requests.
    for ( int i = 0; i < NUM_COLUMNS; i++ )
    {
        if ( cStoreScreenMgr::GetTextureManager() && cStoreScreenMgr::GetAdvertData()->HasValidData() )
        {
            if ( m_ImageStates[i] == AD_IMAGE_STATE_LOADED  )
            {
                cStoreScreenMgr::GetTextureManager()->ReleaseTexture( cStoreScreenMgr::GetAdvertData()->GetAdvertRecord(i)->GetImagePath() );
            }

            if ( m_ImageStates[i] == AD_IMAGE_STATE_REQUESTED )
            {
                //TODO: Get the cancel method from Luis A
            }
        }

        m_ImageStates[i] = AD_IMAGE_STATE_NONE;
    }
}

void cStoreAdvertData::cStoreAdSaxReader::Start( cStoreAdvertData* pAdData )
{
    Assert(pAdData);

    m_pAdvertData = pAdData;
    m_State = STATE_EXPECT_ROOT;
    Begin();
}

void cStoreAdvertData::cStoreAdSaxReader::startElement( const char* /*url*/, const char* /*localName*/, const char* qName )
{
    switch( static_cast<int>(m_State) )
    {
    case STATE_EXPECT_ROOT:
        if(0 == stricmp("AdData", qName))
        {
            m_State = STATE_EXPECT_ADVERT;
        }
        else 
        {
            storeUIErrorf("Error parsing [STATE_EXPECT_ROOT] %s", qName);
        }
        break;
    case STATE_EXPECT_ADVERT:
        if(0 == stricmp("Advert", qName))
        {
            storeUIAssert( m_pCurrentAdvert == NULL );
            m_pCurrentAdvert = rage_new cStoreAdvert;

            m_pCurrentAdvert->Init();

            m_State = STATE_EXPECT_ADVERT_SUB_ELEMENT;
        }
        else 
        {
            storeUIErrorf("Error parsing [STATE_EXPECT_ADVERT] %s", qName);
        }
        break;
    case STATE_EXPECT_ADVERT_SUB_ELEMENT:
        if(0 == stricmp("Title", qName))
        {
            m_State = STATE_READING_ADVERT_TITLE;
        }
        else if(0 == stricmp("Text", qName))
        {
            m_State = STATE_READING_ADVERT_TEXT;
        }
        else if(0 == stricmp("ImagePath", qName))
        {
            m_State = STATE_READING_ADVERT_IMAGE_PATH;
        }
        else if(0 == stricmp("LinkedProductID", qName))
        {
            m_State = STATE_READING_ADVERT_LINKED_PRODUCT_ID;
        }
        else 
        {
            storeUIErrorf("Error parsing [STATE_EXPECT_ADVERT_SUB_ELEMENT] %s", qName);
        }
        break;
    default:
        storeUIErrorf("Error parsing [STATE_EXPECT_CATEGORY_SUB_ELEMENT] %s", qName);
    }
}

void cStoreAdvertData::cStoreAdSaxReader::endElement( const char* /*url*/, const char* /*localName*/, const char* qName )
{
    switch( static_cast<int>(m_State) )
    {
    case(STATE_READING_ADVERT_TITLE):
        if (0 == stricmp("Title", qName))
        {
            m_State = STATE_EXPECT_ADVERT_SUB_ELEMENT;
        }
        else
        {
            storeUIErrorf("Error parsing End Element [STATE_READING_ADVERT_TITLE] %s", qName);
        }
        break;
    case(STATE_READING_ADVERT_TEXT):
        if (0 == stricmp("Text", qName))
        {
            m_State = STATE_EXPECT_ADVERT_SUB_ELEMENT;
        }
        else
        {
            storeUIErrorf("Error parsing End Element [STATE_READING_ADVERT_TEXT] %s", qName);
        }
        break;
    case(STATE_READING_ADVERT_IMAGE_PATH):
        if (0 == stricmp("ImagePath", qName))
        {
            m_State = STATE_EXPECT_ADVERT_SUB_ELEMENT;
        }
        else
        {
            storeUIErrorf("Error parsing End Element [STATE_READING_ADVERT_IMAGE_PATH] %s", qName);
        }
        break;
    case(STATE_READING_ADVERT_LINKED_PRODUCT_ID):
        if (0 == stricmp("LinkedProductID", qName))
        {
            m_State = STATE_EXPECT_ADVERT_SUB_ELEMENT;
        }
        else
        {
            storeUIErrorf("Error parsing End Element [STATE_READING_ADVERT_LINKED_PRODUCT_ID] %s", qName);
        }
        break;
    case(STATE_EXPECT_ADVERT_SUB_ELEMENT):
        if (0 == stricmp("Advert", qName))
        {
            storeUIAssert( m_pCurrentAdvert != NULL );
            m_pAdvertData->m_pAdvertDataArray.Grow() = m_pCurrentAdvert;
            m_pCurrentAdvert = NULL;

            m_State = STATE_EXPECT_ADVERT;
        }
        else
        {
            storeUIErrorf("Error parsing End Element [STATE_EXPECT_ADVERT_SUB_ELEMENT] %s", qName);
        }
        break;
    case(STATE_EXPECT_ADVERT):
        if (0 == stricmp("AdData", qName))
        {
            m_State = STATE_EXPECT_ROOT;
        }
        else
        {
            storeUIErrorf("Error parsing End Element [STATE_EXPECT_ADVERT] %s", qName);
        }
        break;
    }
}

void cStoreAdvertData::cStoreAdSaxReader::attribute( const char* /*tagName*/, const char* attrName, const char* attrVal )
{
    switch( static_cast<int>(m_State) )
    {
    case(STATE_EXPECT_ADVERT_SUB_ELEMENT):
        if(0 == stricmp("id", attrName))
        {
            m_pCurrentAdvert->GetId() = attrVal;
        }
        break;
    case(STATE_READING_ADVERT_LINKED_PRODUCT_ID):
        if(0 == stricmp("id", attrName))
        {          
            m_pCurrentAdvert->GetLinkedProductId() = attrVal;            
        }
        break;
    }

}

void cStoreAdvertData::cStoreAdSaxReader::characters( const char* ch, const int start, const int length )
{
    atString temp;
    temp.Set( ch, istrlen(ch) , start, length );

    switch( static_cast<int>(m_State) )
    {
    case(STATE_READING_ADVERT_TITLE):
        m_pCurrentAdvert->GetTitle() += temp;
        storeUIDebugf3( "Set current advert title to: %s", m_pCurrentAdvert->GetTitle().c_str() );
        break;
    case(STATE_READING_ADVERT_TEXT):
        m_pCurrentAdvert->GetText() += temp;
        storeUIDebugf3( "Set current advert text to: %s", m_pCurrentAdvert->GetText().c_str() );
        break;
    case(STATE_READING_ADVERT_IMAGE_PATH):
        m_pCurrentAdvert->GetImagePath() += temp;
        storeUIDebugf3( "Set current advert image path to: %s", m_pCurrentAdvert->GetImagePath().c_str() );
        break;
    }
}

void cStoreAdvertData::cStoreAdSaxReader::ResetCurrentData()
{
    if ( m_pCurrentAdvert )
    {
        m_pCurrentAdvert->Init();
    }
}

cStoreAdvertData::cStoreAdvertData() :
    m_ParserState( AD_DATA_XML_WAITING ),
    m_bInitialised( false ),
    m_LocalGamerIndex(-1),
    m_bHasValidData( false ),
    m_bIsInErrorState( false ),
    m_bDataRefreshPending( false ),
    m_pLocalFileStream(NULL)

{

}

cStoreAdvertData::~cStoreAdvertData()
{
    ResetData();
}

void cStoreAdvertData::Init( int localUserIndex, const char* skuFolder )
{
    if ( m_bInitialised )
    {
        storeUIAssert(false);
        return;
    }

    ResetData();

    m_bInitialised = true;

    m_bHasValidData = false;
    m_bIsInErrorState = false;
    m_bDataRefreshPending = false;

    m_SkuDirString.Reset();
    if ( skuFolder != NULL )
    {
        m_SkuDirString = skuFolder;
    }

    m_LocalGamerIndex = localUserIndex;    
}

void cStoreAdvertData::Shutdown()
{
    if ( !m_bInitialised )
    {
        return;
    }

    ResetData();

    m_bInitialised = false;

   
    m_bHasValidData = false;
    m_bIsInErrorState = false;
    m_bDataRefreshPending = false;

    m_LocalGamerIndex = -1;

    m_ParserState = AD_DATA_XML_WAITING;
}

void cStoreAdvertData::Update()
{
    switch( m_ParserState )
    {
    case(AD_DATA_XML_WAITING):
        UpdateParserWaitingState();
        break;
    case(AD_DATA_XML_STARTUP):
        UpdateParserStartupState();
        break;
    case(AD_DATA_XML_RECEIVING):
        UpdateParserReceivingState();
        break;
    case(AD_DATA_XML_CLEANUP):
        UpdateParserCleanupState();
        break;
    default:
        storeUIAssert(false);
    }
}

void cStoreAdvertData::UpdateParserWaitingState()
{
    if(!RL_IS_VALID_LOCAL_GAMER_INDEX(m_LocalGamerIndex))
    {
        return;
    }

    //If we're loading a local file, we don't need credentials
    BANK_ONLY(bool bNeedCredentials = !PARAM_adDataLocalFile.Get());

    const rlRosCredentials& creds = rlRos::GetCredentials( m_LocalGamerIndex );
    if(!creds.IsValid() BANK_ONLY(&& bNeedCredentials) )
    {
        return;
    }

    if (m_bDataRefreshPending)
    {
        ResetData();
        m_bDataRefreshPending = false;
        m_bIsInErrorState = false;
        m_bHasValidData = false;
        m_ParserState = AD_DATA_XML_STARTUP;
    }
}

void cStoreAdvertData::UpdateParserStartupState()
{
    if(!RL_IS_VALID_LOCAL_GAMER_INDEX(m_LocalGamerIndex))
    {
        //No valid gamer index, flee.
        storeUIErrorf("No local gamer index for download of Commerce data xml");
        m_ParserState = AD_DATA_XML_CLEANUP;
        return;
    }

    m_GrowBuffer.Init(NULL, datGrowBuffer::NULL_TERMINATE);
    m_GrowBuffer.Preallocate(1024);


    atString cloudFilePath("store");

    if ( m_SkuDirString.GetLength() != 0 )
    {
        cloudFilePath += "/";
        cloudFilePath += m_SkuDirString;
    }

    cloudFilePath += "/advertdata";
    cloudFilePath += "/adData.xml";

    const char* fileName = NULL;
    if(PARAM_adDataLocalFile.Get(fileName) && fileName && strlen(fileName) > 0)
    {
        storeUIDebugf1("Loading catalogue file from local source: %s", fileName);
        m_pLocalFileStream = ASSET.Open(fileName, "");
        if (storeUIVerifyf(m_pLocalFileStream, "Failed to open local catalogue file %s", fileName))
        {
            m_AdDataXMLFetchStatus.Reset();
            m_AdDataXMLFetchStatus.SetPending();
            m_saxReader.Start(this);
            m_ParserState = AD_DATA_XML_RECEIVING;
        }
        else
        {
            m_AdDataXMLFetchStatus.Reset();
            m_AdDataXMLFetchStatus.SetFailed();
            m_ParserState = AD_DATA_XML_CLEANUP;
        }

    }
    else if ( rlCloud::GetTitleFile( cloudFilePath,
                                    RLROS_SECURITY_DEFAULT,
                                    0,  //ifModifiedSincePosixTime
                                    NET_HTTP_OPTIONS_NONE,
                                    m_GrowBuffer.GetFiDevice(),
                                    m_GrowBuffer.GetFiHandle(),
                                    NULL,   //rlCloudFileInfo
                                    NULL,   //allocator
                                    &m_AdDataXMLFetchStatus ) )
    {
        m_saxReader.Start(this);
        m_ParserState = AD_DATA_XML_RECEIVING;
    }
    else
    {
        storeUIErrorf("Failed to start download of advert data data file.");
        m_ParserState = AD_DATA_XML_CLEANUP;
        m_bIsInErrorState = true;
    }
}

void cStoreAdvertData::UpdateParserReceivingState()
{
    UpdateParserLocalFileState();

    if ( m_saxReader.Failed() )
    {
        //Something has gone wrong in the sax reader, probably an interuption or error code
        m_bIsInErrorState = true;
        m_ParserState = AD_DATA_XML_CLEANUP;
    }
    else if ( (m_AdDataXMLFetchStatus.Succeeded() || m_AdDataXMLFetchStatus.Pending()) )
    {
        if ( m_GrowBuffer.Length() != 0 )
        {
            const char* response = (const char*) m_GrowBuffer.GetBuffer();
            const unsigned responseLen = m_GrowBuffer.Length();


            if(response && responseLen)
            {
                storeUIDisplayf("Processing advert data of length %d: %s\n",responseLen,response);
                const unsigned numConsumed = m_saxReader.Parse(response, 0, responseLen);
                m_GrowBuffer.Remove(0, numConsumed);
            }
        }
    }
    else if ( m_AdDataXMLFetchStatus.Failed() )
    {
        storeUIErrorf("Ad data XML file fetch from cloud failed. Code: %d", m_AdDataXMLFetchStatus.GetResultCode());

        //Get cunning: if we have specified a SKU specific file, try the base version.
        if ( m_SkuDirString.GetLength() != 0 )
        {
            m_SkuDirString.Reset();
            m_bDataRefreshPending = true;
        }
        else
        {
            m_bIsInErrorState = true;
        }

        m_ParserState = AD_DATA_XML_CLEANUP;
    }
    else if ( m_AdDataXMLFetchStatus.Canceled() )
    {
        m_bIsInErrorState = true;
    }

    if ( m_AdDataXMLFetchStatus.Succeeded() && m_GrowBuffer.Length() == 0 )
    {
        storeUIDebugf3("Store advert Data retrieved and parsed.");
        m_bHasValidData = true;
        m_ParserState = AD_DATA_XML_CLEANUP;
    }
}

void cStoreAdvertData::UpdateParserCleanupState()
{
    if ( !m_saxReader.Failed() )
    {
        m_saxReader.End();
    }
    
    m_GrowBuffer.Clear();
    m_ParserState = AD_DATA_XML_WAITING;
}

void cStoreAdvertData::UpdateParserLocalFileState()
{
    if(PARAM_adDataLocalFile.Get() && m_AdDataXMLFetchStatus.Pending())
    {
        if (m_pLocalFileStream)
        {
            //Read it in as chunks, just like the cloud file would.
            static const int READ_LEN = 512;
            char bounceBuffer[READ_LEN];
            int numRead = m_pLocalFileStream->Read(bounceBuffer, READ_LEN);
            if (numRead > 0)
            {
                storeUIDebugf3("%s", bounceBuffer);
                m_GrowBuffer.Append(bounceBuffer, numRead);
            }

            //If we didn't read anything, or we didn't read as much as we aske for, it means we're done.
            if (numRead == 0 || numRead < READ_LEN || m_AdDataXMLFetchStatus.Canceled())
            {
                storeUIDebugf3("Finished parsing local file %s", m_pLocalFileStream->GetName());
                m_pLocalFileStream->Close();
                m_pLocalFileStream = NULL;

                if (m_AdDataXMLFetchStatus.Pending())
                {
                    m_AdDataXMLFetchStatus.SetSucceeded();
                }
            }
        }
    }	
}

bool cStoreAdvertData::StartDataFetch()
{
    if ( m_ParserState == AD_DATA_XML_WAITING )
    {
        m_bDataRefreshPending = true;
        return true;
    }
    else
    {
        storeUIDisplayf("Tried to call StartDataFetch when m_ParserState was not COMMERCE_XML_WAITING");
        return false;
    }
}

void cStoreAdvertData::AbortDataFetch()
{
    if ( m_AdDataXMLFetchStatus.GetStatus() == netStatus::NET_STATUS_PENDING )
    {
        rlCloud::Cancel( &m_AdDataXMLFetchStatus );
        m_AdDataXMLFetchStatus.SetCanceled(); 
    }
}

const bool cStoreAdvertData::IsInDataFetchState() const
{
    return ( m_AdDataXMLFetchStatus.GetStatus() == netStatus::NET_STATUS_PENDING );
}

void cStoreAdvertData::ResetData()
{
    //Delete the actual data items
    for ( int i = 0; i < m_pAdvertDataArray.GetCount(); i++ )
    {
        delete m_pAdvertDataArray[ i ];
    }

    //Clear up the array
    m_pAdvertDataArray.Reset();
}

const cStoreAdvert* cStoreAdvertData::GetAdvertRecord( int adIndex ) const
{
    if ( adIndex < 0 || adIndex > GetNumAdvertDataRecords() )
    {
        storeUIAssertf( false, "Invalid index to GetAdvertRecord" );
        return NULL;
    }

    return m_pAdvertDataArray[ adIndex ];
}

int cStoreAdvertData::GetNumAdvertDataRecords() const
{
    //storeUIDebugf3("GetNumAdvertDataRecords returning %d",m_pAdvertDataArray.GetCount());
    return m_pAdvertDataArray.GetCount();
}

