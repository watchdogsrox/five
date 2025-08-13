/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScaleformMgr.cpp
// PURPOSE : manages the processing of Scaleform movies in the game
// AUTHOR  : Derek Payne
// STARTED : 02/10/2009
//
/////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "fwscene/stores/txdstore.h"
#include "fwsys/FileExts.h"
#include "fwsys/timer.h"
#include "grcore/allocscope.h"
#include "grcore/effect.h"
#include "scaleform/renderer.h"
#include "Scaleform/scaleform.h"
#include "scr_scaleform/scaleform.h"
#include "script/thread.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingvisualize.h"
#include "string/stringutil.h"
#include "frontend/ui_channel.h"
#include "pathserver/ExportCollision.h"
#include "system/ipc.h"
#include "system/spinlock.h"

#if __BANK
#include "parser/manager.h"
#include "bank/bkmgr.h"
#include "text/text.h"
#endif // __BANK

// Game headers
#include "audio/frontendaudioentity.h"
#include "camera/CamInterface.h"
#include "Core/Game.h"
#include "Debug/TextureViewer/TextureViewer.h"
#include "Frontend/MovieCLipText.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "Frontend/Scaleform/ScaleFormStore.h"
#if RSG_PC
#include "Frontend/MultiplayerChat.h"
#endif // RSG_PC
#include "game/Clock.h"
#include "peds/ped.h"
#include "renderer/rendertargets.h"
#include "scene/world/GameWorld.h"
#include "Script/Script_Hud.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"
#include "system/param.h"
#include "system/appcontent.h"
#include "frontend/hud_colour.h"
#include "frontend/MiniMap.h"
#include "frontend/MousePointer.h"
#include "frontend/NewHud.h"
#include "frontend/HudTools.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/PauseMenu.h"
#include "frontend/ReportMenu.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "text/text.h"
#include "text/TextFile.h"
#include "text/TextConversion.h"
#include "text/TextFormat.h"

#if RSG_PC
#include "rline/rlpc.h"
#endif // RSG_PC

SCALEFORM_OPTIMISATIONS();
//OPTIMISATIONS_OFF()

#define SCALEFORM_ACTIVE	(1)

#if __DEV
PARAM(supressspamming, "Try to hide spamming that originates from script");
PARAM(noscaleformprealloc, "Don't preallocate memory for scaleform movies");
PARAM(loadallsfmovies, "Loads each scaleform movie, one at a time");
PARAM(logactionscriptrefs, "Logs any actionscript refs that get added to movies");
#endif  // __DEV

#if __BANK
PARAM(forceremoval, "forces removal of movies even if it has dangling refs");
PARAM(actionscriptdebug, "calls SHOW_DEBUG when movies initialise");
PARAM(showsfperformance, "Shows scaleform performance info on startup");
PARAM(sfgranularity, "Sets the default scaleform granularity");
PARAM(displayvalidsfmethods, "Sets the default scaleform granularity");
PARAM(logExtendedScaleformOutput, "Logs Scaleform output that may produce too much spam for any standard debug channel");
#endif // __BANK

PARAM(lockscaleformtogameframe, "Lock the scaleform framerate to the game's framerate by default (some movies may override this)");

#if TRACK_STALE_REFERENCES
SFStaleList  sm_StaleList ORBIS_ONLY(__attribute__((init_priority(106))));
#endif

#if TRACK_FLIP_BUFFERS
u32 CScaleformMgr::sm_BeginFrameCount = 0;
#endif

ScaleformMethodStruct CScaleformMgr::sm_ScaleformMethodBuild[MAX_METHOD_BUILD_BUFFERS];
atArray<ScaleformMovieStruct> CScaleformMgr::sm_ScaleformArray;
sfScaleformManager*		CScaleformMgr::m_staticMovieMgr = NULL;
char	CScaleformMgr::sm_LastMovieName[255];
char	CScaleformMgr::sm_LastMethodName[255];
char	CScaleformMgr::sm_LastMethodParamName[MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING];
bool	CScaleformMgr::sm_bIsInsideActionScript=false;
char*	CScaleformMgr::sm_CurrTextureDictName = NULL;

Vector2 CScaleformMgr::ms_vScreenSize;

#define MAX_NUM_OF_REFS_PER_TXD_PER_MOVIE_BEFORE_WARNING (40)
#define MAX_UNIQUE_RETURN_COUNTER (1000)
#define MAX_CHARS_FOR_TEXT_STRING_IN_PARAM	(4096)  // we need to cater for very large strings.  This is now only used for local strings however

s32 CScaleformMgr::ms_iGlobalReturnCounterUT = 1;
s32 CScaleformMgr::ms_iGlobalReturnCounterRT = MAX_UNIQUE_RETURN_COUNTER+1;
s32 CScaleformMgr::ms_iReturnValueCount = 0;

#if __BANK
#define SCALEFORM_BANK_NAME	"Scaleform"  // scaleform Bank widget name
float CScaleformMgr::ms_emissiveLevel = 0.0f;
bool CScaleformMgr::ms_bOverrideEmissiveLevel = false;
s32 CScaleformMgr::ms_iDebugMovieId = -1;
char CScaleformMgr::ms_cDebugMovieName[256];
char CScaleformMgr::ms_cDebugMethod[256];
char CScaleformMgr::ms_cDebugLoadedMovieName[256];
char CScaleformMgr::ms_cDebugIgnoreMethod[256];
Vector2 CScaleformMgr::ms_vDebugMoviePos(0.0f,0.0f);
Vector2 CScaleformMgr::ms_vDebugMovieSize(1.0f, 1.0f);
bool CScaleformMgr::ms_bPrintDebugInfoLog = false;
bool CScaleformMgr::ms_bVerifyActionScriptMethods = false;
bool CScaleformMgr::ms_bDebugOutputOn = false;
bool CScaleformMgr::ms_bFilterAwayDebugLog = true;
bool CScaleformMgr::ms_bShowOutlineFor3DMovie = false;
bool CScaleformMgr::ms_bShowOutlineFor2DMovie = false;
bool CScaleformMgr::ms_bWildcardNameisRecursive = true;
Vector2 CScaleformMgr::ms_vTextOverride(0.0f,0.0f);
bool CScaleformMgr::ms_bDrawStandardVariables = false;
bool CScaleformMgr::ms_bShowMemberVariables = false;
bool CScaleformMgr::ms_bFullNames = false;
int CScaleformMgr::ms_iMaxMemberVariableDepth = 1;
atHashWithStringBank CScaleformMgr::ms_OutlineFilter;
bool CScaleformMgr::ms_bShowPerformanceInfo = false;
bool CScaleformMgr::ms_bShowPreallocationInfo = false;
bool CScaleformMgr::ms_bShowAllPreallocationInfo = false;
bool CScaleformMgr::ms_bShowRefCountInfo = false;
bool CScaleformMgr::ms_bShowExtraDebugInfo = false;
#endif

atArray<ScaleformMethodStruct> CScaleformMgr::sm_MethodInvokeInfo[2];
atArray<ScaleformMethodStruct> CScaleformMgr::sm_MethodCallbackInfo[2];
ScaleformMethodReturnStruct CScaleformMgr::sm_ReturnValuesFromInvoke[MAX_STORED_RETURN_VALUES];
ScaleformMethodReturnStruct CScaleformMgr::sm_ReturnValuesForAccess[MAX_STORED_RETURN_VALUES];
s32 CScaleformMgr::sm_iMethodInvokeInfoBufferInUse = 0;
bool CScaleformMgr::ms_bExternalInterfaceCalled[3] = {false, false, false};
bool CScaleformMgr::ms_bFrameBegun = false;
bool CScaleformMgr::ms_bfullScreenBlockedInputLastFrame = false;
s32 CScaleformMgr::ms_iFontMovieId = -1;

#if __SCALEFORM_CRITICAL_SECTIONS
atRangeArray<sysCriticalSectionToken,MAX_SCALEFORM_MOVIES> gs_ScaleformMovieCS;
#endif  // __SCALEFORM_CRITICAL_SECTIONS
sysCriticalSectionToken CScaleformMgr::sm_RealSafeZoneToken;
sysCriticalSectionToken CScaleformMgr::sm_InvokeFlushToken;

#define MOVIE_STREAMING_FLAGS (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE)

grcBlendStateHandle			CScaleformMgr::ms_defDiffuseBlendState;
grcBlendStateHandle			CScaleformMgr::ms_defEmissiveBlendState;
grcDepthStencilStateHandle	CScaleformMgr::ms_defDiffuseDepthStencilState;
grcDepthStencilStateHandle	CScaleformMgr::ms_defEmissiveDepthStencilState;
grcRasterizerStateHandle	CScaleformMgr::ms_defDiffuseRasterizerState;
grcRasterizerStateHandle	CScaleformMgr::ms_defEmissiveRasterizerState;

grcBlendStateHandle			CScaleformMgr::ms_defExitBlendState;
grcDepthStencilStateHandle	CScaleformMgr::ms_defExitDepthStencilState;
grcRasterizerStateHandle	CScaleformMgr::ms_defExitRasterizerState;

#if __BANK
GFxResource*				CScaleformMgr::ms_watchedResource = NULL;

extern void (*sm_sfAddRefCB)(GFxResource*, int, int);
extern void (*sm_sfDecRefCB)(GFxResource*, int, int);

sysCriticalSectionToken g_ScaleformAppendValidMethodsCS;
#endif

#if SUPPORT_MULTI_MONITOR
// Some configurations have current window size smaller than the buffer size.
// Hence, we check if the actual size is within this margin of the full-screen size.
const unsigned s_FullScreenMargin = 16;
#endif

////////////////////////////////////////////////////////////////////////////////
// Data file loader interface
////////////////////////////////////////////////////////////////////////////////

class CScaleformPreallocationDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		//@TODO: Possible optimization. Check if there are compatibility packs in EXTRACONTENT. Only load this file if it's triggered from the newest one
		switch(file.m_fileType)
		{
		case  CDataFileMgr::SCALEFORM_PREALLOC_FILE:
			CScaleformMgr::LoadPreallocationInfo(file.m_filename);
		break;
		case  CDataFileMgr::SCALEFORM_DLC_FILE:
			CScaleformMgr::LoadDLCData(file.m_filename);
		break;
		default:
		break;		
		}
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file)
	{
		//@TODO: Make EXTRACONTENT trigger again its newest compatibility pack PreAllocation file. If there are none, call LoadPreallocationInfo(SCALEFORM_PREALLOC_XML_FILENAME);
		switch(file.m_fileType)
		{
		case  CDataFileMgr::SCALEFORM_DLC_FILE:
			CScaleformMgr::UnloadDLCData(file.m_filename);
			break;

		default:
			break;
		}
	}

} g_ScaleformPreallocationDataFileMounter;

fwLinkList<CScaleformMgr::CSorted3DMovie>	CScaleformMgr::ms_Sorted3DMovies;
Vector3 CScaleformMgr::ms_LastCameraPos;

static __THREAD ScaleformMovieStruct* g_CurrentMovie = NULL;
static __THREAD s32 g_CurrentTxd = -1;

static bool g_DontDoUpdateAtEndOfFrame = false;

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	class sfCustomGFxURLBuilder : public GFxURLBuilder
// PURPOSE: GTA5 custom URL builder
/////////////////////////////////////////////////////////////////////////////////////
class sfCustomGFxURLBuilder : public GFxURLBuilder, CScaleformMgr
{
public:
	// Default TranslateFilename implementation.
	void    BuildURL(GString *ppath, const LocationInfo& loc)
	{
		DefaultBuildURL(ppath, loc);
	}

	void    DefaultBuildURL(GString *ppath, const LocationInfo& loc)
	{
		*ppath = loc.FileName;

		sfDebugf3("Importing %d %s", loc.Use, ppath->ToCStr());


		// A number of import types can import gfx file (at least Regular, Import and LoadMovie). But we know if we're importing an image
		// we don't need to bother with some of these checks, since they're meant for converting one .gfx name into another

		// we need to check for certain files (ie font library) and revise the filename that we use to the one that we
		// have loaded in (ie platform specific file and the language used)
		if (ppath && loc.Use != File_ImageImport)
		{
			bool bReplaceFontFile = false;

			u32 pathHash = atStringHash(ppath->ToCStr());
			if (pathHash == ATSTRINGHASH("font_lib_efigs.gfx", 0xe0d4994e))
			{
				bReplaceFontFile = true;
			}

			if (CText::IsAsianLanguage())
			{
				if ((pathHash == ATSTRINGHASH("font_lib_sc.gfx", 0xfb300697)) ||
					(pathHash == ATSTRINGHASH("font_lib_heist.gfx", 0xe764f5d6)) ||
					(pathHash == ATSTRINGHASH("font_lib_web.gfx", 0x64a96611)) )
				{
					bReplaceFontFile = true;
				}
			}

			if (bReplaceFontFile)
			{
				*ppath = CText::GetFullFontLibFilename();
			}
		}

		const char* extn = ASSET.FindExtensionInPath(ppath->ToCStr());

		if (loc.Use != File_ImageImport)
		{

			const char *pOriginalFilename = CScaleformMgr::FindOriginalFilename(ppath->ToCStr());
			if (pOriginalFilename && pOriginalFilename[0] != '\0')
			{
				sfDebugf1("ScaleformMgr: (DefaultBuildURL) Trying to open %s so using %s", ppath->ToCStr(), pOriginalFilename);
				*ppath = pOriginalFilename;

				return;
			}
		}

			// See if there is a texture dictionary name AND that this texture is in the current dictionary
			// and prepend it so that two textures with the same name
			// from different dictionaries don't have the same URL as far as scaleform is concerned.
		if (loc.Use == File_ImageImport && CScaleformMgr::sm_CurrTextureDictName && !strcmp(extn, ".dds"))
		{
			pgDictionary<grcTexture>* texDict = pgDictionary<grcTexture>::GetCurrent();

			if (Verifyf(texDict, "missing texture dictionary"))
			{
				char baseName[RAGE_MAX_PATH];
				ASSET.BaseName(baseName, RAGE_MAX_PATH, ppath->ToCStr());
				if (texDict->LookupLocal(baseName))
				{
					*ppath = CScaleformMgr::sm_CurrTextureDictName;
					*ppath += '/';
					*ppath += loc.FileName;
					return;
				}
			}
		}

		//
		// otherwise do the standard Scaleform stuff...
		//

		GFxURLBuilder::DefaultBuildURL(ppath, loc);
	}
};



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	class sfCustomGFxImageLoader : public GFxImageLoader
// PURPOSE: GTA5 custom image loader - allows texture dictionaries that are loaded in
//			on the fly by movies using REQUEST_TXD_AND_ADD_REF to be used in movies
//			using the 'img://' tags (html tags or LoadBitmap
/////////////////////////////////////////////////////////////////////////////////////
class sfCustomGFxImageLoader : public GFxImageLoader, CScaleformMgr
{
public:
	GImageInfoBase* LoadImage(const char* purl)
	{
		// construct a texture dictionary name and a texture name from the url we get:
		char cTxdName[RAGE_MAX_PATH];
		char cTextureName[RAGE_MAX_PATH];

		cTxdName[0] = '\0';
		cTextureName[0] = '\0';

		if (sfVerifyf(purl, "Actionscript passed in invalid purl int LoadImage"))
		{
			if (sfVerifyf(CScaleformMgr::GetTxdAndTextureNameFromText(&purl[0], &cTxdName[0], &cTextureName[0]), "CScaleformMgr: GetTxdAndTextureNameFromText failed with params ('%s', '%s', '%s')", purl, cTxdName, cTextureName))
			{
				// Make sure our renderer is initialised - we need it to create sfTextures
				sfRendererBase* renderer = &CScaleformMgr::GetMovieMgr()->GetRenderer();
				sfAssertf(renderer, "No renderer has been set up - do this before loading movies so we can load textures from resources");

				// find the txd and texture and create the image to use:
				strLocalIndex iTxdId = g_TxdStore.FindSlot(cTxdName);

				g_CurrentTxd = iTxdId.Get();

				if (sfVerifyf(iTxdId != -1, "Invalid or missing slot %s", cTxdName))
				{
					sysMemUseMemoryBucket b(CScaleformMgr::GetMovieMgr()->sm_MemoryBucket);

					// Check the current texture dictionaries for the texture
					pgDictionary<grcTexture>* texDict = g_TxdStore.Get(iTxdId);

					if (sfVerifyf(texDict, "texture dictionary not resident at index %d (%s)", iTxdId.Get(), cTxdName))
					{
						grcTexture *pTexture = texDict->LookupLocal(cTextureName);
						if (pTexture)
						{
							sfTextureBase* tex = renderer ? renderer->CreateTexture(pTexture) : NULL;
							if (tex)
							{
								// OK we found the grcTexture, we've created the sfTexture, so
								// build a new GImageInfo for it
								GImageInfoBase* newInfo = rage_new GImageInfo(tex, pTexture->GetWidth(), pTexture->GetHeight());

								tex->Release();

								g_CurrentTxd = -1;
								return newInfo;
							}
						}
					}
				}

				g_CurrentTxd = -1;
			}
		}

		return NULL;
	}
};



//////////////////////////////////////////////////////////////////////////

void CScaleformMovieWrapper::Param::AddParam()
{
	switch(m_data.param_type)
	{
	case PARAM_TYPE_INVALID:
		break;
	case PARAM_TYPE_BOOL:
		CScaleformMgr::AddParamBool(m_data.bParamBool);
		break;
	case PARAM_TYPE_NUMBER:
		CScaleformMgr::AddParamFloat(m_data.ParamNumber);
		break;
	case PARAM_TYPE_STRING:
	{
		sfAssertf(0, "ScaleformMgr: CScaleformMovieWrapper using PARAM_TYPE_STRING directly - something went wrong as should be using PARAM_TYPE_EXTERNAL_STRING");
		break;
	}
	case PARAM_TYPE_EXTERNAL_STRING:
	{
		if (m_data.pParamString)
		{
			CScaleformMgr::AddParamString((char*)m_data.pExternalString, m_convertToHtml);
		}
		break;
	}
	case PARAM_TYPE_GFXVALUE:
		CScaleformMgr::AddParamGfxValue(*m_data.pParamGfxValue);
		break;
	}
}

void CScaleformMovieWrapper::Render(bool bForceUpdateBeforeRender /* = false */, float fAlpha /* = 1.0f */)
{
	if (CScaleformMgr::IsMovieActive(GetMovieID()))
	{
		CScaleformMgr::RenderMovie(GetMovieID(), m_bUseSystemTime?fwTimer::GetSystemTimeStep():0.0f, true, bForceUpdateBeforeRender, fAlpha);
	}
}

void CScaleformMovieWrapper::AffectRenderSize(float fScalar)
{
	if (CScaleformMgr::IsMovieActive(GetMovieID()))
	{
		CScaleformMgr::AffectRenderSizeOnly(GetMovieID(), fScalar);
	}
}

bool CScaleformMovieWrapper::SetDisplayConfig(CScaleformMgr::SDC flags)
{
	if (CScaleformMgr::IsMovieActive(GetMovieID()))
	{
		return CScaleformMgr::SetMovieDisplayConfig(m_iMovieId, m_iBaseClass, flags);
	}

	return false;
}

void CScaleformMovieWrapper::CreateMovie(eSCALEFORM_BASE_CLASS iBaseClass, const char* pFilename, Vector2 vPos, Vector2 vSize, bool bRemovable, s32 iParentMovie, s32 iDependentMovie, bool bRequiresMovieView, eMovieOwnerTags calledBy, bool bIgnoreSuperWidescreenAdjustment)
{
	m_iBaseClass = iBaseClass;
	if (uiVerifyf(!IsActive(), "CScaleformMovieWrapper::CreateMovie - Trying to create a menu movie when it's already created."))
	{
		m_iMovieId = CScaleformMgr::CreateMovie(pFilename, vPos, vSize, bRemovable, iParentMovie, iDependentMovie, bRequiresMovieView, calledBy, false, bIgnoreSuperWidescreenAdjustment);
	}
}

void CScaleformMovieWrapper::CreateMovieAndWaitForLoad(eSCALEFORM_BASE_CLASS iBaseClass, const char* pFilename, Vector2 vPos, Vector2 vSize, bool bRemovable, s32 iParentMovie, s32 iDependentMovie, bool bRequiresMovieView, eMovieOwnerTags calledBy, bool bIgnoreSuperWidescreenAdjustment)
{
	m_iBaseClass = iBaseClass;
	if (uiVerifyf(!IsActive(), "CScaleformMovieWrapper::CreateMovie - Trying to create a menu movie when it's already created."))
	{
		m_iMovieId = CScaleformMgr::CreateMovieAndWaitForLoad(pFilename, vPos, vSize, bRemovable, iParentMovie, iDependentMovie, bRequiresMovieView, calledBy, false, bIgnoreSuperWidescreenAdjustment);
	}
}

void CScaleformMovieWrapper::RemoveMovie()
{
	if(m_iMovieId != INVALID_MOVIE_ID)
	{
		if (uiVerifyf(CScaleformMgr::HasMovieGotAnyState(m_iMovieId), "CScaleformMovieWrapper::RemoveMovie - Trying to remove movie (%d) that has already been removed", m_iMovieId))  // we need to still be able to remove movies even if they are not yet fully active (i.e. they may have started to stream the movie so we need to set it to be removed even if it is not fully active)
		{
			CScaleformMgr::RequestRemoveMovie(m_iMovieId);  // remove the movie
		}
	}
	Clear();
}

void CScaleformMovieWrapper::Clear()
{
	m_iMovieId = INVALID_MOVIE_ID;
}

bool CScaleformMovieWrapper::CallMethod(const char* method,
										Param param1/*=Param()*/, Param param2/*=Param()*/, Param param3/*=Param()*/, Param param4/*=Param()*/)
{
	if(BeginMethod(method))
	{
		param1.AddParam();
		param2.AddParam();
		param3.AddParam();
		param4.AddParam();
		EndMethod();
		return true;
	}
	return false;
}

bool CScaleformMovieWrapper::BeginMethod(const char* method)
{
	return CScaleformMgr::BeginMethod(GetMovieID(), m_iBaseClass, method);
}

void CScaleformMovieWrapper::EndMethod()
{
	CScaleformMgr::EndMethod();
}

s32 CScaleformMovieWrapper::EndMethodReturnValue(const s32 iExistingReturnValue)
{
	return(CScaleformMgr::EndMethodReturnValue(iExistingReturnValue));
}

void CScaleformMovieWrapper::AddParam(Param param)
{
	param.AddParam();
}

void CScaleformMovieWrapper::AddParamString(const char *pParam, bool bConvertToHtml /*= true*/)
{
	CScaleformMgr::AddParamString(pParam, bConvertToHtml);
}

void CScaleformMovieWrapper::AddParamLocString(const char *pParam, bool bConvertToHtml /*= true*/)
{
	CScaleformMgr::AddParamLocString(pParam, bConvertToHtml);
}

void CScaleformMovieWrapper::AddParamLocString(u32 hash, bool bConvertToHtml /*= true*/)
{
	CScaleformMgr::AddParamLocString(hash, bConvertToHtml);
}

bool CScaleformMovieWrapper::IsActive() const
{
	if( m_iMovieId == INVALID_MOVIE_ID )
		return false;

	return CScaleformMgr::IsMovieActive(m_iMovieId);
}

CScaleformMovieWrapper::CScaleformMovieWrapper(bool bUseSystemTime) : m_iMovieId(INVALID_MOVIE_ID), m_iBaseClass(SF_BASE_CLASS_GENERIC), m_bUseSystemTime(bUseSystemTime)
{
#if TRACK_STALE_REFERENCES
	sm_StaleList.push_back(this);
#endif
}

CScaleformMovieWrapper::CScaleformMovieWrapper(const char* pFilename, eSCALEFORM_BASE_CLASS iBaseClass, bool bUseSystemTime) 
	: m_iMovieId(INVALID_MOVIE_ID), m_iBaseClass(iBaseClass), m_bUseSystemTime(bUseSystemTime)
{
#if TRACK_STALE_REFERENCES
	sm_StaleList.push_back(this);
#endif
	CreateMovie(m_iBaseClass, pFilename);
}
CScaleformMovieWrapper::~CScaleformMovieWrapper()
{
	RemoveMovie();
#if TRACK_STALE_REFERENCES
	sm_StaleList.erase(this);
#endif
}

void CScaleformMovieWrapper::Shutdown()
{
#if TRACK_STALE_REFERENCES
	for(SFStaleList::const_iterator stales( sm_StaleList.begin() ); stales!=sm_StaleList.end(); ++stales )
	{
		sfAssertf( !CScaleformMgr::HasMovieGotAnyState((*stales)->GetMovieID()), "Movie Wrapper pointing to '%s' is not cleaned up. THIS IS PRETTY DARN BAD!!!!", CScaleformMgr::GetMovieFilename((*stales)->GetMovieID()));
		sfAssertf( (*stales)->GetMovieID()==-1, "Movie Wrapper pointing to '%s' is not zero'd out. Any further work on this could go bad.", CScaleformMgr::GetMovieFilename((*stales)->GetMovieID()));
	}
#endif
}

//////////////////////////////////////////////////////////////////////////

void CScaleformMgr::LoadDLCData(const char* pFileName)
{
#if !__GAMETOOL
	if(parTree* pTree = PARSER.LoadTree(pFileName,""))
	{
		parTreeNode* pRootNode = pTree->GetRoot();
		parTreeNode* pNodeMovie = NULL;
		
		while ((pNodeMovie = pRootNode->FindChildWithName("movie", pNodeMovie)) != NULL)
		{
			const char* movieName = NULL;
			if(pNodeMovie->FindValueFromPath("@name", movieName))
			{
				strLocalIndex slot = g_ScaleformStore.FindSlot(movieName);
				int idx=0;
				for(idx=0;idx<sm_ScaleformArray.GetCount();idx++)
				{
					if(sm_ScaleformArray[idx].iMovieId == slot.Get())
 					{
						Displayf("Found a movie in the scaleform array (running): %s",sm_ScaleformArray[idx].cFilename);
						DestroyMovieNow(idx);
					}
				}
				if(slot!=-1)
				{
					CScaleformDef* pDef = g_ScaleformStore.GetSlot(strLocalIndex(slot));
					pDef->ResetPrealloc();
					strLocalIndex txdSlot = g_ScaleformStore.GetParentTxdForSlot(strLocalIndex(slot));
					if(txdSlot!=-1)
					{
						Displayf("Removing %s with index %d", g_TxdStore.GetName(txdSlot), txdSlot.Get());
						strStreamingEngine::GetInfo().RemoveObjectAndDependents(g_TxdStore.GetStreamingIndex(txdSlot), HMT_DISABLE);
					}
					else
					{
						strStreamingEngine::GetInfo().RemoveObject(g_ScaleformStore.GetStreamingIndex(slot));
					}
				}
				FixupScaleformAllocation(pNodeMovie,movieName);
				HandleNewTextureDependencies();
				if(atStringHash("hud")==atStringHash(movieName))
				{
					CNewHud::Restart();
				}			
			}
		}
		delete pTree;
	}
#else
	(void)pFileName;
#endif
}

void CScaleformMgr::UnloadDLCData(const char* pFileName)
{
	if(parTree* pTree = PARSER.LoadTree(pFileName,""))
	{
		parTreeNode* pRootNode = pTree->GetRoot();
		parTreeNode* pNodeMovie = NULL;

		while ((pNodeMovie = pRootNode->FindChildWithName("movie", pNodeMovie)) != NULL)
		{
			const char* movieName = NULL;
			if(pNodeMovie->FindValueFromPath("@name", movieName))
			{
				strLocalIndex slot = g_ScaleformStore.FindSlot(movieName);
				
				int idx=0;
				for(idx=0;idx<sm_ScaleformArray.GetCount();idx++)
				{
					if(sm_ScaleformArray[idx].iMovieId == slot.Get())
					{
						Displayf("Found a movie in the scaleform array (running): %s",sm_ScaleformArray[idx].cFilename);
						RequestRemoveMovie(idx);
					}
				}

				// Ensure movies are available for deletion.
				UpdateMoviesUntilReadyForDeletion();

				if(slot!=-1)
				{
					//CScaleformDef* pDef = g_ScaleformStore.GetSlot(slot);

					g_ScaleformStore.StreamingRemove(slot);

					strLocalIndex txdSlot = g_ScaleformStore.GetParentTxdForSlot(slot);
					if(txdSlot!=-1)
					{
						strStreamingEngine::GetInfo().RemoveObjectAndDependents(g_TxdStore.GetStreamingIndex(txdSlot), HMT_DISABLE);
					}
					else
					{
						strStreamingEngine::GetInfo().RemoveObject(g_ScaleformStore.GetStreamingIndex(slot));
					}
				}		
			}
		}
		delete pTree;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void CScaleformMgr::FixupScaleformAllocation(parTreeNode* pNodeMovie, const char* movieName)
{
	bool isMP = false;
	pNodeMovie->FindValueFromPath("@multiplayer", isMP);

	int peakSize = 0;
	pNodeMovie->FindValueFromPath("@peakSize", peakSize);
	peakSize *= 1024;

	bool useGameHeap = false;
	const char* heap = NULL;
	pNodeMovie->FindValueFromPath("@heap", heap);
	if (heap && !stricmp(heap, "game"))
	{
		useGameHeap = true;
	}

	int granularityKb = 16;
#if __BANK
	PARAM_sfgranularity.Get(granularityKb);
#endif
	pNodeMovie->FindValueFromPath("@granularity", granularityKb);

	const char* platform = NULL;
	if (pNodeMovie->FindValueFromPath("@platforms", platform))
	{
		if (!strstr(platform, RSG_PLATFORM_ID))
		{
			return;
		}
	}
	const char* addFonts = NULL;
	pNodeMovie->FindValueFromPath("@addFonts", addFonts);
	s32 addFontsIdx = -1;
	if (addFonts)
	{
		addFontsIdx = g_ScaleformStore.FindSlot(addFonts).Get();
	}


	strLocalIndex slot = g_ScaleformStore.FindSlot(movieName);

	if (slot.Get() >= 0)
	{
		datResourceInfo rscInfo;
		memset(&rscInfo, 0, sizeof(rscInfo));

		size_t newGranularity = granularityKb * 1024;

		bool useSfallocFormat = false;
		if (pNodeMovie->FindValueFromPath("@sfalloc", useSfallocFormat) && useSfallocFormat)
		{
			sfPreallocatedMemoryWrapper::AllocsRequiredArray allocsReq;
			for(parTreeNode::ChildNodeIterator iter = pNodeMovie->BeginChildren(); iter != pNodeMovie->EndChildren(); ++iter)
			{
				sfPreallocatedMemoryWrapper::ScaleformAllocStat& allocStat = allocsReq.Append();

				allocStat.m_Size = 0;
				(*iter)->FindValueFromPath("Size", (int&)allocStat.m_Size);
				allocStat.m_Size *= 1024;

				allocStat.m_Count = 0;
				(*iter)->FindValueFromPath("Count", (int&)allocStat.m_Count);
			}

			sfPreallocatedMemoryWrapper::ComputePreallocation(allocsReq, granularityKb * 1024, rscInfo, newGranularity);
		}
		else
		{
			// Given a granularity of N, these are our page size limits:
			// 1 1N page		(HasTail4)
			// 1 2N page		(HasTail2)
			// 127 4N pages		(BaseCount)
			// 63 8N pages		(Head2Count)
			// 15 16N pages		(Head4Count)
			// 3 32N pages		(Head8Count)

			u32 leafSize = g_rscVirtualLeafSize;
			while(leafSize < 4 * granularityKb * 1024) // "4 *" because we want to find BaseCount, but it's the HasTail4-sized page that should be granularity Kbs.
			{
				rscInfo.Virtual.LeafShift++;
				leafSize <<= 1;
			}

			char attribName[32];

			int val = 0;
			int carry = 0;
			int totalPages = 0;
			formatf(attribName, "Pages%dk", granularityKb);
			if (pNodeMovie->FindValueFromPath(attribName, val))
			{
				sfAssertf(val < 2, "Only 1 %dk page allowed (%s) and why are you using the old prealloc format?", granularityKb, movieName);

				val += carry;
				carry = 0;
				if (val >= 1)
				{
					int overflow = val - 1;
					carry = (overflow + 1)/2; // round the number of pages up
					val -= carry * 2;
				}
				totalPages += val;
				rscInfo.Virtual.HasTail4 = (val == 1);
			}

			val = 0;
			formatf(attribName, "Pages%dk", 2 * granularityKb);
			if (pNodeMovie->FindValueFromPath(attribName, val) || carry > 0)
			{
				sfAssertf(val < 2, "Only 1 %dk page allowed (%s) and why are you using the old prealloc format?", 2 * granularityKb, movieName);
				val += carry;
				carry = 0;
				if (val >= 2)
				{
					int overflow = val - 1;
					carry = (overflow + 1)/2;
					val -= carry * 2;
				}
				totalPages += val;
				rscInfo.Virtual.HasTail2 = (val == 1);
			}

			val = 0;
			formatf(attribName, "Pages%dk", 4 * granularityKb);
			if (pNodeMovie->FindValueFromPath(attribName, val) || carry > 0)
			{
				sfAssertf(val < (1 << RESOURCE_N_BITS), "Too many %d k pages requested (%d), %d max (%s) and why are you using the old prealloc format?", 4 * granularityKb, val, (1 << RESOURCE_N_BITS)-1, movieName);
				val += carry;
				carry = 0;
				if (val >= (1 << RESOURCE_N_BITS))
				{
					int overflow = val - ((1 << RESOURCE_N_BITS)-1);
					carry = (overflow + 1)/2;
					val -= carry * 2;
				}
				totalPages += val;
				rscInfo.Virtual.BaseCount = val;
			}

			val = 0;
			formatf(attribName, "Pages%dk", 8 * granularityKb);
			if (pNodeMovie->FindValueFromPath(attribName, val) || carry > 0)
			{
				sfAssertf(val < (1 << RESOURCE_2N_BITS), "Too many %d k pages requested (%d), %d max (%s) and why are you using the old prealloc format?", 8 * granularityKb, val, (1 << RESOURCE_2N_BITS)-1, movieName);
				val += carry;
				carry = 0;
				if (val >= (1 << RESOURCE_2N_BITS))
				{
					int overflow = val - ((1 << RESOURCE_2N_BITS)-1);
					carry = (overflow + 1)/2;
					val -= carry*2;
				}
				totalPages += val;
				rscInfo.Virtual.Head2Count = val;
			}

			val = 0;
			formatf(attribName, "Pages%dk", 16 * granularityKb);
			if (pNodeMovie->FindValueFromPath(attribName, val) || carry > 0)
			{
				sfAssertf(val < (1 << RESOURCE_4N_BITS), "Too many %d k pages requested (%d), %d max (%s) and why are you using the old prealloc format?", 16 * granularityKb, val, (1 << RESOURCE_4N_BITS)-1, movieName);
				val += carry;
				carry = 0;
				if (val >= (1 << RESOURCE_4N_BITS))
				{
					int overflow = val - ((1 << RESOURCE_4N_BITS)-1);
					carry = (overflow + 1)/2;
					val -= carry*2;
				}
				totalPages += val;
				rscInfo.Virtual.Head4Count = val;
			}

			val = 0;
			formatf(attribName, "Pages%dk", 32 * granularityKb);
			if (pNodeMovie->FindValueFromPath(attribName, val) || carry > 0)
			{
				sfAssertf(val < (1 << RESOURCE_8N_BITS), "Too many %d k pages requested (%d), %d max (%s) and why are you using the old prealloc format?", 32 * granularityKb, val, (1 << RESOURCE_8N_BITS)-1, movieName);
				val += carry;
				if (val >= (1 << RESOURCE_8N_BITS))
				{
					sfErrorf("Too many 512 pages needed (%d, max is %d)!  (%s) and why are you using the old prealloc format?", val, (1 << RESOURCE_8N_BITS)-1, movieName);
					val = (1 << RESOURCE_8N_BITS) - 1;
				}
				totalPages += val;
				rscInfo.Virtual.Head8Count = val;
			}

			sfAssertf(totalPages < datResourceChunk::MAX_CHUNKS, "Too many total pages needed (%d specified, %d max). Try using fewer, larger pages (%s) and why are you using the old prealloc format?", totalPages, datResourceChunk::MAX_CHUNKS, movieName);
			if (totalPages >= datResourceChunk::MAX_CHUNKS)
			{
				sfErrorf("Too many total pages needed (%d specified, %d max). Try using fewer, larger pages (%s) and why are you using the old prealloc format?", totalPages, datResourceChunk::MAX_CHUNKS, movieName);
				// Just set up some bogus 8m preallocation
				rscInfo.Virtual.HasTail4 = false;	// 16
				rscInfo.Virtual.HasTail2 = false;	// 32
				rscInfo.Virtual.BaseCount = 0;		// 64
				rscInfo.Virtual.Head2Count = (1 << RESOURCE_2N_BITS)-1;		// 128
				rscInfo.Virtual.Head4Count = 0;		// 256
				rscInfo.Virtual.Head8Count = 0;		// 512
			}
		}

		CScaleformDef* pDef = g_ScaleformStore.GetSlot(slot);

		FastAssert(addFontsIdx >= -1 && addFontsIdx < SHRT_MAX);
		pDef->m_iAdditionalFonts = (s16)addFontsIdx;

		peakSize = Max(peakSize, (int)rscInfo.GetVirtualSize());

		pDef->m_bOverallocInGameHeap = useGameHeap;

		if (isMP)
		{
			pDef->m_PreallocationInfoMP = rscInfo;
			pDef->m_GranularityKb_MP = (u16)(newGranularity / 1024);
#if __SF_STATS
			pDef->m_PeakAllowedMP = (size_t)peakSize;
#endif
		}
		else
		{
			pDef->m_PreallocationInfo = rscInfo;
			pDef->m_GranularityKb = (u16)(newGranularity / 1024);
#if __SF_STATS
			pDef->m_PeakAllowed = peakSize;
#endif
			if (pDef->m_PreallocationInfoMP.GetVirtualSize() == 0)
			{
				// hasn't been set yet
				pDef->m_PreallocationInfoMP = rscInfo;
				pDef->m_GranularityKb_MP = (u16)(newGranularity / 1024);
#if __SF_STATS
				pDef->m_PeakAllowedMP = (size_t)peakSize;
#endif
			}
		}
	}

}

void CScaleformMgr::LoadPreallocationInfo(const char* pFileName)
{
#if __DEV
	if (PARAM_noscaleformprealloc.Get())
	{
		return;
	}
#endif
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision())
		return;
#endif

	if ( parTree* pTree = PARSER.LoadTree(pFileName, "") )
	{
		parTreeNode* pRootNode = pTree->GetRoot();

		parTreeNode* pNodeMovie = NULL;

		for(int i = 0; i < g_ScaleformStore.GetCount(); i++)
		{
			CScaleformDef* pDef = g_ScaleformStore.GetSlot(strLocalIndex(i));

			if (pDef)
			{
				pDef->ResetPrealloc();
			}
		}

		while ((pNodeMovie = pRootNode->FindChildWithName("movie", pNodeMovie)) != NULL)
		{
			const char* movieName = NULL;
			pNodeMovie->FindValueFromPath("@name", movieName);
			FixupScaleformAllocation(pNodeMovie,movieName);
		}

		delete pTree;
	}
}

#if __SF_STATS
void CScaleformMgr::ScaleformAddRefCB(GFxResource* whichRsc, int /*oldCount*/, int newCount)
{
	if (ms_watchedResource == whichRsc)
	{
		sfErrorf("GFXResource AddRef - new count: %d", newCount);
	}
}

void CScaleformMgr::ScaleformDecRefCB(GFxResource* whichRsc, int /*oldCount*/, int newCount)
{
	if (ms_watchedResource == whichRsc)
	{
		sfErrorf("GFXResource Release - new count: %d", newCount);
	}
}
#endif

// multithreaded refcounting support for sfCustomGFxImageLoader::LoadImage (adds) and sfTexture destruction (removes)
sysCriticalSectionToken s_token;

struct RefCountInfo
{
	strLocalIndex m_TxdIdx;
	ScaleformMovieStruct* m_Movie;
	pgRef<grcTexture> m_Tex;
};

atVector<RefCountInfo> s_Add;
atVector<RefCountInfo> s_Remove;

int AddTxdReference(grcTexture* tex)
{
	sysCriticalSection cs(s_token);
	if (g_CurrentTxd >= 0 && sfVerifyf(tex, "Adding a reference to a NULL texture? TxdId=%d", g_CurrentTxd))
	{
		RefCountInfo info;
		sfAssertf(g_CurrentMovie, "Adding a reference to a texture while there is no current scaleform movie");
		info.m_Movie = g_CurrentMovie;
		info.m_TxdIdx = g_CurrentTxd;
		info.m_Tex = tex;
		s_Add.PushAndGrow(info);
	}
	return g_CurrentTxd;
}

void RemoveTxdReference(grcTexture* tex, int iTxd)
{
	sysCriticalSection cs(s_token);
	if (iTxd >= 0 && sfVerifyf(tex, "Removing a reference from a NULL texture? TxdId=%d", iTxd))
	{
		RefCountInfo info;
		info.m_Movie = g_CurrentMovie;
		info.m_TxdIdx = iTxd;
		info.m_Tex = tex;
		s_Remove.PushAndGrow(info);
	}
}

extern const char *CreateFlagString(u32 flags, char *outBuffer, size_t bufferSize);

void PrintTxdRef(const char* OUTPUT_ONLY(string), strLocalIndex OUTPUT_ONLY(txdId), s32* OUTPUT_ONLY(spamCtr) = NULL)
{
#if !__NO_OUTPUT
	if (spamCtr)
	{
		if (*spamCtr <= 0)
		{
			return;
		}
		(*spamCtr)--;
		if (*spamCtr == 0)
		{
			sfDebugf3("TXDREF %d: TOO MUCH SPAM", txdId.Get());
			(*spamCtr)--;
		}
	}

	char buf[32];
	buf[0] = '\0';
	char flags[256];
	flags[0] = '\0';
	const char* txdName = "";
	if (g_TxdStore.IsValidSlot(txdId))
	{
		txdName = g_TxdStore.GetName(txdId);
		g_TxdStore.GetRefCountString(txdId, buf, NELEM(buf));
		CreateFlagString(g_TxdStore.GetStreamingInfo(txdId)->GetFlags(), flags, sizeof(flags));
	}
	else
	{
		formatf(buf, "N/A");
	}
	sfDebugf3("TXDREF %s: %s (%d): %s %s", string, txdName, txdId.Get(), buf, flags);
#endif
}

void CScaleformMgr::UpdateTxdReferences()
{
	sysCriticalSection cs(s_token);

	// Check on all the pending txd requests. As soon as it loads we can clear the dontdelete flag, since
	// checking HasLoaded will add a ref for us.
	for (s32 i = 0; i < sm_ScaleformArray.GetCount(); i++)
	{
		for(int txdReqIdx = 0; txdReqIdx < sm_ScaleformArray[i].requestedTxds.GetCount(); txdReqIdx++)
		{
			ScaleformMovieTxd& txdReq = sm_ScaleformArray[i].requestedTxds[txdReqIdx];
			if (txdReq.AlreadyLoaded())
			{
				if (g_TxdStore.IsObjectInImage(txdReq.iTxdId))
				{
					g_TxdStore.ClearRequiredFlag(txdReq.iTxdId.Get(), STRFLAG_DONTDELETE);
					txdReq.streamingRequest.ClearMyRequestFlags(STRFLAG_DONTDELETE);
					OUTPUT_ONLY(PrintTxdRef("Loaded, cleared flag", txdReq.iTxdId, &txdReq.iSpamCounter));
				}
				else
				{
					OUTPUT_ONLY(PrintTxdRef("It's alive, not in an image", txdReq.iTxdId, &txdReq.iSpamCounter));
				}
			}
			else
			{
				OUTPUT_ONLY(PrintTxdRef("Still not loaded", txdReq.iTxdId, &txdReq.iSpamCounter));
			}
		}
	}

	for (int i = 0; i < s_Add.GetCount();++i)
	{
		ScaleformMovieStruct* movie = s_Add[i].m_Movie;
		g_TxdStore.AddRef(s_Add[i].m_TxdIdx, REF_OTHER);
		s_Add[i].m_Tex->AddRef();
		s_Add[i].m_Tex = pgRef<grcTexture>(NULL);
		PrintTxdRef("Added sfTex use", s_Add[i].m_TxdIdx);

		if(sfVerifyf(movie, "No movie to remove requested Txds from"))
		{
			// If there was a pending request for a texture, remove it
			for(int txdReqIdx = 0; txdReqIdx < movie->requestedTxds.GetCount(); txdReqIdx++)
			{
				ScaleformMovieTxd& txdRequest = movie->requestedTxds[txdReqIdx];
				strLocalIndex txdId = txdRequest.iTxdId;
				if (txdId == s_Add[i].m_TxdIdx)
				{
					sfDebugf1("Removing txd request for %d, because we're referencing it for real now", txdId.Get());
					if (!g_TxdStore.IsObjectInImage(txdId))
					{
						// Manually remove the request. For streamed txds the strRequest will do this for us
						g_TxdStore.RemoveRef(txdId, REF_OTHER);
					}
					movie->requestedTxds.DeleteFast(txdReqIdx);
					--txdReqIdx;
					PrintTxdRef("Removed from req queue", txdId);
				}
			}
		}
	}
	s_Add.Resize(0);

	for (int i = 0; i < s_Remove.GetCount();++i)
	{
		g_TxdStore.AddRef(s_Remove[i].m_TxdIdx, REF_RENDER); // Make sure the reference sticks around for a few more frames
		gDrawListMgr->AddRefCountedModuleIndex(s_Remove[i].m_TxdIdx.Get(), &g_TxdStore);
		s_Remove[i].m_Tex->Release();
		s_Remove[i].m_Tex = pgRef<grcTexture>(NULL);
		g_TxdStore.RemoveRef(s_Remove[i].m_TxdIdx, REF_OTHER);
		PrintTxdRef("Removing sfTex use", s_Remove[i].m_TxdIdx);
	}
	s_Remove.Resize(0);
}


#if !__NO_OUTPUT
static bool g_sfPrintFullMemoryReport = false;
void ScaleformAllocFailedCb()
{
	static bool hasPrinted = false;
	if (!hasPrinted)
	{
		g_sfPrintFullMemoryReport = true;
		hasPrinted = true;
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::Init()
// PURPOSE: initialises the Scaleform manager
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
#if __BANK
		if (PARAM_actionscriptdebug.Get())
		{
			ms_bDebugOutputOn = true;
		}
#endif // __BANK

		CScaleformComplexObjectMgr::Init();  // initialise complex object

		// init movie storage
		if(sm_ScaleformArray.GetCapacity() == 0)
		{
			int scaleformArraySize = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("ScaleformMgrArray", 0x0966ba3bc), CONFIGURED_FROM_FILE);
			Assertf(scaleformArraySize <= MAX_SCALEFORM_MOVIES, "Gameconfig-derived scaleform move array larger than MAX_SCALEFORM_MOVIES, gs_ScaleformMovieCS will be too small");
			sm_ScaleformArray.ResizeGrow(scaleformArraySize);
		}

		// inital method call flags:
		for (s32 i = 0; i < MAX_METHOD_BUILD_BUFFERS; i++)
		{
			sm_ScaleformMethodBuild[i].cMethodName[0] = '\0';
			sm_ScaleformMethodBuild[i].iMovieId = -1;
			sm_ScaleformMethodBuild[i].iMovieIdThisInvokeIsLinkedTo = -1;
			sm_ScaleformMethodBuild[i].iParamCount = 0;
			sm_ScaleformMethodBuild[i].iReturnId = -1;
			sm_ScaleformMethodBuild[i].iBaseClass = SF_BASE_CLASS_GENERIC;
			sm_ScaleformMethodBuild[i].ComplexObjectIdToInvoke = INVALID_COMPLEX_OBJECT_ID;

			for (s32 j = 0; j < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD; j++)
			{
				sm_ScaleformMethodBuild[i].params[j].pParamString = NULL;
				sm_ScaleformMethodBuild[i].params[j].bParamBool = false;
				sm_ScaleformMethodBuild[i].params[j].ParamNumber = 0;
				sm_ScaleformMethodBuild[i].params[j].pParamGfxValue = NULL;
				sm_ScaleformMethodBuild[i].params[j].param_type = PARAM_TYPE_INVALID;
			}
		}

		for (s32 k = 0; k < MAX_STORED_RETURN_VALUES; k++)
		{
			sm_ReturnValuesFromInvoke[k].iUniqueId = 0;
			sm_ReturnValuesFromInvoke[k].gfxvalue_return_value.SetUndefined();
			sm_ReturnValuesFromInvoke[k].iFramesToStayValid = 0;

			sm_ReturnValuesForAccess[k].iUniqueId = 0;
			sm_ReturnValuesForAccess[k].gfxvalue_return_value.SetUndefined();
			sm_ReturnValuesForAccess[k].iFramesToStayValid = 0;
		}

		
		sm_MethodInvokeInfo[0].Reset();
		sm_MethodInvokeInfo[1].Reset();
		sm_iMethodInvokeInfoBufferInUse = 0;

		sm_MethodCallbackInfo[0].Reset();
		sm_MethodCallbackInfo[1].Reset();

		sfRendererBase::sm_AlwaysSmooth = true; // Force smooth interpolation for all textures

		sfScaleformManager* movieMgr = rage_new sfScaleformManager;
		movieMgr->Init(&strStreamingEngine::GetAllocator()/*, 64*/); // Allocate with 64k pages from the streaming heap

		movieMgr->GetLoader()->SetURLBuilder((GPtr<GFxURLBuilder>)*rage_new sfCustomGFxURLBuilder);
		movieMgr->GetLoader()->SetImageLoader((GPtr<GFxImageLoader>)*rage_new sfCustomGFxImageLoader);

		sfTextureBase::addRefCallback.Bind(&AddTxdReference);
		sfTextureBase::removeRefCallback.Bind(&RemoveTxdReference);

		movieMgr->InitExternalInterface(*rage_new sfCallGameFromFlash());
		movieMgr->GetRageRenderer().SetUseDeletionQueue(false);

		for (s32 iCount = 0; iCount < sm_ScaleformArray.GetCount(); iCount++)
		{
			ClearScaleformArraySlot(iCount);
		}

#if !__NO_OUTPUT
		sfPreallocatedMemoryWrapper::sm_AllocationFailedDelegate = atDelegate<void(void)>(ScaleformAllocFailedCb);
#endif

		// store the initial resolution
		ms_vScreenSize.x = (float)VideoResManager::GetUIWidth();
		ms_vScreenSize.y = (float)VideoResManager::GetUIHeight();

		InitRenderStateBlocks();

		// initialise the list of sorted 3d movies
		ms_Sorted3DMovies.Init(MAX_SCALEFORM_MOVIES);

#if __SF_STATS
		sm_sfAddRefCB = ScaleformAddRefCB;
		sm_sfDecRefCB = ScaleformDecRefCB;
#endif

#if __BANK
		CDataFileMount::RegisterMountInterface(CDataFileMgr::SCALEFORM_VALID_METHODS_FILE, &g_ScaleformPreallocationDataFileMounter);
#endif // __BANK
		CDataFileMount::RegisterMountInterface(CDataFileMgr::SCALEFORM_PREALLOC_FILE, &g_ScaleformPreallocationDataFileMounter);
		CDataFileMount::RegisterMountInterface(CDataFileMgr::SCALEFORM_DLC_FILE, &g_ScaleformPreallocationDataFileMounter, eDFMI_UnloadLast);

		// Initialization is done, let the world know about the movie manager
		sys_lwsync();
		m_staticMovieMgr = movieMgr;
	}
	else if(initMode == INIT_SESSION)
	{
#if __BANK
		if (PARAM_showsfperformance.Get())
		{
			ms_bShowPerformanceInfo = true;
		}
#endif

		HandleNewTextureDependencies();
	}
}

void CScaleformMgr::HandleNewTextureDependencies()
{
	static s32 nextItemToProcess = 0;


	s32 iStoreSize = g_ScaleformStore.GetNumUsedSlots();

	// init the store's texture id's
	for (s32 iCount = nextItemToProcess; iCount < iStoreSize; iCount++)
	{
		if (g_ScaleformStore.IsValidSlot(strLocalIndex(iCount)))
		{
			InitTextures(iCount, g_ScaleformStore.GetName(strLocalIndex(iCount)));
		}
	}

	nextItemToProcess = iStoreSize;
}

#if __DEV
void CScaleformMgr::PreallocTest(unsigned initMode)
{
	if (initMode == INIT_SESSION)
	{
		if (PARAM_loadallsfmovies.Get())
		{
			for(int i = 0; i < g_ScaleformStore.GetCount(); i++)
			{
				CScaleformDef* pDef = g_ScaleformStore.GetSlot(strLocalIndex(i));

				if (!pDef)
				{
					continue;
				}

				if (pDef->m_pObject)
				{
					sfDisplayf("Skipping %s because it's already loaded", pDef->m_cFullFilename);
				}
				else if (pDef->m_name.IsNotNull())
				{
					const char* movieName = pDef->m_name.GetCStr();

					Vector2 size;
					size.x = size.y = 1.0f;

					int index = CreateMovieAndWaitForLoad(movieName, ORIGIN, ORIGIN, size);

					// go through each base class and try to invoke a "TEST_CASE" method on the movie
					if (GetMovieView(index))
					{
						sfDisplayf("Movie %s is preallocated as a standalone/parent movie", movieName);

						for (s32 iBaseClass = 0; iBaseClass < SF_MAX_BASE_CLASSES; iBaseClass++)
						{
							CallMethod(index, (eSCALEFORM_BASE_CLASS)iBaseClass, "TEST_CASE");
						}
					}
					else
					{
						sfDisplayf("Movie %s is preallocated without a movieview", movieName);
					}

					if (IsMovieActive(index) && (GetMovieView(index)))
					{
						UpdateMovie(index);
					}

					DestroyMovieNow(index);
					g_ScaleformStore.StreamingRemove(strLocalIndex(i));
				}
			}
		}
	}
}
#endif // __DEV


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::InitRenderStateBlocks()
// PURPOSE: init state blocks used in RenderWorldSpace
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::InitRenderStateBlocks()
{
	// blend state block for 3d diffuse pass
	grcBlendStateDesc blendStateBlockDesc;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].BlendEnable = 1;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	blendStateBlockDesc.IndependentBlendEnable = 1;
	ms_defDiffuseBlendState = grcStateBlock::CreateBlendState(blendStateBlockDesc);


	// blend state block for 3d emissive pass
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_1].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_2].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
	blendStateBlockDesc.BlendRTDesc[GBUFFER_RT_3].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;

	// on 360 all 4 targets are bound
	const GBufferRT SC_GBUFFER_RT_IDX = (__XENON ? GBUFFER_RT_3 : GBUFFER_RT_0);

	// only write to blue channel (emissive scale)
	blendStateBlockDesc.BlendRTDesc[SC_GBUFFER_RT_IDX].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_BLUE;

	// set alpha blending
	ms_defEmissiveBlendState = grcStateBlock::BS_Add;

	// common depth state block for 3d diffuse pass
	grcDepthStencilStateDesc depthStencilBlockDesc;
	depthStencilBlockDesc.StencilEnable = false;
	depthStencilBlockDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	ms_defEmissiveDepthStencilState = grcStateBlock::CreateDepthStencilState(depthStencilBlockDesc);
	ms_defDiffuseDepthStencilState = grcStateBlock::CreateDepthStencilState(depthStencilBlockDesc);

	// common rasterizer state (tbr)
	ms_defEmissiveRasterizerState = grcStateBlock::RS_NoBackfaceCull;
	ms_defDiffuseRasterizerState = grcStateBlock::RS_NoBackfaceCull;

	// exit state
	grcRasterizerStateDesc defaultExitStateR;
	defaultExitStateR.CullMode = grcRSV::CULL_NONE;
	ms_defExitRasterizerState = grcStateBlock::CreateRasterizerState(defaultExitStateR);

	grcBlendStateDesc defaultExitStateB;
	defaultExitStateB.BlendRTDesc[0].BlendEnable = 1;
	defaultExitStateB.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
	defaultExitStateB.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
	ms_defExitBlendState = grcStateBlock::CreateBlendState(defaultExitStateB);

	grcDepthStencilStateDesc defaultExitStateDS;
	defaultExitStateDS.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	ms_defExitDepthStencilState = grcStateBlock::CreateDepthStencilState(defaultExitStateDS);

}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::Shutdown()
// PURPOSE: shuts down the Scaleform manager
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
#if __BANK
		ShutdownWidgets();
#endif
		KillAllMovies();

		// shutdown the drawtext manager if its open:
		RemoveFontMovie();

		// shutdown the main movie manager
	    if (m_staticMovieMgr)
	    {
		    m_staticMovieMgr->Shutdown();
		    delete m_staticMovieMgr;
		    m_staticMovieMgr = NULL;
	    }

		// free move array
		if(sm_ScaleformArray.GetCapacity() != 0)
		{
			sm_ScaleformArray.Reset();
		}

		ms_Sorted3DMovies.Shutdown();

    }
	else if(shutdownMode == SHUTDOWN_SESSION)
	{
		CScaleformMovieWrapper::Shutdown();

		// all movies must be completely removed at shutdown session as some container movies will hold
		// internal ActionScript references that need to be refreshed when the movie is created, which needs
		// to happen on the INIT(SESSION).  If this doesnt happen it will reuse the existing movie from the
		// old session and AS will be confused with whats going on in the game.
		for (s32 iNum = 0; iNum < sm_ScaleformArray.GetCount(); iNum++)
		{
			if (IsMovieActive(iNum) && sm_ScaleformArray[iNum].bRemovableAtInit)
			{
				uiWarningf("CScaleformMgr::Shutdown - Requesting movie removal at shutdown on '%s'.  This may put the Scaleform manager in a volatile state as the system that has requested this movie may be unknowing of it's removal", sm_ScaleformArray[iNum].cFilename);
				RequestRemoveMovie(iNum);
			}
		}
	}
}

void CScaleformMgr::KillAllMovies()
{
#if __BANK
	ShutdownWidgets();
#endif
	CScaleformMovieWrapper::Shutdown();
	// remove any movies that are currently active:
	for (s32 iNum = 0; iNum < sm_ScaleformArray.GetCount(); iNum++)
	{
		if (sm_ScaleformArray[iNum].iMovieId != -1)
		{
			DestroyMovieNow(iNum);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::FindOriginalFilename()
// PURPOSE: retrieves the original filename that was used for the stream of this file
//			Returns NULL if its not found
/////////////////////////////////////////////////////////////////////////////////////
char *CScaleformMgr::FindOriginalFilename(const char *pFilename)
{
	s32 iMaxChar = istrlen(pFilename);

	// remove any extension from the name if found:
	for (s32 i = 0; i < iMaxChar; i++)
	{
		if (pFilename[i] == '.')
		{
			iMaxChar = i;
			break;
		}
	}

	if (iMaxChar > 0)
	{
		char cFilename[RAGE_MAX_PATH];
		strcpy(cFilename, pFilename);
		cFilename[iMaxChar] = '\0';

		for (s32 i = 0; i < sm_ScaleformArray.GetCount(); i++)
		{
			if (!strcmpi(sm_ScaleformArray[i].cFilename, cFilename))
			{
				if (sm_ScaleformArray[i].iMovieId != -1)
				{
					if (g_ScaleformStore.GetFilenameUsedForLoad(strLocalIndex(sm_ScaleformArray[i].iMovieId))[0] != '\0')
					{
						return (g_ScaleformStore.GetFilenameUsedForLoad(strLocalIndex(sm_ScaleformArray[i].iMovieId)));
					}
					else
					{
						return NULL;
					}
				}
				else
				{
					sfAssertf(0, "ScaleformMgr: Movie %s isnt active yet, but another movie is trying to use it!", cFilename);
					return NULL;
				}
			}
		}
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsMovieActive()
// PURPOSE: Returns whether this movie is loaded and active ready to be used.
//			note that this will now fail if the movie has been set to be removed
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsMovieActive(s32 iIndex)
{
	if (!IsMovieIdInRange(iIndex))
		return false;

	return (sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_ACTIVE);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::HasMovieGotAnyState()
// PURPOSE: Returns whether this movie has any other state than INACTIVE
//			ie may be loading or active
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::HasMovieGotAnyState(s32 iIndex)
{
	if (!IsMovieIdInRange(iIndex))
		return false;

	return (sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_INACTIVE);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetNoofMoviesActiveOrLoading
// PURPOSE: Returns the number of movies active or loading
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::GetNoofMoviesActiveOrLoading()
{
	s32 count = 0;
	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_ACTIVE ||
			sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_FLAGGED_FOR_USE ||
			sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_STREAMING_MOVIE ||
			sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_RESTARTING)
		{
			++count;
		}
	}
	return count;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsMovieShuttingDown()
// PURPOSE: Is the movie in a state of shutting down
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsMovieShuttingDown(s32 iIndex)
{
	if (!IsMovieIdInRange(iIndex))
		return false;

	if (sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_INACTIVE)
	{
		if (sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1 ||
			sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2 ||
			sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_SET_TO_REMOVE_NO_REF_CHECK)
		{
			return true;
		}
	}

	return false;
}




////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsMovieUpdateable()
// PURPOSE: Returns whether this movie is in a state were it can be updated or
//			callbacks processed on.  It may be "active", but also "pending final update"
////////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsMovieUpdateable(s32 iIndex)
{
	if (!IsMovieIdInRange(iIndex))
		return false;

	return (sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_ACTIVE || sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CanMovieBeRemovedByScript()
// PURPOSE: Returns whether this movie is in a state where it can be removed by script.
//				If the movie is already in the process of being removed then this function will return false
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::CanMovieBeRemovedByScript(s32 iIndex)
{
	if (!IsMovieIdInRange(iIndex))
		return false;

	switch (sm_ScaleformArray[iIndex].iState)
	{
		case SF_MOVIE_STATE_INACTIVE :
		case SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE :
		case SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1 :
		case SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2 :
		case SF_MOVIE_STATE_SET_TO_REMOVE_NO_REF_CHECK :
			return false;
//			break;
		case SF_MOVIE_STATE_FLAGGED_FOR_USE :
		case SF_MOVIE_STATE_STREAMING_MOVIE :
 		case SF_MOVIE_STATE_ACTIVE :
 		case SF_MOVIE_STATE_RESTARTING :
			return true;
//			break;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsMovieRendering()
// PURPOSE: Returns whether this movie is currently rendering
////////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsMovieRendering(s32 iIndex)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CScaleformMgr::IsMovieRendering can only be called on the RenderThread!");

		return false;
	}

	if (!IsMovieIdInRange(iIndex))
		return false;

	if (IsMovieActive(iIndex))
	{
		return sm_ScaleformArray[iIndex].iRendered > 0;
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsChildMovie()
// PURPOSE: Returns whether this movie is a child movie or not
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsChildMovie(s32 iIndex)
{
	if (!IsMovieIdInRange(iIndex))
		return false;

	return (sm_ScaleformArray[iIndex].iParentMovie != -1);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::DeviceLost() and CScaleformMgr::DeviceReset()
// PURPOSE: Callback functions for device lost and reset for PC
/////////////////////////////////////////////////////////////////////////////////////

#if RSG_PC
void CScaleformMgr::DeviceLost()
{

}

void CScaleformMgr::DeviceReset()
{
	// store the initial resolution
	ms_vScreenSize.x = (float)VideoResManager::GetUIWidth();
	ms_vScreenSize.y = (float)VideoResManager::GetUIHeight();

	for (s32 i = 0; i < sm_ScaleformArray.GetCount(); i++)
	{
		if (HasMovieGotAnyState(i))  // only process movies that have some sort of state
		{
			sm_ScaleformArray[i].bChangeMovieParams.Set(true);
		}
	}
}

bool CScaleformMgr::GetMovieShouldIgnoreSuperWidescreen(s32 iIndex)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		return sm_ScaleformArray[iIndex].bIgnoreSuperWidescreenScaling.GetUpdateBuf();
	}
	else if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		return sm_ScaleformArray[iIndex].bIgnoreSuperWidescreenScaling.GetRenderBuf();
	}

	return false;
}
#endif



//////////////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetRequiredScaleMode()
// PURPOSE: checks resolution, movie size, aspect ratio and returns the scale mode
//			we wish to use
// bUseShowAll - return SM_ShowAll if the movie is a fullscreen movie and we're in super widescreen
//////////////////////////////////////////////////////////////////////////////////////////////////
GFxMovieView::ScaleModeType CScaleformMgr::GetRequiredScaleMode(Vector2 const& vPos, Vector2 const& vSize, bool bUseShowAll/* = false*/)
{
	// Full screen
	if (vPos.x == 0.0f && vPos.y == 0.0f && vSize.x == 1.0f && vSize.y == 1.0f)
	{
		if (!CHudTools::IsSuperWideScreen())
		{
			// Less than 16:10
			return GFxMovieView::SM_NoBorder;
		}
		else if (bUseShowAll)
		{
			return GFxMovieView::SM_ShowAll;
		}
	}

	return GFxMovieView::SM_ExactFit;  // not full screen, so use exact coords
}


GFxMovieView::ScaleModeType CScaleformMgr::GetRequiredScaleMode(s32 iIndex, bool bUseShowAll/* = false*/)
{
	if (uiVerify(iIndex != INVALID_MOVIE_ID))
	{
		if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
		{
			Vector2 vPos(sm_ScaleformArray[iIndex].vOriginalPos.GetUpdateBuf().x, sm_ScaleformArray[iIndex].vOriginalPos.GetUpdateBuf().y);
			return CScaleformMgr::GetRequiredScaleMode(vPos, sm_ScaleformArray[iIndex].vOriginalSize.GetUpdateBuf(), bUseShowAll);
		} 
		else
		{
			Vector2 vPos(sm_ScaleformArray[iIndex].vOriginalPos.GetRenderBuf().x, sm_ScaleformArray[iIndex].vOriginalPos.GetRenderBuf().y);
			return CScaleformMgr::GetRequiredScaleMode(vPos, sm_ScaleformArray[iIndex].vOriginalSize.GetRenderBuf(), bUseShowAll);
		}
	}

	return GFxMovieView::SM_ExactFit;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::PrepareUpdateAllMoviesOnRT()
// PURPOSE: any movie that is currently active is updated on the renderthread
//			automatically, independent to whether its rendered or not
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::PrepareUpdateAllMoviesOnRT()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr::UpdateAllMoviesOnRT can only be called on the RenderThread!");
		return;
	}

	for (s32 i = 0; i < sm_ScaleformArray.GetCount(); i++)
	{
		sm_ScaleformArray[i].bNeedsToUpdateThisFrame = false;
		if (IsMovieUpdateable(i))  // only process movies that have some sort of state
		{
#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[i]);
#endif // __SCALEFORM_CRITICAL_SECTIONS

			if (sm_ScaleformArray[i].bUpdateMovie)
			{
				if (sm_ScaleformArray[i].fCurrentTimestep >= 0.0f)  // neg results in no render
				{
					sm_ScaleformArray[i].bNeedsToUpdateThisFrame = true;
				}
			}
		}
	}
}

rage::eBlockedMouseInputs GetBlockedMouseInputs()
{
#if RSG_PC
	if(STextInputBox::IsInstantiated() && STextInputBox::GetInstance().IsActive())
		return BLOCKED_MOUSE_NONE;
#endif // RSG_PC

	CControl& control = CControlMgr::GetMainFrontendControl();

	rage::eBlockedMouseInputs eBMI = BLOCKED_MOUSE_NONE;
	if( !control.GetCursorAccept().IsEnabled() ) 
		eBMI |= BLOCKED_MOUSE_LEFT;

	if( !control.GetCursorCancel().IsEnabled() ) 
		eBMI |= BLOCKED_MOUSE_RIGHT;
	

	if( !control.GetCursorX().IsEnabled() || !control.GetCursorY().IsEnabled() )
		eBMI |= BLOCKED_MOUSE_MOVE;

	if( !control.GetValue(INPUT_CURSOR_SCROLL_DOWN).IsEnabled() || !control.GetValue(INPUT_CURSOR_SCROLL_UP).IsEnabled() )
		eBMI |= BLOCKED_MOUSE_SCROLL;

	return eBMI;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::EnsureAllMoviesUpdatedOnRT()
// PURPOSE: Makes sure all the movies that needed to update this frame have been updated.
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::EnsureAllMoviesUpdatedOnRT()
{
#if RSG_PC
	// Update Input Events (Currently only Mouse Input on PC)
	sfInput& movieMgrInput = GetMovieMgr()->GetInput();
	movieMgrInput.Update(fwTimer::GetSystemTimeStep(), GetBlockedMouseInputs());
#endif
	
	for(s32 i = 0; i < sm_ScaleformArray.GetCount(); i++)
	{
		if (sm_ScaleformArray[i].bNeedsToUpdateThisFrame && IsMovieUpdateable(i)) // make sure the movie is still in an updateable state
		{
#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[i]);
#endif // __SCALEFORM_CRITICAL_SECTIONS
			UpdateMovie(i, sm_ScaleformArray[i].fCurrentTimestep);
		}
	}

#if KEYBOARD_MOUSE_SUPPORT
	ms_bfullScreenBlockedInputLastFrame = IsFullScreenScaleformMovieBlockingInput();
#endif
}

bool CScaleformMgr::IsFullScreenScaleformMovieBlockingInput()
{
	return CWarningScreen::IsActive() && !SReportMenu::GetInstance().IsActive();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CleanupMoviesOnRT()
// PURPOSE: deals with any cleanup of movies that needs to happen on the RT
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::CleanupMoviesOnRT()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr::CleanupMoviesOnRT can only be called on the RenderThread!");
		return;
	}

	//
	// check for movie getting deleted and if so, reinstate it
	//
	for (s32 i = 0; i < sm_ScaleformArray.GetCount(); i++)
	{
		if (HasMovieGotAnyState(i))  // only process movies that have some sort of state
		{
			if (IsMovieActive(i))
			{

#if __SCALEFORM_CRITICAL_SECTIONS
				SYS_CS_SYNC(gs_ScaleformMovieCS[i]);
#endif // __SCALEFORM_CRITICAL_SECTIONS

				if (sm_ScaleformArray[i].iRendered > 0)
				{
					sm_ScaleformArray[i].iRendered--;
				}

				if (sm_ScaleformArray[i].bPerformGarbageCollection.GetRenderBuf())
				{
					sfDebugf3("CScaleformMgr: CollectGarbage about to be called on movie %s (%d)", sm_ScaleformArray[i].cFilename, i);

					sfScaleformMovieView *pRawMovieView = g_ScaleformStore.GetRawMovieView(strLocalIndex(sm_ScaleformArray[i].iMovieId));
					if (pRawMovieView)
					{
						pRawMovieView->GetMovieView().ForceCollectGarbage();

						sfDebugf3("CScaleformMgr: CollectGarbage called on movie %s (%d)", sm_ScaleformArray[i].cFilename, i);
					}

					sm_ScaleformArray[i].bPerformGarbageCollection.GetRenderBuf() = false;
				}

				if (sm_ScaleformArray[i].bChangeMovieParams.GetRenderBuf())
				{
					if(sfVerifyf(g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[i].iMovieId)), "ScaleformMgr: store %d invalid! (%d not loaded!)", sm_ScaleformArray[i].iMovieId, i))
					{
						GFxMovieView *pMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(sm_ScaleformArray[i].iMovieId));

						if (pMovieView)
						{
							float sx = ms_vScreenSize.x;
							float sy = ms_vScreenSize.y;
							float ox = 0.f;
							float oy = 0.f;
#if SUPPORT_MULTI_MONITOR
							if (sx >= VideoResManager::GetUIWidth() && sy >= VideoResManager::GetUIHeight())
							{
								const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
								sx = (float)mon.getWidth();
								sy = (float)mon.getHeight();
								ox = (float)mon.uLeft;
								oy = (float)mon.uTop;
							}
#endif //SUPPORT_MULTI_MONITOR
							GViewport viewportDesc(VideoResManager::GetUIWidth(), VideoResManager::GetUIHeight(),
								(s32)(sm_ScaleformArray[i].vPos.GetRenderBuf().x*sx + ox), (s32)(sm_ScaleformArray[i].vPos.GetRenderBuf().y*sy + oy),
								(s32)(sm_ScaleformArray[i].vSize.GetRenderBuf().x*sx), (s32)(sm_ScaleformArray[i].vSize.GetRenderBuf().y*sy));

#if __CONSOLE
							viewportDesc.AspectRatio = CHudTools::GetAspectRatioMultiplier();
#endif //__CONSOLE
							pMovieView->SetViewport(viewportDesc);

							if (pMovieView->GetViewScaleMode() != sm_ScaleformArray[i].eScaleMode.GetRenderBuf())
							{
								pMovieView->SetViewScaleMode(sm_ScaleformArray[i].eScaleMode.GetRenderBuf());
							}

							sfDebugf3("CScaleformMgr::CleanupMoviesOnRT -- Changing Movie Params for %s: UIWidth = %d, UIHeight = %d, MovieX = %.2f, MovieY = %.2f, MovieScaleX = %.2f, MovieScaleY = %.2f, ScreenSizeX = %.2f, ScreenSizeY = %.2f", GetMovieFilename(i), VideoResManager::GetUIWidth(), VideoResManager::GetUIHeight(), sm_ScaleformArray[i].vPos.GetRenderBuf().x, sm_ScaleformArray[i].vPos.GetRenderBuf().y, sm_ScaleformArray[i].vSize.GetRenderBuf().x, sm_ScaleformArray[i].vSize.GetRenderBuf().y, CScaleformMgr::ms_vScreenSize.x, CScaleformMgr::ms_vScreenSize.y);
						}
					}

					sm_ScaleformArray[i].bChangeMovieParams.GetRenderBuf() = false;
				}

			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CreateMovie()
// PURPOSE: Finds an empty slot to create the move in and starts setting it up
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::CreateMovie(const char *cFilename, Vector2 vPos, Vector2 vSize, bool bRemovable, s32 iParentMovie, s32 iDependentMovie, bool bRequiresMovieView, eMovieOwnerTags calledBy, bool bDontRenderWhilePaused, bool bIgnoreSuperWidescreenAdjustment)
{
	CreateMovieParams params(cFilename);
	params.vPos = Vector3(vPos.x, vPos.y, 0.0f);
	params.vRot = Vector3(0.0f, 0.0f, 0.0f);
	params.vSize = vSize;
	params.bStream = true;
	params.bForceLoad = false;
	params.bRemovable = bRemovable;
	params.iParentMovie = iParentMovie;
	params.iDependentMovie = iDependentMovie;
	params.bRequiresMovieView = bRequiresMovieView;
	params.movieOwnerTag = calledBy;
	params.bDontRenderWhilePaused = bDontRenderWhilePaused;
	params.bIgnoreSuperWidescreenScaling = bIgnoreSuperWidescreenAdjustment;

	params.eScaleMode = GetRequiredScaleMode(vPos, vSize);

	return CreateMovieInternal(params);
}



s32 CScaleformMgr::CreateMovie(const char *cFilename, const Vector3& vPos, const Vector3& vRot, Vector2 vSize, bool bRemovable, s32 iParentMovie, s32 iDependentMovie, bool bRequiresMovieView, eMovieOwnerTags calledBy, bool bDontRenderWhilePaused)
{
	CreateMovieParams params(cFilename);
	params.vPos = vPos;
	params.vRot = vRot;
	params.vSize = vSize;
	params.bStream = true;
	params.bForceLoad = false;
	params.bRemovable = bRemovable;
	params.iParentMovie = iParentMovie;
	params.iDependentMovie = iDependentMovie;
	params.bRequiresMovieView = bRequiresMovieView;
	params.movieOwnerTag = calledBy;
	params.eScaleMode = GetRequiredScaleMode(Vector2(vPos.x, vPos.y), vSize);
	params.bDontRenderWhilePaused = bDontRenderWhilePaused;

	return CreateMovieInternal(params);
}
s32 CScaleformMgr::CreateMovie(CreateMovieParams& params)
{
	params.bStream = true;
	params.bForceLoad = false;

	return CreateMovieInternal(params);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CreateMovieAndWaitForLoad()
// PURPOSE: Finds an empty slot to create the move in and starts setting it up - waits for load
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::CreateMovieAndWaitForLoad(const char *cFilename, Vector2 vPos, Vector2 vSize, bool bRemovable, s32 iParentMovie, s32 iDependentMovie, bool bRequiresMovieView, eMovieOwnerTags calledBy, bool bDontRenderWhilePaused, bool bIgnoreSuperWidescreenAdjustment)
{
	CreateMovieParams params(cFilename);
	params.vPos.Set(vPos.x, vPos.y, 0.0f);
	params.vRot.Set(0.0f, 0.0f, 0.0f);
	params.vSize = vSize;
	params.bStream = true;
	params.bForceLoad = true;
	params.bRemovable = bRemovable;
	params.iParentMovie = iParentMovie;
	params.iDependentMovie = iDependentMovie;
	params.bRequiresMovieView = bRequiresMovieView;
	params.movieOwnerTag = calledBy;
	params.eScaleMode = GetRequiredScaleMode(vPos, vSize);
	params.bDontRenderWhilePaused = bDontRenderWhilePaused;
	params.bIgnoreSuperWidescreenScaling = bIgnoreSuperWidescreenAdjustment;

	return CreateMovieInternal(params);
}
s32 CScaleformMgr::CreateMovieAndWaitForLoad(const char *cFilename, const Vector3& vPos, const Vector3& vRot, Vector2 vSize, bool bRemovable, s32 iParentMovie, s32 iDependentMovie, bool bRequiresMovieView, eMovieOwnerTags calledBy, bool bDontRenderWhilePaused)
{
	CreateMovieParams params(cFilename);
	params.vPos = vPos;
	params.vRot = vRot;
	params.vSize = vSize;
	params.bStream = true;
	params.bForceLoad = true;
	params.bRemovable = bRemovable;
	params.iParentMovie = iParentMovie;
	params.iDependentMovie = iDependentMovie;
	params.bRequiresMovieView = bRequiresMovieView;
	params.movieOwnerTag = calledBy;
	params.eScaleMode = GetRequiredScaleMode(Vector2(vPos.x, vPos.y), vSize);
	params.bDontRenderWhilePaused = bDontRenderWhilePaused;

	return CreateMovieInternal(params);
}
s32 CScaleformMgr::CreateMovieAndWaitForLoad(CreateMovieParams& params)
{
	params.bStream = true;
	params.bForceLoad = true;

	return CreateMovieInternal(params);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CreateMovieInternal()
// PURPOSE: Finds an empty slot to create the move in and starts setting it up
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::CreateMovieInternal(const CreateMovieParams& params)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr:: CreateMovie can only be called on the UpdateThread!");
		return -1;
	}

	//
	// check for movie getting deleted and if so, reinstate it
	//
	for (s32 i = 0; i < sm_ScaleformArray.GetCount(); i++)
	{
		if (sm_ScaleformArray[i].iState == SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE || sm_ScaleformArray[i].iState == SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1 || sm_ScaleformArray[i].iState == SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2 || sm_ScaleformArray[i].iState == SF_MOVIE_STATE_SET_TO_REMOVE_NO_REF_CHECK)
		{
			// the movie getting requested is flagged to get deleted, so re-enable it and return its id:
			if (!strcmpi(sm_ScaleformArray[i].cFilename, params.cFilename))
			{
				sfDebugf1("ScaleformMgr: Movie %s is set to be deleted - reinstating %d", sm_ScaleformArray[i].cFilename, i);

				if (!params.bForceLoad)
				{
					if (g_ScaleformStore.GetNumRefs(strLocalIndex(sm_ScaleformArray[i].iMovieId)) > 0)  // if we already have references to this, then set it back to active as its still there
					{
						if ( (!params.bRequiresMovieView) )  // this must be a child or has no movieview, so dont try to restart as we cant if it has no movieview!
						{
							sm_ScaleformArray[i].iState = SF_MOVIE_STATE_ACTIVE;  // set it to be active again
						}
						else
						{
							sm_ScaleformArray[i].iState = SF_MOVIE_STATE_RESTARTING;  // set it to be active again after it has restarted correctly (it will return unloaded until this happens)
						}

					}
					else
					{
						sm_ScaleformArray[i].iState = SF_MOVIE_STATE_STREAMING_MOVIE;  // set it to be streaming again as it never added a ref so it will continue to try and stream in and add a ref if successful
					}
				}
				else
				{
					sfDebugf1("ScaleformMgr:: Movie %s reinitialised on a forced load - it will not restart", params.cFilename);
					sm_ScaleformArray[i].iState = SF_MOVIE_STATE_ACTIVE;  // set it to be active again
				}

#if __ASSERT
				if (!(sm_ScaleformArray[i].movieOwnerTag & params.movieOwnerTag))
				{
					if (params.movieOwnerTag == SF_MOVIE_TAGGED_BY_CODE)
					{
						sfAssertf(0, "ScaleformMgr: Movie '%s' is getting requested by CODE whilst it is already in use by SCRIPT", params.cFilename);
					}

					if (params.movieOwnerTag == SF_MOVIE_TAGGED_BY_SCRIPT)
					{
						sfAssertf(0, "ScaleformMgr: Movie '%s' is getting requested by SCRIPT whilst it is already in use by CODE", params.cFilename);
					}

				}
#endif // #if __ASSERT

				ChangeMovieParams(i, Vector2(params.vPos.x, params.vPos.y), params.vSize, params.eScaleMode);

				return i;
			}
		}
	}

	//
	// check for movie already requested:
	//
	for (s32 i = 0; i < sm_ScaleformArray.GetCount(); i++)
	{
		if (sm_ScaleformArray[i].iState != SF_MOVIE_STATE_INACTIVE &&
			sm_ScaleformArray[i].iState != SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE &&
			sm_ScaleformArray[i].iState != SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1 &&
			sm_ScaleformArray[i].iState != SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2 &&
			sm_ScaleformArray[i].iState != SF_MOVIE_STATE_SET_TO_REMOVE_NO_REF_CHECK)
		{
			// it isnt inactive, but its also not fully active (so its loading...)
			if (!strcmpi(sm_ScaleformArray[i].cFilename, params.cFilename))
			{
#if __DEV
				if (!PARAM_supressspamming.Get())
#endif
				{
					sfDebugf1("ScaleformMgr: Movie %s already requested, reusing %d", sm_ScaleformArray[i].cFilename, i);
				}

				if (IsMovieActive(i))  // if already active, then change the pos and size to the new values so the latest pos/size is the one that gets used
				{
					ChangeMovieParams(i, Vector2(params.vPos.x, params.vPos.y), params.vSize, params.eScaleMode);
				}

#if __ASSERT
				if (!(sm_ScaleformArray[i].movieOwnerTag & params.movieOwnerTag))
				{
					if (params.movieOwnerTag == SF_MOVIE_TAGGED_BY_CODE)
					{
						sfAssertf(0, "ScaleformMgr: Movie '%s' is getting requested by CODE whilst it is already in use by SCRIPT", params.cFilename);
					}

					if (params.movieOwnerTag == SF_MOVIE_TAGGED_BY_SCRIPT)
					{
						sfAssertf(0, "ScaleformMgr: Movie '%s' is getting requested by SCRIPT whilst it is already in use by CODE", params.cFilename);
					}

				}
#endif // #if __ASSERT

				return i;
			}
		}
	}

	//
	// find and use empty slot:
	//
	s32 iIndex = 0;

	while (HasMovieGotAnyState(iIndex))
	{
		iIndex ++;

		if (iIndex == sm_ScaleformArray.GetCount())
		{
#if __ASSERT
			sfDebugf3("ScaleformMgr: Begin");

			for (s32 iDebugCount = 0; iDebugCount < sm_ScaleformArray.GetCount(); iDebugCount++)
			{
				sfDebugf3("ScaleformMgr:    Movie id: %d  -  Store %d  -  Filename: %s   State: %d", iDebugCount, sm_ScaleformArray[iDebugCount].iMovieId, sm_ScaleformArray[iDebugCount].cFilename, (s32)sm_ScaleformArray[iDebugCount].iState);
			}

			sfDebugf3("ScaleformMgr: End");
#endif // __ASSERT

			sfAssertf(0, "ScaleformMgr: Too many Scaleform movies currently active!  Increase the store size - ScaleformMgrArray in gameconfig.xml!");
			return -1;
		}
	}

	sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_FLAGGED_FOR_USE;
	sm_ScaleformArray[iIndex].vPos.Set(params.vPos);
	sm_ScaleformArray[iIndex].vOriginalPos.Set(params.vPos);
	sm_ScaleformArray[iIndex].vRot.Set(params.vRot);
	sm_ScaleformArray[iIndex].vSize.Set(params.vSize);
	sm_ScaleformArray[iIndex].vOriginalSize.Set(params.vSize);
	sm_ScaleformArray[iIndex].bChangeMovieParams.Set(true);
	sm_ScaleformArray[iIndex].eScaleMode.Set(params.eScaleMode);
	strcpy(sm_ScaleformArray[iIndex].cFilename, params.cFilename);
	sm_ScaleformArray[iIndex].bRemovableAtInit = params.bRemovable;
	sm_ScaleformArray[iIndex].fCurrentTimestep = 0.0f;
	sm_ScaleformArray[iIndex].bUpdateMovie = false;
	sm_ScaleformArray[iIndex].iRendered = 0;
	sm_ScaleformArray[iIndex].bNeedsToUpdateThisFrame = false;
	sm_ScaleformArray[iIndex].bIgnoreSuperWidescreenScaling.Set(params.bIgnoreSuperWidescreenScaling);
	sm_ScaleformArray[iIndex].iDependentMovie = params.iDependentMovie;
	sm_ScaleformArray[iIndex].bDontRenderWhilePaused = params.bDontRenderWhilePaused;
	sm_ScaleformArray[iIndex].bForceRemoval = false;

#if __SCALEFORM_CRITICAL_SECTIONS
	if (params.bInitiallyLocked)
	{
		gs_ScaleformMovieCS[iIndex].Lock();
	}
#endif // __SCALEFORM_CRITICAL_SECTIONS

	sfAssertf(params.movieOwnerTag != SF_MOVIE_TAGGED_BY_NONE, "ScaleformMgr: no movie owner has been specified for movie %s (%d)", params.cFilename, iIndex);

	sm_ScaleformArray[iIndex].movieOwnerTag = (u8)params.movieOwnerTag;

	StreamMovie(iIndex, params.cFilename, params.bStream, params.bForceLoad, params.iParentMovie, params.bRequiresMovieView);

	if(sm_ScaleformArray[iIndex].iMovieId == -1)
	{
		sfAssertf(0, "ScaleformMgr: Failed to stream movie: returning index -1");
		return -1;
	}

	sfDebugf1("ScaleformMgr: Movie %s been set up to use sm_ScaleformArray slot %d", sm_ScaleformArray[iIndex].cFilename, iIndex);

	if (sm_ScaleformArray[iIndex].iDependentMovie != -1)
	{
		sfDebugf1("	                          Dependent movie %s (%d)", sm_ScaleformArray[sm_ScaleformArray[iIndex].iDependentMovie].cFilename, sm_ScaleformArray[iIndex].iDependentMovie);
	}

	if (sm_ScaleformArray[iIndex].iParentMovie != -1)
	{
		sfDebugf1("	                          Parent movie %s (%d)", sm_ScaleformArray[sm_ScaleformArray[iIndex].iParentMovie].cFilename, sm_ScaleformArray[iIndex].iParentMovie);
	}

	if (g_ScaleformStore.GetParentTxdForSlot(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)) != -1)
	{
		sfDebugf1("	                          Texture slot %d", g_ScaleformStore.GetParentTxdForSlot(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)).Get());
	}

	for (s32 i = 0; i < MAX_ASSET_MOVIES; i++)
	{
		s32 iMovieAssetSlot = g_ScaleformStore.GetMovieAssetForSlot(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), i);

		if (iMovieAssetSlot != -1)
			sfDebugf1("	                          Asset movie slot %d", iMovieAssetSlot);
	}

	return iIndex;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::InitTextures()
// PURPOSE: sets the parent texture slot for each movie slot
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::InitTextures(s32 iIndex, const char *cFilename)
{
	// If this is already setup then dont do anything else
	if(g_ScaleformStore.GetParentTxdForSlot(strLocalIndex(iIndex)) != -1)
		return;

	// DND DLH
	// Need an extra temp char buffer here to avoid strcpy with overlapping memory range errors
	s32 iSlotTextureId = -1;
	char cTxdNameTemp[RAGE_MAX_PATH];
	char cTxdName[RAGE_MAX_PATH];
	char cTxdNamePlatformSpecific[RAGE_MAX_PATH];
	ASSET.RemoveExtensionFromPath(cTxdNameTemp, RAGE_MAX_PATH, cFilename);
	strcpy(cTxdName, ASSET.FileName(cTxdNameTemp));
	strcpy(cTxdNamePlatformSpecific, cTxdName);

#ifdef SCALEFORM_MOVIE_PLATFORM_SUFFIX
	safecat(cTxdNamePlatformSpecific, SCALEFORM_MOVIE_PLATFORM_SUFFIX, RAGE_MAX_PATH);
#else
	sfAssertf(0, "Invalid platform specific texture for %s", cTxdNamePlatformSpecific);  // NOTHING VALID!
	return -1;
#endif

	// check to see how many chars (if any) the filename marker takes up, so we can remove it from the name we look for
	s32 iFilenameMarkerOffset = 0;
	if (!strncmp("non_str_", cFilename, 8))
	{
		iFilenameMarkerOffset = 8;
	}

	bool bFoundPlatformSpecific = true;
	char* texDictName = &cTxdNamePlatformSpecific[iFilenameMarkerOffset];
	iSlotTextureId = g_TxdStore.FindSlot(texDictName).Get();  // filename, less any marker chars

	if (iSlotTextureId == -1)
	{
		texDictName = &cTxdName[iFilenameMarkerOffset];
		iSlotTextureId = g_TxdStore.FindSlot(texDictName).Get();  // filename, less any marker chars
		bFoundPlatformSpecific = false;
	}

	if (iSlotTextureId != -1)
	{
		g_ScaleformStore.SetParentTxdForSlot(strLocalIndex(iIndex), strLocalIndex(iSlotTextureId));
#if !__FINAL
		if (strlen(texDictName) >= SCALEFORM_DICT_MAX_PATH)
			Quitf("Scaleform dict path: %s is too long (%d). Decrease to < %d characters!", texDictName, (int)strlen(texDictName), SCALEFORM_DICT_MAX_PATH);
#endif		
		safecpy(g_ScaleformStore.GetSlot(strLocalIndex(iIndex))->m_cFullTexDictName, texDictName);
		sfDisplayf("Set Movie %s (%d) to use texture %s (%d)", cFilename, iIndex, texDictName, iSlotTextureId);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::InitMovieAssets()
// PURPOSE: sets the parent texture slot for each movie slot
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::InitMovieAssets(s32 iIndex, const char *cFilename, s32 iAssetIndex)
{
	// If this is already setup then dont do anything else
	if (g_ScaleformStore.GetMovieAssetForSlot(strLocalIndex(iIndex), iAssetIndex) != -1)
		return;

	// Load a texture dictionary (if one exists)
	s32 iSlotId = -1;
	iSlotId = g_ScaleformStore.FindSlot(cFilename).Get();  // filename, less any marker chars

	if (iSlotId != -1)
	{
		g_ScaleformStore.SetMovieAssetForSlot(strLocalIndex(iIndex), iSlotId, iAssetIndex);

		sfDisplayf("Set Movie %s (%d) to use asset movie %s (%d)", g_ScaleformStore.GetName(strLocalIndex(iIndex)), iIndex, cFilename, iAssetIndex);
	}
	else
	{
		sfAssertf(0, "Invalid asset (%s) assigned to movie %s", cFilename, g_ScaleformStore.GetName(strLocalIndex(iIndex)));
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::LoadTextures()
// PURPOSE: loads in the textures for the movie if we are not streaming
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::LoadTextures(const char *cFilename)
{
	// Load a texture dictionary (if one exists)
	char cTxdName[RAGE_MAX_PATH];
	char cTxdNamePlatformSpecific[RAGE_MAX_PATH];

	ASSET.RemoveExtensionFromPath(cTxdName, RAGE_MAX_PATH, cFilename);
	sprintf(cTxdName, "platform:/textures/%s", ASSET.FileName(cTxdName));

	safecpy(cTxdNamePlatformSpecific, cTxdName);
/*  // code no longer needed as the url builder deals with the font library filename changes for the actionscript linkages and seperate gfx now exist
#if __XENON || __WIN32PC			// TEMPORARY HACK (somewhat reasonable since PC may well use 360 button faces)
		safecat(cTxdNamePlatformSpecific, "_360", RAGE_MAX_PATH);  // XBOX 360 ONLY
#elif __PPU
		safecat(cTxdNamePlatformSpecific, "_PS3", RAGE_MAX_PATH);  // PS3 ONLY
#else
		sfAssertf(0, "Invalid platform specific texture for %s", cTxdName);  // NOTHING VALID!
		return -1;
#endif*/
	strLocalIndex iSlotTextureId = g_TxdStore.FindSlot(cTxdName);
	if(iSlotTextureId == -1)
		iSlotTextureId = g_TxdStore.FindSlot(cTxdNamePlatformSpecific);

	if (iSlotTextureId != -1)
	{
		CStreaming::LoadObject(iSlotTextureId, g_TxdStore.GetStreamingModuleId());
	}

	return iSlotTextureId.Get();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::DoesMovieExistInImage()
// PURPOSE: returns whether the filename passed in exists in the scaleform store
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::DoesMovieExistInImage(char *pFilename)
{
	return (g_ScaleformStore.FindSlot(pFilename) != -1);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::StreamMovie()
// PURPOSE: streams in the movie itself
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::StreamMovie(s32 iIndex, const char *cFilename, bool bStream, bool bForceLoad, s32 iParentMovie, bool bMovieView)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::StreamMovie can only be called on the UpdateThread!");
		return;
	}

	STRVIS_AUTO_CONTEXT(strStreamingVisualize::SCALEFORM);

	// During the loading screens, StreamMovie can trigger a KeepAliveCallback which can then call UpdateAtEndOfFrame.
	// But we can get some out-of-order locks if we do this.
	g_DontDoUpdateAtEndOfFrame = true; 

	//
	// stream the movie
	//
	if (bStream)
	{
		sm_ScaleformArray[iIndex].iMovieId = g_ScaleformStore.FindSlot(cFilename).Get();

		if(sfVerifyf(sm_ScaleformArray[iIndex].iMovieId != -1, "%s:Scaleform movie doesn't exist", cFilename))
		{
			g_ScaleformStore.SetMovieAsRequiringMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), bMovieView);

			if (iParentMovie != -1)  // if this movie has a parent then store its parent's id
			{
				sm_ScaleformArray[iIndex].iParentMovie = iParentMovie;
			}
			else
			{
				sm_ScaleformArray[iIndex].iParentMovie = -1;
			}

			g_ScaleformStore.StreamingRequest(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), MOVIE_STREAMING_FLAGS);

			if (bForceLoad)  // force the movie to load in now
			{
				CStreaming::LoadAllRequestedObjects(true);  // force the load NOW

				if (!g_ScaleformStore.HasObjectLoaded(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)))
				{
					sfAssertf(0, "ScaleformMgr: LoadAllRequestedObjects failed so Movie '%s' GFX has failed to block load and will now be streamed", cFilename);
					sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_STREAMING_MOVIE;
				}
			}
			else
			{
				sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_STREAMING_MOVIE;
			}
		}
	}
	else
	//
	// load the movie directly from the path - ideal (for now) for the loadingscreens and font system when the
	// streaming system hasnt initialised yet
	//
	{
		char cStoreName[RAGE_MAX_PATH];

		ASSET.RemoveExtensionFromPath(cStoreName, RAGE_MAX_PATH, cFilename);
		strcpy(cStoreName, ASSET.FileName(cStoreName));

		char cNewStoreName[RAGE_MAX_PATH];
		sprintf(cNewStoreName, "non_str_%s", cStoreName);

		sm_ScaleformArray[iIndex].iMovieId = g_ScaleformStore.FindSlot(cNewStoreName).Get();

		sm_ScaleformArray[iIndex].iParentMovie = -1;

#if __DEV
		sm_ScaleformArray[iIndex].iNumRefs = 0;
#endif // __DEV

		if (sm_ScaleformArray[iIndex].iMovieId == -1)
		{
			sm_ScaleformArray[iIndex].iMovieId = g_ScaleformStore.AddSlot(strStreamingObjectNameString(cNewStoreName)).Get();
			sfAssertf(sm_ScaleformArray[iIndex].iMovieId != -1, "ScaleformMgr: Slot could not be added for movie %s", cNewStoreName);
		}

		strcpy(sm_ScaleformArray[iIndex].cFilename, cNewStoreName);

		s32 iTextureId = LoadTextures(cFilename);

		g_ScaleformStore.SetParentTxdForSlot(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), strLocalIndex(iTextureId));

		//
		// load directly from path (no stream):
		//

		g_ScaleformStore.LoadFile(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), cFilename);
	}

	//
	// if movie is already loaded in the store, then create the movie view here (so it gets created as early as it can in the process)
	// if it is not loaded, then the movie view will get created later on when it is loaded in by the streaming system
	//
	if (sfVerifyf(sm_ScaleformArray[iIndex].iMovieId != -1,"ScaleformMgr: Scaleform Movie %s failed to load...", cFilename))
	{
		// Need to re-create movie view if scaleform slot hasnt been streamed out after a RemoveMovieView()
		if (g_ScaleformStore.HasObjectLoaded(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)))
		{
			g_ScaleformStore.CreateMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));
		}

		if (!bStream || bForceLoad)
		{
			bool bSetToActiveNow = true; // set to active unless we find we cant...

			if (bStream && bForceLoad)
			{
				if (!g_ScaleformStore.HasObjectLoaded(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)))
				{
					bSetToActiveNow = false;
					sfDebugf1("ScaleformMgr: Scaleform Movie %s failed to block load, so now streaming...", cFilename);
				}
			}

			if (bSetToActiveNow)
			{
				InitialiseAndSetActive(iIndex);
				sfDebugf1("ScaleformMgr: Scaleform Movie %s created in slot %d (%d) (blocking load)", cFilename, iIndex, sm_ScaleformArray[iIndex].iMovieId);
			}
		}
	}

	g_DontDoUpdateAtEndOfFrame = false;
}


void CScaleformMgr::ForceMovieUpdateInstantly(s32 iIndex, bool bUpdate)
{
	if (sfVerifyf(IsMovieActive(iIndex), "ScaleformMgr: ForceMovieUpdateInstantly called before movie is active"))
	{
		if (sfVerifyf(GetMovieView(iIndex), "ScaleformMgr: ForceMovieUpdateInstantly called on a movie without a movieview"))
		{
#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
#endif // __SCALEFORM_CRITICAL_SECTIONS

			sm_ScaleformArray[iIndex].bUpdateMovie = bUpdate;
		}
	}
}

void CScaleformMgr::FlipBuffers(bool bCalledDuringRegularUpdate)
{
	if( bCalledDuringRegularUpdate || sm_MethodInvokeInfo[1-sm_iMethodInvokeInfoBufferInUse].empty())
	{
#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(sm_InvokeFlushToken);
#endif
		// flip buffers
		sm_iMethodInvokeInfoBufferInUse = 1-sm_iMethodInvokeInfoBufferInUse;

#if !__NO_OUTPUT
		if( !sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse].empty() )
			uiWarningf("Flipped to a non-empty invoke buffer! %i calls to Scaleform will be out of order! Calls like %s!", sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse].GetCount(), sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse][0].cMethodName);
#endif
	}
	else
	{
		uiWarningf("Attempted to flip invoke buffers before we'd had a chance to flush the %i invokes! We got ahead of ourselves, and so we skipped the buffer flip this time.", sm_MethodInvokeInfo[1-sm_iMethodInvokeInfoBufferInUse].GetCount());
	}

#if TRACK_FLIP_BUFFERS
	static u32 lastFlipTime = ~0u;
	static const int CALLSTACK_SIZE = 32;
	static size_t	lastFlipCallstack[CALLSTACK_SIZE];
	size_t			nowFlipCallstack[CALLSTACK_SIZE];
	sysStack::CaptureStackTrace(nowFlipCallstack, CALLSTACK_SIZE);
	if( bCalledDuringRegularUpdate && sm_BeginFrameCount==lastFlipTime && memcmp(nowFlipCallstack, lastFlipCallstack, sizeof(nowFlipCallstack)) != 0)
	{
		struct localScope
		{
			static void uiErrorDisplayLine(size_t OUTPUT_ONLY(addr),const char *OUTPUT_ONLY(sym),size_t OUTPUT_ONLY(offset)) {
#	if __64BIT
				uiErrorf("0x%016" SIZETFMT "x - %s+0x%" SIZETFMT "x",addr,sym,offset);
#	else
				uiErrorf("0x%08" SIZETFMT "x - %s+0x%" SIZETFMT "x",addr,sym,offset);
#	endif
			}
		};
		uiErrorf("Double flipped the buffer! Previous flip called from: ");
		sysStack::PrintCapturedStackTrace(lastFlipCallstack, CALLSTACK_SIZE, localScope::uiErrorDisplayLine);
		uiErrorf("Second flip called from: ");
		sysStack::PrintStackTrace(localScope::uiErrorDisplayLine);
#if __ASSERT
		uiAssertf(false, "Flipped the Scaleform Method buffers 2+ times this frame! This is pretty effing bad! See stack dumps preceding this assert for details!");
#else
		uiErrorf("Flipped the Scaleform Method buffers 2+ times this frame! This is pretty effing bad! See stack dumps preceding this assert for details!");
#endif
	}
	memcpy(lastFlipCallstack, nowFlipCallstack, sizeof(nowFlipCallstack));

	lastFlipTime = sm_BeginFrameCount;
#endif

	//sfDebugf3("ScaleformMgr: Flipping buffers (new buffer %d)...", sm_iMethodInvokeInfoBufferInUse);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CreateFontMovie()
// PURPOSE: sets up the font movie
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::CreateFontMovie()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::CreateFontMovie can only be called on the UpdateThread!");
		return -1;
	}

	s32 iIndex = CreateMovieAndWaitForLoad(CText::GetFullFontLibFilename(), Vector2(0,0), Vector2(0,0), false);

	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Cannot create Scaleform Fonts!"))
	{
		ms_iFontMovieId = sm_ScaleformArray[iIndex].iMovieId;

		GPtr<GFxFontLib> fontLib = *new GFxFontLib;
		m_staticMovieMgr->GetLoader()->SetFontLib(fontLib);
		AddGlobalFontsToLib(fontLib);

#if !__NO_OUTPUT
		GStringHash<GString> fontNames;
		fontLib->LoadFontNames(fontNames);
		int i = 0;
		for(GStringHash<GString>::Iterator iter = fontNames.Begin(); iter != fontNames.End(); ++iter)
		{
			sfDebugf2("Font %d: %s, %s", i, (*iter).First.ToCStr(), (*iter).Second.ToCStr());
			i++;
		}
#endif

		sfScaleformMovie *rawmov = NULL;

		rawmov = g_ScaleformStore.GetRawMovie(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

		m_staticMovieMgr->InitDrawTextManager(*rawmov);
		m_staticMovieMgr->GetDrawTextManager()->SetFontLib(fontLib); // Need to set it explicitly, or it will change when the loader does

		parTree* fontMap = PARSER.LoadTree("common:/data/ui/fontmap.xml", "");
		if (fontMap && fontMap->GetRoot())
		{
			HandleXML(fontMap->GetRoot());
			delete fontMap;
		}
	}

	return iIndex;
}

void CScaleformMgr::AddGlobalFontsToLib(GFxFontLib* lib)
{
	g_ScaleformStore.AddFontsToLib(lib, ms_iFontMovieId);
}

bool CScaleformMgr::GetMappedFontName( char const * const unmappedName, GString& out_mappedName )
{
	GPtr<GFxFontMap>pfontMap = m_staticMovieMgr && m_staticMovieMgr->GetLoader() ? m_staticMovieMgr->GetLoader()->GetFontMap() : 0;

	GFxFontMap::MapEntry fontEntry;
	bool c_mappingFound = pfontMap ? pfontMap->GetFontMapping( &fontEntry, unmappedName ) : false;
	out_mappedName = c_mappingFound ? fontEntry.Name : "";

	return c_mappingFound;
}

bool CScaleformMgr::DoFontsShareMapping( char const * const unmappedNameA, char const * const unmappedNameB )
{
	GString mappedNameA;
	bool const c_mappedA = GetMappedFontName( unmappedNameA, mappedNameA );

	GString mappedNameB;
	bool const c_mappedB = GetMappedFontName( unmappedNameB, mappedNameB );

	return c_mappedA && c_mappedB && mappedNameA == mappedNameB;
}

void CScaleformMgr::HandleXML(parTreeNode* pFontMapNode)
{

	// only share the fonts of the streamed font movie:
	// DP: COMMENTED OUT PENDING ANSWER FROM SCALEFORM SUPPORT AS THIS REMOVES THE LINK BETWEEN SHARED TEXTURES BETWEEN MOVIES AND FONTLIB:
	if (m_staticMovieMgr->GetLoader()->GetFontMap())
	{
		m_staticMovieMgr->GetLoader()->GetFontMap()->Release();
	}

	GPtr<GFxFontMap>pfontMap = *new GFxFontMap;
	m_staticMovieMgr->GetLoader()->SetFontMap(pfontMap);

	const char *pFontFileName = CText::GetLanguageFilename();
	parTreeNode* pNode = NULL;
	while ((pNode = pFontMapNode->FindChildWithName(pFontFileName, pNode)) != NULL)
	{
		char cFontName[100];
		char cMappingName[100];
		bool bBold;
		GFxFontMap::MapFontFlags MapFontFlags;

		formatf(cFontName, pNode->GetElement().FindAttributeAnyCase("name")->GetStringValue(), NELEM(cFontName));
		formatf(cMappingName, pNode->GetElement().FindAttributeAnyCase("mapping")->GetStringValue(), NELEM(cMappingName));
		bBold = ((s32)atoi(pNode->GetElement().FindAttributeAnyCase("bold")->GetStringValue()) == 1);

		if (bBold)
		{
			MapFontFlags = GFxFontMap::MFF_Bold;
		}
		else
		{
			MapFontFlags = GFxFontMap::MFF_Original;
		}

		pfontMap->MapFont(cFontName, cMappingName, MapFontFlags);
	}

}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RemoveFontMovie()
// PURPOSE: removes the font movie
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RemoveFontMovie()
{
	m_staticMovieMgr->ShutdownDrawTextManager();
}

void CScaleformMgr::SetForceShutdown(s32 iIndex)
{
	sm_ScaleformArray[iIndex].bForceRemoval = true;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RequestRemoveMovie()
// PURPOSE: sets the movie to be deleted at the next safe point (note no delete is
//			actually done here)
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::RequestRemoveMovie(s32 iIndex, eMovieOwnerTags calledBy)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::RequestRemoveMovie can only be called on the UpdateThread!");
		return false;
	}

	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		if (sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE &&  // fix for 685736 (if already requested to be removed, do not request it again, let the original run)
			sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1 &&
			sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2 &&
			sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_SET_TO_REMOVE_NO_REF_CHECK)
		{
			if (sm_ScaleformArray[iIndex].iMovieId == -1)
			{
				sfAssertf(0, "ScaleformMgr: Tried to remove a movie (%d) that doesnt exist", iIndex);
				return false;
			}

	#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
	#endif // __SCALEFORM_CRITICAL_SECTIONS


			u8 currentOwnerTag = sm_ScaleformArray[iIndex].movieOwnerTag;

			currentOwnerTag &= ~calledBy;

			if (currentOwnerTag == SF_MOVIE_TAGGED_BY_NONE)
			{
				sfDebugf3("ScaleformMgr: movie %s (%d) has been set to SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE state", sm_ScaleformArray[iIndex].cFilename, iIndex);

				// if it is a child movie, we want to ensure that the parent has the garbage collection done before it is removed
				s32 iParentId = sm_ScaleformArray[iIndex].iParentMovie;
				if (iParentId != -1)
				{
					sfDebugf3("ScaleformMgr: movie %s (%d) has set its parent (%s - %d) to have its garbage collection done", sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iParentId].cFilename, iParentId);

					CScaleformMgr::ForceCollectGarbage(iParentId);
				}

				sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE;
			}

			return true;
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::SetScriptRequired()
// PURPOSE: sets the movie streaming flag to use STRFLAG_MISSION_REQUIRED or not
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::SetScriptRequired(s32 iIndex, bool bScriptRequired)
{
	if (!IsMovieIdInRange(iIndex))
		return;

	const int iMovieId = sm_ScaleformArray[iIndex].iMovieId;

//// TODO: Remove this temporary hack which is for working around [PT] crash B*1702494 ////
if (iMovieId < 0) return;
///////////////////////////////////////////////////////////////////////////////////////////

	if (bScriptRequired)
	{
		g_ScaleformStore.SetRequiredFlag(iMovieId, STRFLAG_MISSION_REQUIRED);
	}
	else
	{
		g_ScaleformStore.ClearRequiredFlag(iMovieId, STRFLAG_MISSION_REQUIRED);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::DestroyMovieNow()
// PURPOSE: Actually deletes one movie
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::DestroyMovieNow(s32 iIndex)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::DestroyMovieNow can only be called on the UpdateThread!");
		return false;
	}

	bool bSuccess = false;

	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		if (sm_ScaleformArray[iIndex].iMovieId != -1)  // if it has a movie id then it must have a movie set up on it
		{
	#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
	#endif // __SCALEFORM_CRITICAL_SECTIONS

			m_staticMovieMgr->FinishedDrawing();

#if __BANK
			s32 const c_numberOfActiveMovieClips = CScaleformComplexObjectMgr::FindNumberOfObjectsInUseByMovie(iIndex);  // check for anything still set up

			if (c_numberOfActiveMovieClips != 0)
			{
				CScaleformComplexObjectMgr::OutputMovieClipsInUseByMovieToLog(iIndex);

				sfAssertf(0, "Scaleform Movie %s (%d) is about to be destroyed but has %d movieclips open on it (see log for list of movieclips open)", sm_ScaleformArray[iIndex].cFilename, iIndex, c_numberOfActiveMovieClips);
			}
#endif // __BANK

			CScaleformComplexObjectMgr::ForceRemoveAllObjectsInUseByMovie(iIndex);  // ensure any flagged objects are removed if they havnt been already

			//
			// only remove from the store if there are no other movie instances using it:
			//
			bool bRemoveFromStore = true;

			for (s32 iCount1 = 0; iCount1 < sm_ScaleformArray.GetCount(); iCount1++)
			{
				if (iCount1 != iIndex)
				{
					if (sm_ScaleformArray[iCount1].iMovieId != -1)
					{
						if (sm_ScaleformArray[iCount1].iMovieId == sm_ScaleformArray[iIndex].iMovieId)
						{
							bRemoveFromStore = false;  // found another movie that uses this store index, so dont remove it
						}

						// also need to check to see if there is any movies that think this is its parent, and if it is
						// tell the child movie the parent is gone
						if (g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iCount1].iMovieId)))
						{
							s32 iParentId = sm_ScaleformArray[iCount1].iParentMovie;

							if (iParentId == iIndex)  // parent id matches this movie id
							{
								sfDebugf3("Scaleform Movie %s object is about to be destroyed but %s (%d) movie is marked as its child, so remove the link", sm_ScaleformArray[iIndex].cFilename, sm_ScaleformArray[iCount1].cFilename, iCount1);

								// so set the child not have this as a parent anymore so it doesnt try to force garbage collection
								// on a parent that no longer exists, or worse, a movie that has since taken up the slot tha parent used to reside
								sm_ScaleformArray[iCount1].iParentMovie = -1;
							}
						}
					}
				}
			}

			if (bRemoveFromStore)
			{
				sfDebugf1("ScaleformMgr: Movie %s has been set as no longer needed %d (%d)", sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iIndex].iMovieId);

				// reset/uninitialise any data in the movie here (as soon as its requested to be deleted) - if its reused, it will use fresh data
				GFxMovieView *pMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

				if (pMovieView)
				{
					g_ScaleformStore.RemoveMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));
				}

				for(int txdReqIdx = 0; txdReqIdx < sm_ScaleformArray[iIndex].requestedTxds.GetCount(); txdReqIdx++)
				{
					strLocalIndex iTxdIdx = sm_ScaleformArray[iIndex].requestedTxds[txdReqIdx].iTxdId;

					if (g_TxdStore.IsObjectInImage(iTxdIdx))
					{
						// This could clear the dontdelete flag on txd that's not yet loaded in. That's ok though because if another movie
						// was trying to load the same txd, it'll call HasLoaded() on the next frame which will re-request the txd if necessary
						g_TxdStore.ClearRequiredFlag(iTxdIdx.Get(), STRFLAG_DONTDELETE);
						sm_ScaleformArray[iIndex].requestedTxds[txdReqIdx].streamingRequest.ClearMyRequestFlags(STRFLAG_DONTDELETE);
					}
					PrintTxdRef("Shutting down movie", iTxdIdx);
				}
				sm_ScaleformArray[iIndex].requestedTxds.Reset();

				// Remove the flags we gave it. Note that we're normally not allowed to touch the priority flag.
				g_ScaleformStore.ClearRequiredFlag(sm_ScaleformArray[iIndex].iMovieId, MOVIE_STREAMING_FLAGS & ~STRFLAG_PRIORITY_LOAD);

				// only remove the ref if we have added one.  This may not happen if the file has been requested but not actually streamed and set up yet by the time its been deleted (bug 95868)
				if (g_ScaleformStore.GetNumRefs(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)) > 0)
				{
					g_ScaleformStore.RemoveRef(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), REF_OTHER);

					// If the movie isn't managed by streaming then set any related txd to not needed
					if(!g_ScaleformStore.IsObjectInImage(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)))
					{
						strLocalIndex txdSlot = strLocalIndex(g_ScaleformStore.GetParentTxdForSlot(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)));
						if(txdSlot != -1)
						{
							if(!g_TxdStore.IsObjectInImage(txdSlot))
								g_TxdStore.Remove(txdSlot);
							else
							{
								g_TxdStore.ClearRequiredFlag(txdSlot.Get(), STRFLAG_DONTDELETE);
								//if(g_TxdStore.GetNumRefs(txdSlot) == 0)
								//	g_TxdStore.StreamingRemove(txdSlot);
							}
						}

						for (s32 i = 0; i < MAX_ASSET_MOVIES; i++)
						{
							int movieSlot = g_ScaleformStore.GetMovieAssetForSlot(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), i);
							if(movieSlot != -1)
							{
								if(!g_ScaleformStore.IsObjectInImage(strLocalIndex(movieSlot)))
									g_ScaleformStore.Remove(strLocalIndex(movieSlot));
								else
								{
									g_ScaleformStore.ClearRequiredFlag(movieSlot, STRFLAG_DONTDELETE);
									if(g_ScaleformStore.GetNumRefs(strLocalIndex(movieSlot)) == 0)
										g_ScaleformStore.StreamingRemove(strLocalIndex(movieSlot));
								}
							}
						}
					}
				}
			}

			ClearScaleformArraySlot(iIndex);

			bSuccess = true;
		}
	}

	return bSuccess;
}


#define __DEBUG_PENDING_BUFFERED_INFO (0)

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::DoesMovieHaveInvokesPending()
// PURPOSE: returns whether there are any invokes in the buffer
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::DoesMovieHaveInvokesPending(s32 iIndex)
{
#if __DEBUG_PENDING_BUFFERED_INFO
	bool bFoundAnyBufferedMethodCalls = false;
#endif // __DEBUG_PENDING_BUFFERED_INFO

	if (HasMovieGotAnyState(iIndex))
	{
		// ensure there are no buffered invokes:
#if __DEBUG_PENDING_BUFFERED_INFO
		sfDebugf3(">>> BEGIN Invoke BUFFER (In Use: %d)", sm_iMethodInvokeInfoBufferInUse);
#endif //#if __DEBUG_PENDING_BUFFERED_INFO

		for (s32 i = 0; i < sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse].GetCount(); i++)
		{
			if (sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse][i].iMovieId == iIndex || sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse][i].iMovieIdThisInvokeIsLinkedTo == iIndex)
			{
#if __DEBUG_PENDING_BUFFERED_INFO
				sfDebugf3(">>>    %d. Found %s - %d", i, sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse][i].cMethodName, sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse][i].iMovieId);

				bFoundAnyBufferedMethodCalls = true;
#else
				return true;
#endif
			}
		}

#if __DEBUG_PENDING_BUFFERED_INFO
		sfDebugf3(">>> END Invoke BUFFER %d", sm_iMethodInvokeInfoBufferInUse);
#endif // #if __DEBUG_PENDING_BUFFERED_INFO
	}

#if __DEBUG_PENDING_BUFFERED_INFO
	return (bFoundAnyBufferedMethodCalls);
#else
	return false;
#endif
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::DoesMovieHaveCallbacksPending()
// PURPOSE: returns whether there are any callbacks in the buffer
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::DoesMovieHaveCallbacksPending(s32 iIndex)
{
#if __DEBUG_PENDING_BUFFERED_INFO
	bool bFoundAnyBufferedMethodCalls = false;
#endif //#if __DEBUG_PENDING_BUFFERED_INFO

	if (HasMovieGotAnyState(iIndex))
	{
		// ensure there are no buffered callbacks:
		sfDebugf3(">>> BEGIN Callback BUFFER (In Use: %d)", sm_iMethodInvokeInfoBufferInUse);

		for (s32 i = 0; i < sm_MethodCallbackInfo[sm_iMethodInvokeInfoBufferInUse].GetCount(); i++)
		{
			if (sm_MethodCallbackInfo[sm_iMethodInvokeInfoBufferInUse][i].iMovieId == iIndex)
			{
#if __DEBUG_PENDING_BUFFERED_INFO
				sfDebugf3(">>>    %d. Found %s - %d", i, sm_MethodCallbackInfo[sm_iMethodInvokeInfoBufferInUse][i].cMethodName, sm_MethodCallbackInfo[sm_iMethodInvokeInfoBufferInUse][i].iMovieId);

				bFoundAnyBufferedMethodCalls = true;
#else
				return true;
#endif
			}
		}

#if __DEBUG_PENDING_BUFFERED_INFO
		sfDebugf3(">>> END Callback BUFFER %d", sm_iMethodInvokeInfoBufferInUse);
#endif
	}

#if __DEBUG_PENDING_BUFFERED_INFO
	return (bFoundAnyBufferedMethodCalls);
#else
	return false;
#endif
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::DoesMovieHaveInvokesOrCallbacksPending()
// PURPOSE: returns whether there are any invokes or callbacks in the buffers
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::DoesMovieHaveInvokesOrCallbacksPending(s32 iIndex)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::DoesMovieHaveInvokesOrCallbacksPending can only be called on the UpdateThread!");
		return false;
	}

	bool bFoundAnyBufferedMethodCalls = false;

	if (HasMovieGotAnyState(iIndex))
	{
#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
#endif // __SCALEFORM_CRITICAL_SECTIONS

		bFoundAnyBufferedMethodCalls = DoesMovieHaveInvokesPending(iIndex);

		if (!bFoundAnyBufferedMethodCalls)
		{
			bFoundAnyBufferedMethodCalls = DoesMovieHaveCallbacksPending(iIndex);
		}
	}

	return (bFoundAnyBufferedMethodCalls);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::UpdateAtEndOfFrame()
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::UpdateAtEndOfFrame(bool bCalledDuringRegularUpdate)
{
	OUTPUT_ONLY( static u32 framesSpentNotUpdating = 0; )
	if (g_DontDoUpdateAtEndOfFrame)
	{
#if !__NO_OUTPUT
		uiAssertf(!bCalledDuringRegularUpdate || framesSpentNotUpdating<=1, "Update has been skipped for %i subsequent frames. This is pretty darned bad!", framesSpentNotUpdating);
		uiWarningf("Skipping SF update has been skipped for %i frames", framesSpentNotUpdating);
		++framesSpentNotUpdating;
#endif
		return;
	}

	OUTPUT_ONLY( framesSpentNotUpdating = 0; )

	SYS_CS_SYNC(GetRealSafeZoneToken());

#if !__NO_OUTPUT
	if (g_sfPrintFullMemoryReport)
	{
		g_strStreamingInterface->FullMemoryReport();
		g_sfPrintFullMemoryReport = false;
	}
#endif

	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::UpdateAtEndOfFrame can only be called on the UpdateThread!");
		return;
	}

	UpdateTxdReferences();

	UpdateAllMoviesStateMachines();

#if __BANK
	if (ms_bShowPerformanceInfo)
	{
		DisplayPerformanceInfo();
	}
#endif // __BANK

	if (ms_iReturnValueCount > 0)
	{
		// copy over any new ids to the access array:
		for (s32 i = 0; i < ms_iReturnValueCount; i++)
		{
			bool bFoundFreeSlot = false;
			for (s32 iR = 0; (!bFoundFreeSlot) && (iR < MAX_STORED_RETURN_VALUES); iR++)
			{
				if (sm_ReturnValuesForAccess[iR].iUniqueId == 0)
				{
					sm_ReturnValuesForAccess[iR] = sm_ReturnValuesFromInvoke[i];
					if (sm_ReturnValuesForAccess[iR].gfxvalue_return_value.IsString())
					{
						sm_ReturnValuesForAccess[iR].gfxvalue_return_value.SetString(
							sm_ReturnValuesForAccess[iR].stringData.c_str());
					}
					sm_ReturnValuesForAccess[iR].iFramesToStayValid = FRAMES_FOR_RETURNED_VALUE_TO_STAY_VALID;
					bFoundFreeSlot = true;
				}
			}

			sfAssertf(bFoundFreeSlot, "ScaleformMgr: Too many return values stored!");
		}
	}

#if __BANK
	if (ms_iReturnValueCount > 0)
	{
		sfDebugf3("BEGIN - RETURN VALUES AT END OF THIS FRAME");
	}
#endif // __BANK

	for (s32 iR = 0; iR < MAX_STORED_RETURN_VALUES; iR++)
	{
		if (sm_ReturnValuesForAccess[iR].iUniqueId != 0)
		{
			if (sm_ReturnValuesForAccess[iR].iFramesToStayValid > 0)
			{
				sm_ReturnValuesForAccess[iR].iFramesToStayValid--;

#if __BANK
				if (ms_iReturnValueCount > 0)
				{
					sfDebugf3("     %d - %d (frames left before delete %d", iR, sm_ReturnValuesForAccess[iR].iUniqueId, sm_ReturnValuesForAccess[iR].iFramesToStayValid);
				}
#endif // __BANK
			}
			else
			{
#if __BANK
					sfDebugf3("Return value %d was not accessed in %d frames.  Its now deleted", sm_ReturnValuesForAccess[iR].iUniqueId, FRAMES_FOR_RETURNED_VALUE_TO_STAY_VALID);
#endif // __BANK

				sm_ReturnValuesForAccess[iR].iUniqueId = 0;
				sm_ReturnValuesForAccess[iR].gfxvalue_return_value.SetUndefined();
				sm_ReturnValuesForAccess[iR].stringData.Clear();
				sm_ReturnValuesForAccess[iR].iFramesToStayValid = 0;
			}
		}
	}

#if __BANK
	if (ms_iReturnValueCount > 0)
	{
		sfDebugf3("END - RETURN VALUES AT END OF THIS FRAME");
	}
#endif // __BANK

	ms_iReturnValueCount = 0;

	ms_LastCameraPos = camInterface::GetPos();

	CScaleformMgr::FlipBuffers(bCalledDuringRegularUpdate);
	CScaleformComplexObjectMgr::SyncBuffers();

	if (gRenderThreadInterface.IsUsingDefaultRenderFunction())
	{
		m_staticMovieMgr->PerformSafeModeOperations();
	}
}

//////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::UpdateMovieStateMachine()
// PURPOSE: Updates the movies' states
//////////////////////////////////////////////////////////////////////////
void CScaleformMgr::UpdateAllMoviesStateMachines()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::UpdateMovieStateMachine can only be called on the UpdateThread!");
		return;
	}

	FlushCallbackList();

	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		//
		// if movie is waiting on textures loading, see if they have loaded:
		//
		switch (sm_ScaleformArray[iIndex].iState)
		{
		case SF_MOVIE_STATE_RESTARTING:
			{
				// HACK_OF_SORTS (ACTUALLY, NO, A REAL HACK)
				// Klaas dislikes the speed of restarting the switch movies so we do the crappy non-working scaleform Restart here, else my restart which is slow but works
				// Klaas knows about this hack
				bool bUseDietRestart = false;
				u32 iFilenameHash = atStringHash(sm_ScaleformArray[iIndex].cFilename);
				if ( (iFilenameHash == ATSTRINGHASH("PLAYER_SWITCH",0xb2a0e3c7)) ||
					(iFilenameHash == ATSTRINGHASH("PLAYER_SWITCH_PROLOGUE",0x5e4712fe)) ||
					(iFilenameHash == ATSTRINGHASH("PLAYER_SWITCH_PANEL",0xae5abfe7))||
					(iFilenameHash == ATSTRINGHASH("PLAYER_SWITCH_STATS_PANEL",0x9be50081)) )
				{
					sfDebugf1("Restarting movie %s (%d) with LITE restart", sm_ScaleformArray[iIndex].cFilename, iIndex);

					bUseDietRestart = true;
				}

				// END OF HACK_OF_SORTS (ACTUALLY, NO, A REAL HACK)

				if (RestartMovie(iIndex, false, bUseDietRestart))
				{
					sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_ACTIVE;

#if __BANK
					DebugTurnOnOffOutput(iIndex);  // turn on debug output for this movie if required
#endif
				}

				break;
			}

			//
			// if movie state is inactive, then check to see if its ready to become active
			//
		case SF_MOVIE_STATE_STREAMING_MOVIE:
			{
				// check for movie loaded and also its texture:
				if (g_ScaleformStore.HasObjectLoaded(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)))
				{
					if (InitialiseAndSetActive(iIndex))
					{
						sfDebugf1("ScaleformMgr: Scaleform Movie %s created in slot %d (%d) (streamed)", sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iIndex].iMovieId);
					}
				}

				break;
			}

			//
			// delete the movie:
			//
		case SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE:
		case SF_MOVIE_STATE_SET_TO_REMOVE_NO_REF_CHECK:
			{
				if (!g_ScaleformStore.HasObjectLoaded(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)))  // 1496431
				{
					sfDebugf3("ScaleformMgr: movie %s (%d) has been set to SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE state again as the movie has not loaded from the store yet", sm_ScaleformArray[iIndex].cFilename, iIndex);
					sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE;
				}
				else
					// only remove this movie now if there are no invokes in the buffer still to be flushed on it
					// if we found any buffered method calls, set it to update again 1 final time
					if (DoesMovieHaveInvokesOrCallbacksPending(iIndex))
					{
						if (sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE)
						{
							sfDebugf3("ScaleformMgr: movie %s (%d) has been set to SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE state again as there are still buffered invokes/callbacks on this movie", sm_ScaleformArray[iIndex].cFilename, iIndex);
							sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE;
						}
					}
					else
					{
						if (sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1 && sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2)
						{
							// ensure we dont remove this movie if any other movies are dependent on it
							bool bFound = false;
							for (s32 iCount = 0; ((!bFound) && (iCount < sm_ScaleformArray.GetCount())); iCount++)  // go through all movies and see if there are any that are dependent on this movie
							{
								if (iCount == iIndex)
									continue;

								if (sm_ScaleformArray[iCount].iDependentMovie == iIndex)
								{
									sfDebugf3("ScaleformMgr: movie %s (%d) waiting on dependent movie %s (%d) to be removed", sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iCount].cFilename, iCount);

									bFound = true;
									break;
								}
							}

							// ensure we dont remove this movie if there are children of it still active
							for (s32 iCount = 0; ((!bFound) && (iCount < sm_ScaleformArray.GetCount())); iCount++)  // go through all movies and see if there are any that are dependent on this movie
							{
								if (iCount == iIndex)
									continue;

								if (sm_ScaleformArray[iCount].iParentMovie == iIndex)
								{
									sfDebugf3("ScaleformMgr: movie %s (%d) waiting on child movie %s (%d) to be removed", sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iCount].cFilename, iCount);

									bFound = true;
									break;
								}
							}

							bool bAllowedToDestroy = false;

							if (!bFound)
							{
								if ( (sm_ScaleformArray[iIndex].iParentMovie == -1) || 
									 (sm_ScaleformArray[iIndex].bForceRemoval) ||
									(!g_ScaleformStore.HasObjectLoaded(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId))) )  // we only want to show refs on movies that are loaded and have a parent (to indicate if a parent still has a ref to it)
								{
									bAllowedToDestroy = true;
								}
								else
								{
									sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_SET_TO_REMOVE_NO_REF_CHECK;  // ensures we only check this once for this movie (so we can RETRY the assert and see future asserts on other movies)

									s32 iParentId = -1;

									if (sm_ScaleformArray[iIndex].iMovieId != -1)
									{
										iParentId = sm_ScaleformArray[iIndex].iParentMovie;

										// If there is a parent movie, flush it's text format caches, because they might have
										// text format objects that reference the child movie. We don't want these references stopping
										// us from unloading the child. Related to scaleform support ticket 854919
										if (iParentId >= 0)
										{
											CScaleformMovieObject* movieObj = g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iParentId].iMovieId));
											if (movieObj && movieObj->GetMovieView())
											{
												movieObj->GetMovieView()->FlushTextFormatCaches();
											}
										}
									}

									s32 iNumRefsAtRemoval = GetMovieNumRefs(iIndex);

									if (sfVerifyf(iNumRefsAtRemoval == 0, "ScaleformMgr: Movie %s (%d) has %d outstanding references at removal", sm_ScaleformArray[iIndex].cFilename, iIndex, iNumRefsAtRemoval))
									{
										bAllowedToDestroy = true;
									}

#if __BANK
									if (PARAM_forceremoval.Get())
									{
										bAllowedToDestroy = true;
									}
#endif // __BANK

								}
							}

							if (bAllowedToDestroy)
							{
								sfDebugf3("ScaleformMgr: movie %s (%d) has been set to SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1 state", sm_ScaleformArray[iIndex].cFilename, iIndex);
								sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1;
							}
						}
					}

					break;
			}

		case SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1:
			{
				if (DoesMovieHaveInvokesOrCallbacksPending(iIndex))
				{
					sfDebugf3("ScaleformMgr: movie %s (%d) has been set to SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE state again as there are still buffered invokes/callbacks on this movie when it was about to be removed", sm_ScaleformArray[iIndex].cFilename, iIndex);
					sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE;
				}
				else
				{
					sfDebugf3("ScaleformMgr: movie %s (%d) has been set to SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2 state", sm_ScaleformArray[iIndex].cFilename, iIndex);
					sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2;
				}

				s32 const c_numberOfActiveMovieClips = CScaleformComplexObjectMgr::FindNumberOfObjectsInUseByMovie(iIndex);  // check for anything still set up

				if (c_numberOfActiveMovieClips != 0)
				{
					sfDebugf3("ScaleformMgr: movie %s (%d) has been set to SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE state again as there are still complex objects active on this movie when it was about to be removed", sm_ScaleformArray[iIndex].cFilename, iIndex);

					CScaleformComplexObjectMgr::ReleaseAllObjectsInUseByMovie(iIndex);
					sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE;
				}

				break;
			}

		case SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2:
			{
				DestroyMovieNow(iIndex);
				break;
			}

		case SF_MOVIE_STATE_ACTIVE:
			{
				for (s32 iTxdReqSlot = 0; iTxdReqSlot < sm_ScaleformArray[iIndex].requestedTxds.GetCount(); iTxdReqSlot++)
				{
					ScaleformMovieTxd& requestedTxd = sm_ScaleformArray[iIndex].requestedTxds[iTxdReqSlot];
					OUTPUT_ONLY(strLocalIndex txdSlot = requestedTxd.iTxdId;)

						bool bIsValidRequest = g_TxdStore.IsValidSlot(requestedTxd.iTxdId);
					bool bIsTxdReady = requestedTxd.AlreadyLoaded();

					if (!bIsValidRequest || bIsTxdReady)
					{
#if !__NO_OUTPUT
						if (requestedTxd.iSpamCounter > 0)
						{
							sfDebugf1("Found a completed TXD request %s (%d)", g_TxdStore.GetName(txdSlot), txdSlot.Get());
						}
#endif

						if (requestedTxd.bReportSuccessToActionScript)
						{
							requestedTxd.bReportSuccessToActionScript = false;

							s32 iMovieToCallMethodOn = iIndex;

							if (requestedTxd.cUniqueRefString[0] != '\0')
							{
								s32 iLinkedMovieIdToCallOn = CScaleformMgr::FindMovieByFilename(requestedTxd.cUniqueRefString);

								if (iLinkedMovieIdToCallOn != -1)
								{
									iMovieToCallMethodOn = iLinkedMovieIdToCallOn;
								}
							}

							eSCALEFORM_BASE_CLASS iBaseClass = SF_BASE_CLASS_GENERIC;  // fix for 740752
							if (iMovieToCallMethodOn == CNewHud::GetContainerMovieId())
							{
								iBaseClass = SF_BASE_CLASS_HUD;
							}

							if (CScaleformMgr::BeginMethod(iMovieToCallMethodOn, iBaseClass, "TXD_HAS_LOADED"))
							{
								CScaleformMgr::AddParamString(requestedTxd.cTxdName);
								CScaleformMgr::AddParamBool(bIsValidRequest);  // if not -1 then success (TRUE), otherwise report fail (FALSE)
								CScaleformMgr::AddParamString(requestedTxd.cUniqueRefString);
								CScaleformMgr::EndMethod();
							}

							sfDebugf1("TXD_HAS_LOADED response sent to Scaleform movie %s (%d) - txd '%s' %d (unique string: %s)", sm_ScaleformArray[iIndex].cFilename, iIndex, g_TxdStore.GetName(txdSlot), txdSlot.Get(), requestedTxd.cUniqueRefString);

							requestedTxd.cUniqueRefString[0] = '\0';  // reset the unique string as its no longer needed after this point
						}

						if (!bIsValidRequest)
						{
							sm_ScaleformArray[iIndex].requestedTxds.DeleteFast(iTxdReqSlot);
							iTxdReqSlot--;
						}
					}
				}

				if (!sm_ScaleformArray[iIndex].bUpdateMovie)
				{
					if ( (GetMovieView(iIndex)) && (iIndex != CText::GetFontMovieId()) )  // only set to update if not a child movie (as child movies are updated via their parent) - also never update font movie
					{
						sm_ScaleformArray[iIndex].bUpdateMovie = true;
						//						sfDisplayf("Scaleform movie %s will update next frame", sm_ScaleformArray[iIndex].cFilename);
					}
				}

#if __DEV
				if (PARAM_logactionscriptrefs.Get())
				{
					s32 iNumRefAtEndOfFrame = GetMovieNumRefs(iIndex);
					if (iNumRefAtEndOfFrame != sm_ScaleformArray[iIndex].iNumRefs)
					{
						if (iNumRefAtEndOfFrame > sm_ScaleformArray[iIndex].iNumRefs)
						{
							sfDebugf3("ScaleformMgr: movie %s (%d) has increased refs from %d to %d (ADDREF)", sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iIndex].iNumRefs, iNumRefAtEndOfFrame);
						}
						else
						{
							sfDebugf3("ScaleformMgr: movie %s (%d) has decreased refs from %d to %d (REMOVEREF)", sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iIndex].iNumRefs, iNumRefAtEndOfFrame);
						}

						sm_ScaleformArray[iIndex].iNumRefs = iNumRefAtEndOfFrame;
					}
				}
#endif // __DEV

				// Since Scaleform might allocate memory at runtime, we need to update our cached memory number.
				strStreamingEngine::GetInfo().UpdateLoadedInfo(g_ScaleformStore.GetStreamingIndex(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)),
					g_ScaleformStore.GetVirtualMemoryOfLoadedObj(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), false), 0);

				break;
			}

		default:
			{
				// all other states require no update
				break;
			}
		}
	}
}

void CScaleformMgr::UpdateMoviesUntilReadyForDeletion()
{
	CScaleformMgr::UpdateAllMoviesStateMachines();
	CScaleformMgr::UpdateAllMoviesStateMachines();
	CScaleformMgr::UpdateAllMoviesStateMachines();
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::ClearReturnValue()
// PURPOSE: clears the slot passed if we know we no longer need it
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::ClearReturnValue(s32 iReturnId)
{
	if (iReturnId != 0)
	{
		for (s32 i = 0; i < MAX_STORED_RETURN_VALUES; i++)
		{
			if (sm_ReturnValuesForAccess[i].iUniqueId == iReturnId)  // found the id
			{
				sm_ReturnValuesForAccess[i].iUniqueId = 0;
				sm_ReturnValuesForAccess[i].gfxvalue_return_value.SetUndefined();
				sm_ReturnValuesForAccess[i].stringData.Clear();
				sm_ReturnValuesForAccess[i].iFramesToStayValid = 0;

				return true;
			}
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsReturnValueSet()
// PURPOSE: returns whether the return value is set yet or not
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsReturnValueSet(s32 iReturnId)
{
	if (iReturnId != 0)
	{
		for (s32 i = 0; i < MAX_STORED_RETURN_VALUES; i++)
		{
			if (sm_ReturnValuesForAccess[i].iUniqueId == iReturnId)  // found the id
			{
				if (!sm_ReturnValuesForAccess[i].gfxvalue_return_value.IsUndefined())
				{
					sfDebugf3("ScaleformMgr: Returned value from slot %d has been returned to code/script as TRUE", iReturnId);

					return true;
				}
				else
				{
					sfDebugf3("ScaleformMgr: Returned value from slot %d is not ready yet (frames left %d)", iReturnId, sm_ReturnValuesForAccess[i].iFramesToStayValid);

					return false;
				}
			}
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetReturnValueInt()
// PURPOSE: returns the actual return value from a unique slot ID
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::GetReturnValueInt(s32 iReturnId)
{
	bool bFoundUniqueId = false;

	s32 iReturnValue = -1;

	GFxValue return_value;
	return_value.SetUndefined();

	for (s32 i = 0; i < MAX_STORED_RETURN_VALUES; i++)
	{
		if (sm_ReturnValuesForAccess[i].iUniqueId == iReturnId)  // found the id
		{
			bFoundUniqueId = true;

			return_value = sm_ReturnValuesForAccess[i].gfxvalue_return_value;

			sm_ReturnValuesForAccess[i].iUniqueId = 0;
			sm_ReturnValuesForAccess[i].gfxvalue_return_value.SetUndefined();
			sm_ReturnValuesForAccess[i].stringData.Clear();
			sm_ReturnValuesForAccess[i].iFramesToStayValid = 0;

			sfDebugf3("ScaleformMgr: Returned value from slot %d (took %d frames)", iReturnId, FRAMES_FOR_RETURNED_VALUE_TO_STAY_VALID - sm_ReturnValuesForAccess[i].iFramesToStayValid);

			break;
		}
	}

	if (bFoundUniqueId)
	{
		if (return_value.IsNumber())
		{
			iReturnValue = (s32)return_value.GetNumber();
		}
#if __ASSERT
		else
		{
			sfAssertf(0, "Return ID slot %d does not return a INT", iReturnId);
		}
#endif // __ASSERT
	}
	else
	{
		sfAssertf(0, "Return ID slot %d doesnt exist!", iReturnId);
	}

	return (iReturnValue);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetReturnValueFloat()
// PURPOSE: returns the actual return value from a unique slot ID
/////////////////////////////////////////////////////////////////////////////////////
float CScaleformMgr::GetReturnValueFloat(s32 iReturnId)
{
	bool bFoundUniqueId = false;

	float fReturnValue = -1.0f;

	GFxValue return_value;
	return_value.SetUndefined();

	for (s32 i = 0; i < MAX_STORED_RETURN_VALUES; i++)
	{
		if (sm_ReturnValuesForAccess[i].iUniqueId == iReturnId)  // found the id
		{
			bFoundUniqueId = true;

			return_value = sm_ReturnValuesForAccess[i].gfxvalue_return_value;

			sm_ReturnValuesForAccess[i].iUniqueId = 0;
			sm_ReturnValuesForAccess[i].gfxvalue_return_value.SetUndefined();
			sm_ReturnValuesForAccess[i].stringData.Clear();
			sm_ReturnValuesForAccess[i].iFramesToStayValid = 0;

			sfDebugf3("ScaleformMgr: Returned value from slot %d", iReturnId);

			break;
		}
	}

	if (bFoundUniqueId)
	{
		if (return_value.IsNumber())
		{
			fReturnValue = (float)return_value.GetNumber();
		}
#if __ASSERT
		else
		{
			sfAssertf(0, "Return ID slot %d does not return a FLOAT", iReturnId);
		}
#endif // __ASSERT
	}
	else
	{
		sfAssertf(0, "Return ID slot %d doesnt exist!", iReturnId);
	}

	return (fReturnValue);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetReturnValueBool()
// PURPOSE: returns the actual return value from a unique slot ID
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::GetReturnValueBool(s32 iReturnId)
{
	bool bFoundUniqueId = false;

	bool bReturnValue = false;

	GFxValue return_value;
	return_value.SetUndefined();

	for (s32 i = 0; i < MAX_STORED_RETURN_VALUES; i++)
	{
		if (sm_ReturnValuesForAccess[i].iUniqueId == iReturnId)  // found the id
		{
			bFoundUniqueId = true;

			return_value = sm_ReturnValuesForAccess[i].gfxvalue_return_value;

			sm_ReturnValuesForAccess[i].iUniqueId = 0;
			sm_ReturnValuesForAccess[i].gfxvalue_return_value.SetUndefined();
			sm_ReturnValuesForAccess[i].stringData.Clear();
			sm_ReturnValuesForAccess[i].iFramesToStayValid = 0;

			sfDebugf3("ScaleformMgr: Returned value from slot %d", iReturnId);

			break;
		}
	}

	if (bFoundUniqueId)
	{
		if (return_value.IsBool())
		{
			bReturnValue = return_value.GetBool();
		}
#if __ASSERT
		else
		{
			sfAssertf(0, "Return ID slot %d does not return a BOOL", iReturnId);
		}
#endif // __ASSERT
	}
	else
	{
		sfAssertf(0, "Return ID slot %d doesnt exist!", iReturnId);
	}

	return (bReturnValue);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetReturnValueString()
// PURPOSE: returns the actual return value from a unique slot ID
/////////////////////////////////////////////////////////////////////////////////////
const char *CScaleformMgr::GetReturnValueString(s32 iReturnId)
{
	bool bFoundUniqueId = false;

	static char cRetString[64];
	cRetString[0] = '\0';

	GFxValue return_value;
	return_value.SetUndefined();

	for (s32 i = 0; i < MAX_STORED_RETURN_VALUES; i++)
	{
		if (sm_ReturnValuesForAccess[i].iUniqueId == iReturnId)  // found the id
		{
			bFoundUniqueId = true;

			return_value = sm_ReturnValuesForAccess[i].gfxvalue_return_value;

			if (return_value.IsString())
			{
				safecpy(cRetString, return_value.GetString(), NELEM(cRetString));
			}

			sm_ReturnValuesForAccess[i].iUniqueId = 0;
			sm_ReturnValuesForAccess[i].gfxvalue_return_value.SetUndefined();
			sm_ReturnValuesForAccess[i].stringData.Clear();
			sm_ReturnValuesForAccess[i].iFramesToStayValid = 0;

			sfDebugf3("ScaleformMgr: Returned value from slot %d", iReturnId);

			break;
		}
	}

	if (bFoundUniqueId)
	{
#if __ASSERT
		sfAssertf(return_value.IsString(), "Return ID slot %d does not return a STRING", iReturnId);
#endif // __ASSERT
	}
	else
	{
		sfAssertf(0, "Return ID slot %d doesnt exist!", iReturnId);
	}

	return (const char *)&cRetString;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::AddMethodToBuffer()
// PURPOSE: adds the method and its params to the buffer
/////////////////////////////////////////////////////////////////////////////////////
GFxValue CScaleformMgr::AddMethodToBuffer(s32 iIndex, s32 iIndexMovieLinkedTo, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, const GFxValue *pArgs, s32 iNumberOfParams, bool bStoreReturnedValue, COMPLEX_OBJECT_ID ComplexObjectIdToInvoke)
{
	GFxValue result;

	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr:: AddMethodToBuffer can only be called on the UpdateThread!");

		result.SetNumber(-1);

		return result;
	}

	ScaleformMethodStruct *newMethodInfo = &sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse].Grow();

	newMethodInfo->iMovieId = (s8)iIndex;

	newMethodInfo->iBaseClass = iBaseClass;
	safecpy(newMethodInfo->cMethodName, pFunctionName, MAX_CHARS_IN_SCALEFORM_METHOD_NAME);

	newMethodInfo->iParamCount = (u8)iNumberOfParams;

	newMethodInfo->iReturnId = -1;

	for (s32 i = 0; i < iNumberOfParams && i < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD; i++)
	{
		if (pArgs[i].IsString())
		{
			newMethodInfo->params[i].pParamString = rage_new atString(pArgs[i].GetString());
			newMethodInfo->params[i].param_type = PARAM_TYPE_STRING;
		}
		else if (pArgs[i].IsNumber())
		{
			newMethodInfo->params[i].ParamNumber = (Double)pArgs[i].GetNumber();
			newMethodInfo->params[i].param_type = PARAM_TYPE_NUMBER;
		}
		else if (pArgs[i].IsBool())
		{
			newMethodInfo->params[i].bParamBool = pArgs[i].GetBool();
			newMethodInfo->params[i].param_type = PARAM_TYPE_BOOL;
		}
		else if (!pArgs[i].IsUndefined())
		{
			newMethodInfo->params[i].pParamGfxValue = rage_new GFxValue;
			*newMethodInfo->params[i].pParamGfxValue = pArgs[i];
			newMethodInfo->params[i].param_type = PARAM_TYPE_GFXVALUE;
		}
		else
		{
			sfAssertf(0, "ScaleformMgr:: AddMethodToBuffer '%s' has been passed undefined as param %d %d", pFunctionName, i, pArgs[i].GetType());
			newMethodInfo->params[i].pParamGfxValue = rage_new GFxValue;
			newMethodInfo->params[i].param_type = PARAM_TYPE_GFXVALUE;
		}
	}

	newMethodInfo->iMovieIdThisInvokeIsLinkedTo = (s8)iIndexMovieLinkedTo;
	newMethodInfo->ComplexObjectIdToInvoke = ComplexObjectIdToInvoke;

	if ( (iNumberOfParams > 1) && (pArgs[1].IsString()) && (!stricmp("CONTAINER_METHOD", pFunctionName)) )  // if container method call then store the movie id in use as the 1st param
	{
		sfDebugf3("ScaleformMgr: AddMethodToBuffer: %d %d - %s (CONTAINER_METHOD on movie %d using movie %d)", sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse].GetCount(), sm_iMethodInvokeInfoBufferInUse, pArgs[1].GetString(), newMethodInfo->iMovieId, newMethodInfo->iMovieIdThisInvokeIsLinkedTo);
	}
	else
	{
		sfDebugf3("ScaleformMgr: AddMethodToBuffer: %d %d - %s", sm_MethodInvokeInfo[sm_iMethodInvokeInfoBufferInUse].GetCount(), sm_iMethodInvokeInfoBufferInUse, pFunctionName);
	}

	if (bStoreReturnedValue)
	{
		newMethodInfo->iReturnId = ms_iGlobalReturnCounterUT;

		if (ms_iGlobalReturnCounterUT < MAX_UNIQUE_RETURN_COUNTER)
			ms_iGlobalReturnCounterUT++;
		else
			ms_iGlobalReturnCounterUT = 1;

		sfDebugf3("ScaleformMgr: AddMethodToBuffer: Return Slot %d set for method call %s", newMethodInfo->iReturnId, pFunctionName);

		result.SetNumber(newMethodInfo->iReturnId);  // slot id for the return value in the return array
	}

	return result;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::AddCallbackMethodToBuffer()
// PURPOSE: adds the callback method and its params to the buffer so it can be
//			executed on the update thread
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::AddCallbackMethodToBuffer(s32 iMovieCalledFrom, const char *pFunctionName, const GFxValue *pArgs, s32 iNumberOfParams)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr:: AddMethodToBuffer can only be called on the RenderThread!");

		return;
	}

	if (IsMovieUpdateable(iMovieCalledFrom))  // only add the method to the buffer if the movie is active
	{
/*#if __ASSERT
		for (s32 i = 0; i < iNumberOfParams && i < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD; i++)
		{
			if (pArgs[i].IsUndefined())
			{
				sfAssertf(0, "ScaleformMgr:: AddMethodToBuffer '%s' has been passed undefined as param %d %d - ignoring method!", pFunctionName, i, pArgs[i].GetType());
				return;
			}
		}
#endif // __ASSERT*/

		sfDebugf3("ScaleformMgr: AddCallbackMethodToBuffer: %d %d - %s (num params %d)", sm_MethodCallbackInfo[sm_iMethodInvokeInfoBufferInUse].GetCount(), sm_iMethodInvokeInfoBufferInUse, pFunctionName, iNumberOfParams);

		ScaleformMethodStruct *newMethodInfo = &sm_MethodCallbackInfo[sm_iMethodInvokeInfoBufferInUse].Grow();

		safecpy(newMethodInfo->cMethodName, pFunctionName, MAX_CHARS_IN_SCALEFORM_METHOD_NAME);

		Assign(newMethodInfo->iMovieId, iMovieCalledFrom);
		Assign(newMethodInfo->iParamCount, iNumberOfParams);

		for (s32 i = 0; i < iNumberOfParams && i < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD; i++)
		{
			if (pArgs[i].IsString())
			{
				newMethodInfo->params[i].pParamString = rage_new atString(pArgs[i].GetString());
				newMethodInfo->params[i].param_type = PARAM_TYPE_STRING;
			}
			else if (pArgs[i].IsNumber())
			{
				newMethodInfo->params[i].ParamNumber = (Double)pArgs[i].GetNumber();
				newMethodInfo->params[i].param_type = PARAM_TYPE_NUMBER;
			}
			else if (pArgs[i].IsBool())
			{
				newMethodInfo->params[i].bParamBool = pArgs[i].GetBool();
				newMethodInfo->params[i].param_type = PARAM_TYPE_BOOL;
			}
			else if (!pArgs[i].IsUndefined())
			{
				newMethodInfo->params[i].pParamGfxValue = rage_new GFxValue;
				*newMethodInfo->params[i].pParamGfxValue = pArgs[i];
				newMethodInfo->params[i].param_type = PARAM_TYPE_GFXVALUE;
			}
			else
			{
				sfAssertf(0, "ScaleformMgr:: AddCallbackMethodToBuffer '%s' has been passed undefined as param %d %d", pFunctionName, i, pArgs[i].GetType());
				newMethodInfo->params[i].pParamGfxValue = rage_new GFxValue;
				newMethodInfo->params[i].param_type = PARAM_TYPE_GFXVALUE;
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::FindMovieByFilename()
// PURPOSE: takes a filename and returns the movie id if it exists
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::FindMovieByFilename(const char *pFilename, bool findStreamingMovies /* = false */)
{
	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieUpdateable(iIndex) ||
			(findStreamingMovies && (sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_FLAGGED_FOR_USE || sm_ScaleformArray[iIndex].iState == SF_MOVIE_STATE_STREAMING_MOVIE)))
		{
			if (!stricmp(pFilename, sm_ScaleformArray[iIndex].cFilename))
			{
				return iIndex;
			}
		}
	}

	return -1;
}

// Remove any invokes that are queued for the given complex object
void CScaleformMgr::RemoveInvokesForComplexObject(COMPLEX_OBJECT_ID const c_objectId)
{
    if (c_objectId != INVALID_COMPLEX_OBJECT_ID)
    {

#if __SCALEFORM_CRITICAL_SECTIONS
        SYS_CS_SYNC(sm_InvokeFlushToken);
#endif // __SCALEFORM_CRITICAL_SECTIONS

        for (s32 bufferToFlush = 0; bufferToFlush < 2; bufferToFlush++)
        {
            atArray<ScaleformMethodStruct>& currentBuffer = sm_MethodInvokeInfo[bufferToFlush];

            // Backwards iterate, as we remove from the array
            s32 const c_count = currentBuffer.GetCount();
            for( s32 index = c_count - 1; index >= 0; --index )
            {
                ScaleformMethodStruct& currentMethodStruct = currentBuffer[index];
                if( c_objectId == currentMethodStruct.ComplexObjectIdToInvoke)
                {
                    // It may be safer just to invalidate the object so the invoke is ignored?
                    currentBuffer.Delete( index );
                }
            }
        }

#if !__NO_OUTPUT
        sfDebugf3( "ScaleformMgr: Finished removing pending invokes for ComplexObject %d", c_objectId );
#endif // #if !__NO_OUTPUT
    }
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::FlushInvokeList()
// PURPOSE: Invokes all the Scaleform Methods that were called on the Update Thread
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::FlushInvokeList()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr:: FlushInvokeList can only be called on the RenderThread!");
		return;
	}

#if __SCALEFORM_CRITICAL_SECTIONS
	SYS_CS_SYNC(sm_InvokeFlushToken);
#endif // __SCALEFORM_CRITICAL_SECTIONS

	s32 const c_bufferToFlush = sm_iMethodInvokeInfoBufferInUse == 0 ? 1 : 0;

	if (sm_MethodInvokeInfo[c_bufferToFlush].GetCount() == 0)
	{
		return;  // buffer empty, nothing to flush!
	}

	sfDebugf3("ScaleformMgr: Flushing %d buffered Scaleform methods (buffer %d)...", sm_MethodInvokeInfo[c_bufferToFlush].GetCount(), c_bufferToFlush);

	for (s32 item = 0; item < sm_MethodInvokeInfo[c_bufferToFlush].GetCount(); item++)
	{
		ProcessIndividualInvoke(c_bufferToFlush, item);
	}

	// delete any gfx values we have created here:
	for (s32 iCount = 0; iCount < sm_MethodInvokeInfo[c_bufferToFlush].GetCount(); iCount++)
	{
		if (sfVerifyf(sm_MethodInvokeInfo[c_bufferToFlush][iCount].iMovieId != -1, "ScaleformMgr: No Scaleform method in the buffer to flush!"))
		{
			for (s32 i = 0; i < sm_MethodInvokeInfo[c_bufferToFlush][iCount].iParamCount; i++)
			{
				if (sm_MethodInvokeInfo[c_bufferToFlush][iCount].params[i].param_type == PARAM_TYPE_STRING)
				{
					if (sm_MethodInvokeInfo[c_bufferToFlush][iCount].params[i].pParamString)
					{
						sm_MethodInvokeInfo[c_bufferToFlush][iCount].params[i].pParamString->Clear();
						delete (sm_MethodInvokeInfo[c_bufferToFlush][iCount].params[i].pParamString);
						sm_MethodInvokeInfo[c_bufferToFlush][iCount].params[i].pParamString = NULL;
					}
				}

				if (sm_MethodInvokeInfo[c_bufferToFlush][iCount].params[i].param_type == PARAM_TYPE_GFXVALUE)
				{
					delete (sm_MethodInvokeInfo[c_bufferToFlush][iCount].params[i].pParamGfxValue);
					sm_MethodInvokeInfo[c_bufferToFlush][iCount].params[i].pParamGfxValue = NULL;
				}
			}
		}
	}

	sm_MethodInvokeInfo[c_bufferToFlush].Reset();

	sfDebugf3("ScaleformMgr: Finished Scaleform method buffer flush");
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::ProcessIndividualInvoke()
// PURPOSE: actually processes the past item from the invoke list
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::ProcessIndividualInvoke(s32 const c_buffer, s32 const c_index)
{
	if (sm_MethodInvokeInfo[c_buffer][c_index].cMethodName[0] == '\0')  // method is invalid
	{
		return false;
	}

	const s32 c_movieId = sm_MethodInvokeInfo[c_buffer][c_index].iMovieId;

	if (sfVerifyf(IsMovieIdInRange(c_movieId), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", c_movieId))
	{
		GFxValue params[MAX_NUM_PARAMS_IN_SCALEFORM_METHOD];

		for (s32 i = 0; i < sm_MethodInvokeInfo[c_buffer][c_index].iParamCount; i++)
		{
			switch (sm_MethodInvokeInfo[c_buffer][c_index].params[i].param_type)
			{
				case PARAM_TYPE_NUMBER:
				{
					params[i].SetNumber(sm_MethodInvokeInfo[c_buffer][c_index].params[i].ParamNumber);
					break;
				}
				case PARAM_TYPE_BOOL:
				{
					params[i].SetBoolean(sm_MethodInvokeInfo[c_buffer][c_index].params[i].bParamBool);
					break;
				}
				case PARAM_TYPE_STRING:
				{
					if (sm_MethodInvokeInfo[c_buffer][c_index].params[i].pParamString)
					{
						params[i].SetString(sm_MethodInvokeInfo[c_buffer][c_index].params[i].pParamString->c_str());
					}
					else
					{
						params[i].SetString("\0");
					}
					break;
				}
				case PARAM_TYPE_GFXVALUE:
				{
					params[i] = *sm_MethodInvokeInfo[c_buffer][c_index].params[i].pParamGfxValue;
					break;
				}
				default:
				{
					sfAssertf(0, "ScaleformMgr: Invalid param type (%d)", (s32)sm_MethodInvokeInfo[c_buffer][c_index].params[i].param_type);
				}
			}
		}

		CallMethodInternal(	c_movieId,
							sm_MethodInvokeInfo[c_buffer][c_index].iMovieIdThisInvokeIsLinkedTo,
							sm_MethodInvokeInfo[c_buffer][c_index].iBaseClass,
							sm_MethodInvokeInfo[c_buffer][c_index].cMethodName,
							params,
							sm_MethodInvokeInfo[c_buffer][c_index].iParamCount,
							true,
							false,
							sm_MethodInvokeInfo[c_buffer][c_index].iReturnId,
							sm_MethodInvokeInfo[c_buffer][c_index].ComplexObjectIdToInvoke);
	}

	sm_MethodInvokeInfo[c_buffer][c_index].cMethodName[0] = '\0';  // invalidate the method

	return true;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::FlushCallbackList()
// PURPOSE: flushes the callback list on the UT
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::FlushCallbackList()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr:: FlushCallbackList can only be called on the UpdateThread!");
		return;
	}

	s32 iBufferToFlush;

	if (sm_iMethodInvokeInfoBufferInUse == 0)
		iBufferToFlush = 1;
	else
		iBufferToFlush = 0;

	if (sm_MethodCallbackInfo[iBufferToFlush].GetCount() == 0)
	{
		//sfDebugf3("ScaleformMgr: Trying to Flush %d but no methods!", iBufferToFlush);

		return;  // buffer empty, nothing to flush!
	}

	sfDebugf3("ScaleformMgr: Flushing %d buffered Scaleform callback methods (buffer %d)...", sm_MethodCallbackInfo[iBufferToFlush].GetCount(), iBufferToFlush);

	GFxValue params[MAX_NUM_PARAMS_IN_SCALEFORM_METHOD];

	for (s32 iCount = 0; iCount < sm_MethodCallbackInfo[iBufferToFlush].GetCount(); iCount++)
	{
		if (IsMovieUpdateable(sm_MethodCallbackInfo[iBufferToFlush][iCount].iMovieId))  // only execute callbacks where the movie has some sort of state
		{
			for (s32 i = 0; i < sm_MethodCallbackInfo[iBufferToFlush][iCount].iParamCount; i++)
			{
				switch (sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].param_type)
				{
					case PARAM_TYPE_NUMBER:
					{
						params[i].SetNumber(sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].ParamNumber);
						break;
					}
					case PARAM_TYPE_BOOL:
					{
						params[i].SetBoolean(sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].bParamBool);
						break;
					}
					case PARAM_TYPE_STRING:
					{
						if (sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamString)
						{
							params[i].SetString(sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamString->c_str());
						}
						else
						{
							params[i].SetString("\0");
						}

						break;
					}
					case PARAM_TYPE_GFXVALUE:
					{
						params[i] = *sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamGfxValue;
						break;
					}
					default:
					{
						sfAssertf(0, "ScaleformMgr: Invalid param type (%d)", (s32)sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].param_type);
					}
				}
			}

			ExecuteCallbackMethod(sm_MethodCallbackInfo[iBufferToFlush][iCount].iMovieId, sm_MethodCallbackInfo[iBufferToFlush][iCount].cMethodName, params, sm_MethodCallbackInfo[iBufferToFlush][iCount].iParamCount);
		}
	}

	// delete any gfx values we have created here:
	for (s32 iCount = 0; iCount < sm_MethodCallbackInfo[iBufferToFlush].GetCount(); iCount++)
	{
		for (s32 i = 0; i < sm_MethodCallbackInfo[iBufferToFlush][iCount].iParamCount; i++)
		{
			if (sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].param_type == PARAM_TYPE_STRING)
			{
				if (sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamString)
				{
					sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamString->Clear();
					delete (sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamString);
					sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamString = NULL;
				}
			}

			if (sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].param_type == PARAM_TYPE_GFXVALUE)
			{
				delete (sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamGfxValue);
				sm_MethodCallbackInfo[iBufferToFlush][iCount].params[i].pParamGfxValue = NULL;
			}
		}
	}

	sm_MethodCallbackInfo[iBufferToFlush].Reset();

	sfDebugf3("ScaleformMgr: Finished Scaleform callback method buffer flush");
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::InitialiseAndSetActive()
// PURPOSE: sets the movie as active and sets some params for it
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::InitialiseAndSetActive(s32 iIndex)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::InitialiseAndSetActive can only be called on the UpdateThread!");
		return false;
	}

	if (sfVerifyf((g_ScaleformStore.HasObjectLoaded(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)) && g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId))), "ScaleformMgr: Movie id %d is not ready but is trying to be set active", iIndex))
	{
		if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
		{
			g_ScaleformStore.AddRef(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId), REF_OTHER);

			ChangeMovieParams(iIndex, sm_ScaleformArray[iIndex].vPos.GetUpdateBuf(), sm_ScaleformArray[iIndex].vSize.GetUpdateBuf(), sm_ScaleformArray[iIndex].vWorldSize.GetUpdateBuf(), Vector3(ORIGIN), sm_ScaleformArray[iIndex].eScaleMode.GetUpdateBuf());

			GFxMovieView *pMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

			if (pMovieView)
			{
				pMovieView->SetBackgroundAlpha(0);
			}

			sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_ACTIVE;

	#if __BANK
			CScaleformMgr::DebugTurnOnOffOutput(iIndex);  // turn on debug output for this movie if required
	#endif

			return true;
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::UpdateMovie()
// PURPOSE: Updates a movie with the supplied timestep - if no timestep is supplied
//			we update using the framerate of the supplied movie (default)
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::UpdateMovie(s32 iIndex, float fTimeStep)
{
//	PF_AUTO_PUSH_TIMEBAR("UpdateMovie");

#if RSG_PC
	if (STextInputBox::IsInstantiated() && STextInputBox::GetInstance().IsActive())
	{
		int iMovieID = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).GetMovieID();
		if(iMovieID == iIndex)
		{
			return;
		}
	}
#endif // RSG_PC

	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		sm_ScaleformArray[iIndex].bNeedsToUpdateThisFrame = false;

		g_CurrentMovie = &sm_ScaleformArray[iIndex];

		CScaleformMovieObject* pMovieObject = g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

		if (!pMovieObject)
		{
			sfAssertf(0, "ScaleformMgr: store %d invalid! (%d not loaded!)", sm_ScaleformArray[iIndex].iMovieId, iIndex);
			return;
		}

		sfScaleformMovieView *pRawMovieView = pMovieObject->m_MovieView;
		if (pRawMovieView)
		{
#if __WIN32PC
			if(GRCDEVICE.GetHasFocus())
#endif
			{
				PF_START_TIMEBAR_DETAIL(sm_ScaleformArray[iIndex].cFilename);
			}
			DIAG_CONTEXT_MESSAGE("Updating Flash Movie %s", sm_ScaleformArray[iIndex].cFilename);

			sfScaleformManager::AutoSetCurrMovieName currMovie(sm_ScaleformArray[iIndex].cFilename);

			if (fTimeStep == 0.0f) // If no time step is specified
			{
				if (PARAM_lockscaleformtogameframe.Get())
				{
					// use the current movie frame rate (i.e. 1 game frame = 1 movie frame)
					fTimeStep = (1.0f / pRawMovieView->GetMovieView().GetFrameRate());
				}
				else
				{
					// use a generic timer
					fTimeStep = fwTimer::GetTimeStep();
				}
			}

			if (!IsMovieActive(iIndex))
			{
				sfDebugf3("Updating Scaleform Movie when not fully active: %s (%d) - timestep %0.2f - State: %d", sm_ScaleformArray[iIndex].cFilename, iIndex, fTimeStep, (s32)sm_ScaleformArray[iIndex].iState);
			}
#if RSG_PC
			// The warning/display calibration always receives all input (mouse/keyboard) when it is active
			bool bAlwaysReceivesInput = (iIndex == CWarningScreen::GetMovieWrapper().GetMovieID() || iIndex == CDisplayCalibration::GetMovieID());
			bool bIsBlocked = IsFullScreenScaleformMovieBlockingInput();

			if( bAlwaysReceivesInput || !bIsBlocked )
			{
				// unfortunately, the way this is set up doesn't lend itself to overriding/inheritance, so we gotta do this.
				GetMovieMgr()->GetInput().SetIgnoreBlocksForNextSend(bAlwaysReceivesInput);
				pRawMovieView->ApplyInput(GetMovieMgr()->GetInput());
			}
			else if(!ms_bfullScreenBlockedInputLastFrame)
			{
				GetMovieMgr()->GetInput().SetForceReleaseForNextSend();
				pRawMovieView->ApplyInput(GetMovieMgr()->GetInput());
			}
			// other movies are blocked by mapping and warning screen
#endif

			pRawMovieView->Update(fTimeStep);

#if __SF_STATS_WITHOUT_AMP
			// If we're in the loading screen, we need to reset stats here since PerformSafeModeOperations isn't getting called
			if (!gRenderThreadInterface.IsUsingDefaultRenderFunction())
			{
				pRawMovieView->ResetStats();
			}
#endif
		}

		g_CurrentMovie = NULL;
	}
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::ClearScaleformArraySlot()
// PURPOSE: fully clears all contents of 1 slot in the sm_ScaleformArray array
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::ClearScaleformArraySlot(s32 iIndex)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::ClearScaleformArraySlot can only be called on the UpdateThread!");
		return;
	}

	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		// ensure both buffers are cleared
		sm_ScaleformArray[iIndex].vPos.Set(ORIGIN);
		sm_ScaleformArray[iIndex].vRot.Set(ORIGIN);
		sm_ScaleformArray[iIndex].vSize.Set(Vector2(0,0));
		sm_ScaleformArray[iIndex].vWorldSize.Set(Vector2(0,0));
		sm_ScaleformArray[iIndex].bPerformGarbageCollection.Set(false);
		sm_ScaleformArray[iIndex].bChangeMovieParams.Set(false);
		sm_ScaleformArray[iIndex].eScaleMode.Set(GFxMovieView::SM_ExactFit);
		sm_ScaleformArray[iIndex].bRender3DSolid.Set(false);
		sm_ScaleformArray[iIndex].bRender3D.Set(false);
		sm_ScaleformArray[iIndex].bUpdateMovie = false;
		sm_ScaleformArray[iIndex].iRendered = 0;
		sm_ScaleformArray[iIndex].movieOwnerTag = SF_MOVIE_TAGGED_BY_NONE;
		sm_ScaleformArray[iIndex].iDependentMovie = -1;
		sm_ScaleformArray[iIndex].bNeedsToUpdateThisFrame = false;
		sm_ScaleformArray[iIndex].iState = SF_MOVIE_STATE_INACTIVE;

		sm_ScaleformArray[iIndex].iMovieId = -1;
		sm_ScaleformArray[iIndex].cFilename[0] = '\0';
		sm_ScaleformArray[iIndex].bRemovableAtInit = true;
		sm_ScaleformArray[iIndex].fCurrentTimestep = 0.0f;

		sm_ScaleformArray[iIndex].iBrightness.Set(0);
		sm_ScaleformArray[iIndex].iParentMovie = -1;
	#if __DEV
		sm_ScaleformArray[iIndex].iNumRefs = 0;
	#endif //__DEV

		sm_ScaleformArray[iIndex].requestedTxds.Reset();
#if __SF_STATS
		sm_ScaleformArray[iIndex].allRequestedTxds.Reset();
#endif
		sm_ScaleformArray[iIndex].bNeedsToUpdateThisFrame = false;

		sm_ScaleformArray[iIndex].bDontRenderWhilePaused = false;
		sm_ScaleformArray[iIndex].bForceRemoval = false;
	}
}

void CScaleformMgr::UpdateMovieParams(s32 iIndex)
{
	ChangeMovieParams(iIndex, sm_ScaleformArray[iIndex].vOriginalPos.GetUpdateBuf(), sm_ScaleformArray[iIndex].vOriginalSize.GetUpdateBuf(), sm_ScaleformArray[iIndex].vWorldSize.GetUpdateBuf(), Vector3(ORIGIN), sm_ScaleformArray[iIndex].eScaleMode.GetUpdateBuf());
}

void CScaleformMgr::UpdateMovieParams(s32 iIndex, GFxMovieView::ScaleModeType eScaleMode)
{
	ChangeMovieParams(iIndex, sm_ScaleformArray[iIndex].vOriginalPos.GetUpdateBuf(), sm_ScaleformArray[iIndex].vOriginalSize.GetUpdateBuf(), sm_ScaleformArray[iIndex].vWorldSize.GetUpdateBuf(), Vector3(ORIGIN), eScaleMode);
}

void CScaleformMgr::UpdateMovieParams(s32 iIndex, const Vector2& vPos, const Vector2& vSize)
{
	ChangeMovieParams(iIndex, Vector3(vPos.x, vPos.y, 0.0f), vSize, sm_ScaleformArray[iIndex].vWorldSize.GetUpdateBuf(), Vector3(ORIGIN), sm_ScaleformArray[iIndex].eScaleMode.GetUpdateBuf());
}

void CScaleformMgr::ChangeMovieParams(s32 iIndex,  const Vector2& vPos, const Vector2& vSize, GFxMovieView::ScaleModeType eScaleMode, int CurrentRenderID, bool bOverrideSWScaling, u8 iLargeRt)
{
	ChangeMovieParams(iIndex, Vector3(vPos.x, vPos.y, 0.0f), vSize, Vector2(0.0f, 0.0f), Vector3(ORIGIN), eScaleMode, CurrentRenderID, bOverrideSWScaling, iLargeRt);
}

void CScaleformMgr::ScalePosAndSize(Vector2& vPos, Vector2& vSize, float fScalar)
{
	Vector2 adjPosOffset(Vector2(0.5f, 0.5f)-vPos);
	vPos.SubtractScaled(adjPosOffset, fScalar-1.0f);
	vSize *= fScalar;
}

void CScaleformMgr::AffectRenderSizeOnly(s32 iIndex, float fScalar)
{
	if (!sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
		return;

	ScaleformMovieStruct& thisStruct = sm_ScaleformArray[iIndex];
	if( !sfVerifyf(thisStruct.iMovieId != -1, "ScaleformMgr: Movie %d doesn't exist", thisStruct.iMovieId ))
		return;

	if(!sfVerifyf(g_ScaleformStore.Get(strLocalIndex(thisStruct.iMovieId)), "ScaleformMgr: store %d invalid! (%d not loaded!)", thisStruct.iMovieId, iIndex))
		return;

	if(!sfVerifyf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "ScaleformMgr: AffectRenderSizeOnly can only run on the render thread!"))
		return;


	GFxMovieView *pMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(thisStruct.iMovieId));

	if (pMovieView)
	{

		// movies' registration points are the top left
		// but we want to scale them from the center of the screen
		// so take their position and subtract back out along the ray from the screen center to their position
		Vector2 adjPos( thisStruct.vPos.GetRenderBuf().x, thisStruct.vPos.GetRenderBuf().y );
		Vector2 adjSize(thisStruct.vSize.GetRenderBuf() );
		ScalePosAndSize(adjPos, adjSize, fScalar);

#if SUPPORT_MULTI_MONITOR
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		const Vector2 offset((float)mon.uLeft, (float)mon.uTop);
		const Vector2 size((float)mon.getWidth(), (float)mon.getHeight());
		adjPos.Multiply(size);
		adjPos.Add(offset);
		adjSize.Multiply(size);
#else
		adjPos.Multiply(ms_vScreenSize);
		adjSize.Multiply(ms_vScreenSize);
#endif //SUPPORT_MULTI_MONITOR


		GViewport viewportDesc(
			  rage::round(ms_vScreenSize.x)
			, rage::round(ms_vScreenSize.y)
			, rage::round(adjPos.x)
			, rage::round(adjPos.y)
			, rage::round(adjSize.x)
			, rage::round(adjSize.y)
			);
#if __CONSOLE
		viewportDesc.AspectRatio = CHudTools::GetAspectRatioMultiplier();
#endif //__CONSOLE
		pMovieView->SetViewport(viewportDesc);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::ChangeMovieParams()
// PURPOSE: changes any params (pos, size etc) of the movie
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::ChangeMovieParams(s32 iIndex, const Vector3& vPos, const Vector2& vScale, const Vector2& vWorldSize, const Vector3& vRot, GFxMovieView::ScaleModeType eScaleMode, int CurrentRenderID, bool bOverrideSWScaling, u8 iLargeRt)
{
	if (!sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
		return;

	ScaleformMovieStruct &movie = sm_ScaleformArray[iIndex];

	if( !sfVerifyf(movie.iMovieId != -1, "ScaleformMgr: Movie %d doesn't exist", movie.iMovieId ))
		return;

#if __BANK && !__NO_OUTPUT
	if(PARAM_logExtendedScaleformOutput.Get())
	{
		sfDebugf3("CScaleformMgr::ChangeMovieParams: ");
		sfDebugf3("								-- iIndex = %d", iIndex);
		sfDebugf3("								-- vPos = %.2f, %.2f", vPos.x, vPos.y);
		sfDebugf3("								-- vScale = %.2f, %.2f", vScale.x, vScale.y);
		sfDebugf3("								-- eScaleMode = %d", eScaleMode);
	}
#endif // !__NO_OUTPUT

	if (bOverrideSWScaling)
	{
		movie.bIgnoreSuperWidescreenScaling.Set(true);
	}
	if( sfVerifyf(HasMovieGotAnyState(iIndex), "Attempted to change movie params of an empty index %d!", iIndex) )
	{
		if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		{
			Vector3 vFinalPos = vPos;
			Vector2 vFinalSize = vScale;
			
			if(!movie.bIgnoreSuperWidescreenScaling.GetRenderBuf() && CHudTools::IsSuperWideScreen())
			{
				Vector2 vAdjustedPos(vFinalPos.x, vFinalPos.y);
				CHudTools::AdjustForSuperWidescreen(&vAdjustedPos, &vFinalSize);
				vFinalPos.x = vAdjustedPos.x;
				vFinalPos.y = vAdjustedPos.y;
			}

			// store the 0-1 size:
			movie.vPos.Set(vFinalPos);
			movie.vRot.Set(vRot);
			movie.vSize.Set(vFinalSize);
			movie.vWorldSize.Set(vWorldSize);
			movie.eScaleMode.Set(eScaleMode);

			GFxMovieView *pMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(movie.iMovieId));

			if (pMovieView)
			{
				float total_w = 0.0;
				float total_h = 0.0;
				float vw = 0.0;
				float vh = 0.0;
				float ox = 0.0;
				float oy = 0.0;
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
				// check for fixed size RTs
				unsigned uID = (unsigned)CurrentRenderID;
				bool bFixedSizeRT = (CurrentRenderID != -1 && (uID >= CRenderTargetMgr::RTI_Count || uID == CRenderTargetMgr::RTI_Scripted));
				if (bFixedSizeRT)
				{
					// unsure why this is not 1280 x 720
					// Me neither, but in some case, it needs to be.
					if( iLargeRt == 2 )
					{
						total_w = vw = 2048.0f;
						total_h = vh = 1024.0f;
					}
					else if(iLargeRt == 1)
					{
						total_w = vw = 1280.0f;
						total_h = vh = 720.0f;
					}
					else
					{
						total_w = vw = 640.0f;
						total_h = vh = 360.0f;
					}
				}
				else
#endif	// RSG_PC || RSG_DURANGO || RSG_ORBIS
				{
					total_w = vw = ms_vScreenSize.x;
					total_h = vh = ms_vScreenSize.y;
				}

#if SUPPORT_MULTI_MONITOR
				if (!bFixedSizeRT && total_w >= SCREEN_WIDTH && total_w < SCREEN_WIDTH + s_FullScreenMargin &&
					total_h >= SCREEN_HEIGHT && total_h < SCREEN_HEIGHT + s_FullScreenMargin)
				{
					(void)CurrentRenderID;
					const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
					vw = (float)mon.getWidth();
					vh = (float)mon.getHeight();
					ox = (float)mon.uLeft;
					oy = (float)mon.uTop;
				}
#endif //SUPPORT_MULTI_MONITOR

				GViewport viewportDesc((s32)total_w, (s32)total_h,
					(s32)(movie.vPos.GetRenderBuf().x*vw + ox), (s32)(movie.vPos.GetRenderBuf().y*vh + oy),
					(s32)(movie.vSize.GetRenderBuf().x*vw), (s32)(movie.vSize.GetRenderBuf().y*vh));

#if __CONSOLE
				viewportDesc.AspectRatio = CHudTools::GetAspectRatioMultiplier();
#endif //__CONSOLE
				pMovieView->SetViewport(viewportDesc);

				if (pMovieView->GetViewScaleMode() != movie.eScaleMode.GetRenderBuf())
				{
					pMovieView->SetViewScaleMode(movie.eScaleMode.GetRenderBuf());
				}
			}
		}
		else
		{
			Vector3 vFinalPos = vPos;
			Vector2 vFinalSize = vScale;

			if(!movie.bIgnoreSuperWidescreenScaling.GetUpdateBuf() && CHudTools::IsSuperWideScreen())
			{
				Vector2 vAdjustedPos(vFinalPos.x, vFinalPos.y);
				CHudTools::AdjustForSuperWidescreen(&vAdjustedPos, &vFinalSize);
				vFinalPos.x = vAdjustedPos.x;
				vFinalPos.y = vAdjustedPos.y;
			}

			// store the 0-1 size:
			movie.vPos.Set(vFinalPos);
			movie.vRot.Set(vRot);
			movie.vSize.Set(vFinalSize);
			movie.vWorldSize.Set(vWorldSize);
			movie.bChangeMovieParams.Set(true);
			movie.eScaleMode.Set(eScaleMode);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetMovieNumRefs()
// PURPOSE: returns the number of refs this movie has
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::GetMovieNumRefs(s32 iId)
{
	if (!sfVerifyf(IsMovieIdInRange(iId), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iId))
		return 0;

	return (g_ScaleformStore.GetNumberOfRefs(strLocalIndex(sm_ScaleformArray[iId].iMovieId))-1);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetMovieView()
// PURPOSE: returns the movie view
/////////////////////////////////////////////////////////////////////////////////////
GFxMovieView *CScaleformMgr::GetMovieView(s32 iId)
{
	if (!sfVerifyf(IsMovieIdInRange(iId), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iId))
		return NULL;

	if (!IsMovieActive(iId))
		return NULL;

	if (!g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iId].iMovieId)))
	{
		sfAssertf(g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iId].iMovieId)), "ScaleformMgr: store %d invalid! (%d not loaded!)", sm_ScaleformArray[iId].iMovieId, iId);
		return NULL;
	}

	return (g_ScaleformStore.GetMovieView(strLocalIndex(sm_ScaleformArray[iId].iMovieId)));
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetActionScriptObjectFromRoot()
// PURPOSE: returns the root of the movie as a GFxValue
/////////////////////////////////////////////////////////////////////////////////////
GFxValue CScaleformMgr::GetActionScriptObjectFromRoot(s32 iId)
{
	GFxValue asObject;

	if(iId!=-1)
	{
		GFxMovieView *pMovieView = GetMovieView(iId);
		if (pMovieView)
			pMovieView->GetVariable(&asObject, "_root");
	}

	return asObject;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsFunctionAvailable()
// PURPOSE: returns whether this function is available in the passed base class
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsFunctionAvailable(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName)
{
	if (!sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
		return false;

	if (sm_ScaleformArray[iIndex].iMovieId == -1)
		return false;

	char cFullFunctionName[255];

	//
	// different base classes used:
	//
	switch (iBaseClass)
	{
		case SF_BASE_CLASS_HUD:
		{
			strcpy(cFullFunctionName, "HUD.");
			break;
		}
		case SF_BASE_CLASS_MINIMAP:
		case SF_BASE_CLASS_SCRIPT:
		case SF_BASE_CLASS_WEB:
		case SF_BASE_CLASS_CUTSCENE:
		case SF_BASE_CLASS_PAUSEMENU:
		case SF_BASE_CLASS_GENERIC:
        case SF_BASE_CLASS_STORE:
        case SF_BASE_CLASS_GAMESTREAM:
		case SF_BASE_CLASS_MOUSE:
		{
			strcpy(cFullFunctionName, "TIMELINE.");
			break;
		}
		default:
		{
			sfAssertf(0, "ScaleformMgr: Invalid baseclass passed into IsFunctionAvailable");
			strcpy(cFullFunctionName, "TIMELINE.");
			break;
		}
	}

	safecat(cFullFunctionName, pFunctionName);

	GFxMovieView *pMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

	if (pMovieView)
		return (pMovieView->IsAvailable(cFullFunctionName));
	else
		return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CallMethodInternal()
// PURPOSE: generates the full function name from the base class and invokes the
//			actionscript method
/////////////////////////////////////////////////////////////////////////////////////
GFxValue CScaleformMgr::CallMethodInternal(s32 iIndex, s32 iIndexMovieLinkedTo, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, const GFxValue *pArgs, s32 iNumberOfParams, bool bCallInstantly, bool bStoreReturnValue, s32 iReturnId, COMPLEX_OBJECT_ID ComplexObjectIdToInvoke)
{
	RAGE_TIMEBARS_ONLY(pfAutoMarker mkr1("CScaleformMgr::CallMethodInternal", 10));

	GFxValue result;

	result.SetUndefined();

	if(iIndexMovieLinkedTo >= MAX_SCALEFORM_MOVIES)
	{
		return result;
	}

	if (!bCallInstantly)
	{
		if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		{
			result = AddMethodToBuffer(	iIndex,
										iIndexMovieLinkedTo,
										iBaseClass,
										pFunctionName,
										pArgs,
										iNumberOfParams,
										bStoreReturnValue,
										ComplexObjectIdToInvoke);
			return result;
		}
	}

	char cFullFunctionName[255];

	if (ComplexObjectIdToInvoke == INVALID_COMPLEX_OBJECT_ID)
	{
		//
		// different base classes used:
		//
		switch (iBaseClass)
		{
			case SF_BASE_CLASS_HUD:
			{
				strcpy(cFullFunctionName, "HUD.");
				break;
			}
			case SF_BASE_CLASS_MINIMAP:
			case SF_BASE_CLASS_SCRIPT:
			case SF_BASE_CLASS_WEB:
			case SF_BASE_CLASS_CUTSCENE:
			case SF_BASE_CLASS_PAUSEMENU:
			case SF_BASE_CLASS_GENERIC:
			case SF_BASE_CLASS_STORE:
			case SF_BASE_CLASS_GAMESTREAM:
			case SF_BASE_CLASS_VIDEO_EDITOR:
			case SF_BASE_CLASS_MOUSE:
			{
				strcpy(cFullFunctionName, "TIMELINE.");
				break;
			}
			default:
			{
	#if __ASSERT
				Displayf("Bad function data:");
				Displayf(" iIndex = %d", iIndex);
				Displayf(" iBaseClass = %d", (s32)iBaseClass);
				Displayf(" pFunctionName = %s", pFunctionName);
				Displayf(" iNumberOfParams = %d", iNumberOfParams);
				sfAssertf(0, "ScaleformMgr: Invalid baseclass passed into CallFunction: (%d)", (s32)iBaseClass);
	#endif // __ASSERT

				strcpy(cFullFunctionName, "TIMELINE.");  // try and use timeline since most uses it anyway
				break;

				break;
			}
		}

		safecat(cFullFunctionName, pFunctionName);
	}
	else
	{
		safecpy(cFullFunctionName, pFunctionName);  // just the name needed on complex object invokes
	}

	//
	// ensure movie is valid:
	//
	if ( (!IsMovieIdInRange(iIndex)) || (sm_ScaleformArray[iIndex].iMovieId == -1) )
	{
		sfAssertf(0, "ScaleformMgr: '%s' method called but sm_ScaleformArray slot %d no longer exists!", cFullFunctionName, iIndex);

		// if movie isnt valid anymore then return FALSE as the GfxValue and assert:
		result.SetBoolean(false);

		return result;
	}

	if (sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_ACTIVE &&
		sm_ScaleformArray[iIndex].iState != SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE)
	{
		sfDebugf3("ScaleformMgr: %s called on %s (%d) but not invoked as movie is not currently active", cFullFunctionName, sm_ScaleformArray[iIndex].cFilename, iIndex);

		// if movie isnt valid anymore then return FALSE as the GfxValue and assert:
		result.SetBoolean(false);

		return result;
	}

	if (!g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId)))
	{
		sfAssertf(0, "ScaleformMgr: store %d invalid! (%d not loaded!)", sm_ScaleformArray[iIndex].iMovieId, iIndex);

		result.SetBoolean(false);

		return result;
	}

#if __SCALEFORM_CRITICAL_SECTIONS
	SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
#endif // __SCALEFORM_CRITICAL_SECTIONS

	GFxMovieView *pMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

	if (!pMovieView)  // dont allow methods to be called on child movies:
	{
		sfAssertf(0, "ScaleformMgr: '%s' method called on a child movie %s (%d)", cFullFunctionName, sm_ScaleformArray[iIndex].cFilename, iIndex);

		result.SetBoolean(false);

		return result;
	}

#if __BANK
	// this IsAvailable check will double the time of calls to ActionScript, so only call in __DEV build for development/testing
	if (ms_bVerifyActionScriptMethods)
	{
		sfAssertf(pMovieView->IsAvailable(cFullFunctionName), "ScaleformMgr: ActionScript method %s is not available on movie %s", cFullFunctionName, sm_ScaleformArray[iIndex].cFilename);
	}
#endif // BANK

#if __BANK  // allows the user to ignore an invoke at runtime via the widget
	if ( (iNumberOfParams > 1) && (pArgs[1].IsString()) && (!stricmp("CONTAINER_METHOD", pFunctionName)) )  // if container method call then store the movie id in use as the 1st param
	{
		sfDebugf3("ScaleformMgr: About to call ActionScript CONTAINER_METHOD: %d::%s::%s with %d parameters", iIndex, sm_ScaleformArray[iIndex].cFilename, pArgs[1].GetString(), iNumberOfParams);
	}
	else if (ComplexObjectIdToInvoke != INVALID_COMPLEX_OBJECT_ID)
	{
		sfDebugf3("ScaleformMgr: About to call ActionScript method on ComplexObject: %d - %d::%s::%s with %d parameters", (s32)ComplexObjectIdToInvoke, iIndex, sm_ScaleformArray[iIndex].cFilename, cFullFunctionName, iNumberOfParams);
	}
	else
	{
		sfDebugf3("ScaleformMgr: About to call ActionScript method: %d::%s::%s with %d parameters", iIndex, sm_ScaleformArray[iIndex].cFilename, cFullFunctionName, iNumberOfParams);
	}

	if (!stricmp(pFunctionName, ms_cDebugIgnoreMethod))
	{
		result.SetBoolean(false);
		sfDebugf3("ScaleformMgr: Ignoring invoke to %s called on movie %s", ms_cDebugIgnoreMethod, sm_ScaleformArray[iIndex].cFilename);
	}
	else
#endif // __BANK
	{
		DIAG_CONTEXT_MESSAGE("Invoking actionscript function %s in movie %s", cFullFunctionName, sm_ScaleformArray[iIndex].cFilename);

		safecpy(sm_LastMovieName, sm_ScaleformArray[iIndex].cFilename, 255);
		safecpy(sm_LastMethodName, cFullFunctionName, 255);
		sm_LastMethodParamName[0] = 0;
		sm_bIsInsideActionScript = true;

		if (sfVerifyf(pMovieView, "ScaleformMgr: Movieview invalid at invoke for movie %s (%d)", sm_ScaleformArray[iIndex].cFilename, sm_ScaleformArray[iIndex].iMovieId))
		{
#if RAGE_TIMEBARS
			atDiagHashString hs(cFullFunctionName); // Using the hash strings here so we get a stable global address for the string
			pfAutoMarker mkr2(hs.GetCStr(), 11);
#endif
			g_CurrentMovie = &sm_ScaleformArray[iIndex];
			if (ComplexObjectIdToInvoke == INVALID_COMPLEX_OBJECT_ID)
			{
				// invoke the movie:
				pMovieView->Invoke(cFullFunctionName, &result, pArgs, iNumberOfParams);
			}
			else
			{
                GFxValue *pGfxValue = nullptr;
                bool canInvoke = false;
                CScaleformComplexObjectMgr::GetGfxValueFromUniqueIdForInvoke( ComplexObjectIdToInvoke, pGfxValue, canInvoke );

                if ( canInvoke && // Don't assert if the object has gone out of scope for invoking
                    sfVerifyf(pGfxValue && pGfxValue->IsDefined(), "Object that %s is being called on is undefined!", cFullFunctionName))
                {
                    pGfxValue->Invoke(cFullFunctionName, &result, pArgs, iNumberOfParams);

                    CComplexObjectArrayItem *pObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(ComplexObjectIdToInvoke);
                    if (pObject)
                    {
                        pObject->SetToInvokeOnObject(false);
                    }
                }
			}
			g_CurrentMovie = NULL;
		}
	}

	sm_bIsInsideActionScript=false;

	for (s32 i = 0; i < iNumberOfParams; i++)
	{
		if (pArgs[i].IsNumber())
		{
			char cTempStringParam[50];
			sprintf(cTempStringParam, "%0.2f", pArgs[i].GetNumber());
			safecat(sm_LastMethodParamName, cTempStringParam, MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
		}
		else if (pArgs[i].IsString())
		{
			safecat(sm_LastMethodParamName, "\"", MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
			safecat(sm_LastMethodParamName, pArgs[i].GetString(), MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
			safecat(sm_LastMethodParamName, "\"", MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
		}
		else if (pArgs[i].IsStringW())
		{
			//strcat(sm_LastMethodParamName, "\"");
			//char cTempSubString[64];
			//strncat(sm_LastMethodParamName, CTextConversion::charToAscii((char*)pArgs[i].GetStringW(), cTempSubString, 64), 50);
			//strcat(sm_LastMethodParamName, "\"");
		}
		else if (pArgs[i].IsObject() || pArgs[i].IsDisplayObject())
		{
			safecat(sm_LastMethodParamName, "OBJECT", MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
		}
		else if (pArgs[i].IsBool())
		{
			if (pArgs[i].GetBool())
				safecat(sm_LastMethodParamName, "TRUE", MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
			else
				safecat(sm_LastMethodParamName, "FALSE", MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
		}
		else
		{
			sfAssertf(0, "ScaleformMgr: Invalid params (type %d) passed into ActionScript method %s (see log)", pArgs[i].GetType(), cFullFunctionName);

			char cTempStringParam[50];
			sprintf(cTempStringParam, "Invalid Param Type: %d - %d", i, pArgs[i].GetType());
			safecat(sm_LastMethodParamName, cTempStringParam, MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
		}

		if (i < iNumberOfParams-1)
		{
			safecat(sm_LastMethodParamName, ", ", MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
		}
	}

#if __BANK

	char cDebugString[MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING];

	if (!stricmp(pFunctionName, ms_cDebugIgnoreMethod))
		sprintf(cDebugString, "ScaleformMgr: Ignored ActionScript method: %d::%s::%s(", iIndex, sm_ScaleformArray[iIndex].cFilename, cFullFunctionName);
	else
		sprintf(cDebugString, "ScaleformMgr: Called ActionScript method: %d::%s::%s(", iIndex, sm_ScaleformArray[iIndex].cFilename, cFullFunctionName);

	safecat(cDebugString, sm_LastMethodParamName, MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);
	safecat(cDebugString, ")", MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING);

	sfDebugf3("%s", cDebugString);

#endif // __BANK

	if ( ( ((bCallInstantly) || (!bStoreReturnValue)) && (iReturnId == -1) ) || (result.IsUndefined()) )
	{
		// return the actual result if we want to return instantly
		return result;
	}
	else
	{
		if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
		{
			sfAssertf(0, "ScaleformMgr: Can only set a return value into Return value array on the RenderThread!");
			return result;
		}

		if (iReturnId == -1)
		{
			iReturnId = ms_iGlobalReturnCounterRT;

			if (ms_iGlobalReturnCounterRT < (MAX_UNIQUE_RETURN_COUNTER*2))
				ms_iGlobalReturnCounterRT++;
			else
				ms_iGlobalReturnCounterRT = MAX_UNIQUE_RETURN_COUNTER+1;
		}

		sm_ReturnValuesFromInvoke[ms_iReturnValueCount].iUniqueId = iReturnId;

		if (!result.IsUndefined())
		{
			sm_ReturnValuesFromInvoke[ms_iReturnValueCount].gfxvalue_return_value = result;
			if (result.IsString())
			{
				sm_ReturnValuesFromInvoke[ms_iReturnValueCount].stringData = result.GetString();
				sm_ReturnValuesFromInvoke[ms_iReturnValueCount].gfxvalue_return_value.SetString(
					sm_ReturnValuesFromInvoke[ms_iReturnValueCount].stringData.c_str());
			}
		}
		else
		{
			sfAssertf(0, "ScaleformMgr: Return Slot %d as Scaleform method %s didnt return a value", sm_ReturnValuesFromInvoke[ms_iReturnValueCount].iUniqueId, pFunctionName);

			sm_ReturnValuesFromInvoke[ms_iReturnValueCount].gfxvalue_return_value.SetNumber(-1);
		}

		GFxValue return_value;
		return_value.SetNumber(sm_ReturnValuesFromInvoke[ms_iReturnValueCount].iUniqueId);


		if (ms_iReturnValueCount < MAX_STORED_RETURN_VALUES)
		{
			ms_iReturnValueCount++;
		}
		else
		{
			sfAssertf(0, "ScaleformMgr: Too many return values stored!");
		}

		return (return_value);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CallMethod()
// PURPOSE: Enables code to directly call and pass in parameters into functions
//			inside the Scaleform movie
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::CallMethod(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, bool bCallInstantly/*= false*/)
{
	if (BeginMethod(iIndex, iBaseClass, pFunctionName))
	{
		EndMethod(bCallInstantly);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CallMethodFromScript()
// PURPOSE: sorts out the params to the smallest amount for the methods called via
//			script before they are passed into the internal CallFunction function
//			always returns an INT which is passed back to script
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::CallMethodFromScript(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, s32 iOptionalIndex, float fParam1, float fParam2, float fParam3, float fParam4, float fParam5, const char *pParam1, const char *pParam2, const char *pParam3, const char *pParam4, const char *pParam5, bool bTextFileLookup)
{
	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		if (IsMovieActive(iIndex))
		{
			if (CScaleformMgr::BeginMethod(iIndex, iBaseClass, pFunctionName, iOptionalIndex))
			{
				if (fParam1 != SF_INVALID_PARAM && fParam1 != SF_INVALID_PARAM_SCRIPT)
					CScaleformMgr::AddParamFloat(fParam1);

				if (fParam2 != SF_INVALID_PARAM && fParam2 != SF_INVALID_PARAM_SCRIPT)
					CScaleformMgr::AddParamFloat(fParam2);

				if (fParam3 != SF_INVALID_PARAM && fParam3 != SF_INVALID_PARAM_SCRIPT)
					CScaleformMgr::AddParamFloat(fParam3);

				if (fParam4 != SF_INVALID_PARAM && fParam4 != SF_INVALID_PARAM_SCRIPT)
					CScaleformMgr::AddParamFloat(fParam4);

				if (fParam5 != SF_INVALID_PARAM && fParam5 != SF_INVALID_PARAM_SCRIPT)
					CScaleformMgr::AddParamFloat(fParam5);

				if (bTextFileLookup)
				{
					if (pParam1 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(TheText.Get(pParam1), cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}

					if (pParam2 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(TheText.Get(pParam2), cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}

					if (pParam3 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(TheText.Get(pParam3), cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}

					if (pParam4 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(TheText.Get(pParam4), cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}

					if (pParam5 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(TheText.Get(pParam5), cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}
				}
				else
				{
					if (pParam1 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(pParam1, cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}

					if (pParam2 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(pParam2, cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}

					if (pParam3 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(pParam3, cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}

					if (pParam4 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(pParam4, cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}

					if (pParam5 != NULL)
					{
						char cHtmlText[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
						CTextConversion::TextToHtml(pParam5, cHtmlText, NELEM(cHtmlText));
						CScaleformMgr::AddParamString(cHtmlText);
					}
				}

				CScaleformMgr::EndMethod();  // just buffer up the call but we dont return anything from this call to script
			}
		}
		else
		{
			sfAssertf(0, "ScaleformMgr: '%s' method called but movie '%s' (sm_ScaleformArray slot %d) is not active!", pFunctionName, sm_ScaleformArray[iIndex].cFilename, iIndex);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetMethodBuildBufferIndex()
// PURPOSE: returns which method build buffer to use based on thread or whether
//			an actionscript callback has interrupted flow
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::GetMethodBuildBufferIndex()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		if (HasExternalInterfaceBeenCalled())
		{
			return METHOD_BUILD_BUFFER_EXTERNAL_INTERFACE_UT;  // building from a trigger from External Interface (this can trigger during another call so needs a seperate buffer)
		}
		else
		{
			return METHOD_BUILD_BUFFER_UPDATE_THREAD;  // standard UT
		}
	}
	else
	{
		sfAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "Trying to get a method on some other thread besides the Update or Render thread! THIS IS PRETTY DARNED BAD!");

		s32 base;
		if (HasExternalInterfaceBeenCalled())
		{
			base = METHOD_BUILD_BUFFER_EXTERNAL_INTERFACE_RT;  // building from a trigger from External Interface (this can trigger during another call so needs a seperate buffer)
		}
		else
		{
			base = METHOD_BUILD_BUFFER_RENDER_THREAD;  // standard RT
		}
		return base + g_RenderThreadIndex;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::BeginMethod()
// PURPOSE: BEGIN scaleform movie method call
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::BeginMethod(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName)
{
	return BeginMethodInternal(iIndex, iBaseClass, pFunctionName, -1, INVALID_COMPLEX_OBJECT_ID);
}
bool CScaleformMgr::BeginMethod(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, s32 iOptionalIndex)
{	
	return BeginMethodInternal(iIndex, iBaseClass, pFunctionName, iOptionalIndex, INVALID_COMPLEX_OBJECT_ID);
}
bool CScaleformMgr::BeginMethodOnComplexObject(COMPLEX_OBJECT_ID ComplexObjectId, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName)
{
	CComplexObjectArrayItem *pObject = CScaleformComplexObjectMgr::GetObjectFromUniqueId(ComplexObjectId);

	// invoke the gfx value on the object if it is active:
	if (sfVerifyf(pObject, "ScaleformMgr: Complex object not valid"))
	{
		pObject->SetToInvokeOnObject(true);

		s32 iMovieId = pObject->GetMovieId();
		return BeginMethodInternal(iMovieId, iBaseClass, pFunctionName, -1, ComplexObjectId);
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::BeginMethodInternal()
// PURPOSE: BEGIN scaleform movie method call - internal
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::BeginMethodInternal(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, s32 iOptionalIndex, COMPLEX_OBJECT_ID ComplexObjectId)
{
	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		if (sfVerifyf(IsMovieUpdateable(iIndex), "Scaleform method %s called on inactive movie %d", pFunctionName, iIndex))
		{
			s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();

			if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iMovieId == -1, "CallFunctionEnd not called on %s in %s (%d)", sm_ScaleformMethodBuild[iBuffer].cMethodName, sm_ScaleformArray[sm_ScaleformMethodBuild[iBuffer].iMovieId].cFilename, sm_ScaleformMethodBuild[iBuffer].iMovieId))
			{
				sm_ScaleformMethodBuild[iBuffer].iMovieId = (s8)iIndex;
				safecpy(sm_ScaleformMethodBuild[iBuffer].cMethodName, pFunctionName, MAX_CHARS_IN_SCALEFORM_METHOD_NAME);
				sm_ScaleformMethodBuild[iBuffer].iBaseClass = iBaseClass;

				if (iOptionalIndex != -1)  // if we passed in an optional movie index, then store this
				{
					sm_ScaleformMethodBuild[iBuffer].iMovieIdThisInvokeIsLinkedTo = (s8)iOptionalIndex;

					sfDebugf3("ScaleformMgr: BEGIN_METHOD(%s) called on movie '%s (%d)' linked to '%s (%d)' using buffer %d", pFunctionName, sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iOptionalIndex].cFilename, iOptionalIndex, iBuffer);
				}
				else
				{
					sfDebugf3("ScaleformMgr: BEGIN_METHOD(%s) called on movie '%s (%d)' using buffer %d", pFunctionName, sm_ScaleformArray[iIndex].cFilename, iIndex, iBuffer);
				}

				sm_ScaleformMethodBuild[iBuffer].ComplexObjectIdToInvoke = ComplexObjectId;

				return true;
			}
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::EndMethod()
// PURPOSE: END scaleform movie method call - this invokes and returns a value
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::EndMethod(bool bCallInstantly)
{
	EndMethodInternal(bCallInstantly, false);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::EndMethodReturnValue()
// PURPOSE: END scaleform movie method call - this invokes and returns a value
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::EndMethodReturnValue(const s32 iExistingReturnValue)
{
	if (iExistingReturnValue != 0)  // if we passed a valid existing return value, clear this 1st
	{
		ClearReturnValue(iExistingReturnValue);
	}

	s32 iReturnValue = 0;

#if __ASSERT
	char cTempString[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();
	safecpy(cTempString, sm_ScaleformMethodBuild[iBuffer].cMethodName, MAX_CHARS_IN_SCALEFORM_METHOD_NAME);
#endif // __ASSERT

	GFxValue retValue = EndMethodInternal(false, true);

	if (retValue.IsNumber())
	{
		iReturnValue = (s32)retValue.GetNumber();

#if __ASSERT
		sfDebugf3("ScaleformMgr: Returned id for method %s returned as %d", cTempString, iReturnValue);
#endif // __ASSERT
	}
#if __ASSERT
	else
	{
		sfAssertf(0, "CScaleformMgr: EndMethodReturnValue method %s didnt return a value", cTempString);
	}
#endif // __ASSERT

	return iReturnValue;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::EndMethodInt()
// PURPOSE: END scaleform movie method call - this invokes and returns a value
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::EndMethodInt(bool bCallInstantly)
{
	s32 iReturnValue = 0;

	if (!bCallInstantly)
	{
		sfAssertf(0, "CScaleformMgr::EndMethodInt can only be called if returning instantly");
		return iReturnValue;
	}

#if __ASSERT
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();
	char cTempString[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];
	safecpy(cTempString, sm_ScaleformMethodBuild[iBuffer].cMethodName, MAX_CHARS_IN_SCALEFORM_METHOD_NAME);
#endif // __ASSERT

	GFxValue retValue = EndMethodInternal(bCallInstantly, true);

	if (retValue.IsNumber())
	{
		iReturnValue = (s32)retValue.GetNumber();
	}
#if __ASSERT
	else
	{
		sfAssertf(0, "Method %s does not return a INT", cTempString);
	}
#endif // __ASSERT

	return iReturnValue;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::EndMethodFloat()
// PURPOSE: END scaleform movie method call - this invokes and returns a value
/////////////////////////////////////////////////////////////////////////////////////
float CScaleformMgr::EndMethodFloat(bool bCallInstantly)
{
	float fReturnValue = 0.0f;

	if (!bCallInstantly)
	{
		sfAssertf(0, "CScaleformMgr::EndMethodFloat can only be called if returning instantly");
		return fReturnValue;
	}

#if __ASSERT
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();
	char cTempString[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];
	safecpy(cTempString, sm_ScaleformMethodBuild[iBuffer].cMethodName, MAX_CHARS_IN_SCALEFORM_METHOD_NAME);
#endif // __ASSERT

	GFxValue retValue = EndMethodInternal(bCallInstantly, true);

	if (retValue.IsNumber())
	{
		fReturnValue = (float)retValue.GetNumber();
	}
#if __ASSERT
	else
	{
		sfAssertf(0, "Method %s does not return a FLOAT", cTempString);
	}
#endif // __ASSERT

	return fReturnValue;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::EndMethodBool()
// PURPOSE: END scaleform movie method call - this invokes and returns a value
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::EndMethodBool(bool bCallInstantly)
{
	bool bReturnValue = false;

	if (!bCallInstantly)
	{
		sfAssertf(0, "CScaleformMgr::EndMethodBool can only be called if returning instantly");
		return bReturnValue;
	}

#if __ASSERT
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();
	char cTempString[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];
	safecpy(cTempString, sm_ScaleformMethodBuild[iBuffer].cMethodName, MAX_CHARS_IN_SCALEFORM_METHOD_NAME);
#endif // __ASSERT

	GFxValue retValue = EndMethodInternal(bCallInstantly, true);

	if (retValue.IsBool())
	{
		bReturnValue = retValue.GetBool();
	}
#if __ASSERT
	else
	{
		sfAssertf(0, "Method %s does not return a BOOL", cTempString);
	}
#endif // __ASSERT

	return bReturnValue;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::EndMethodGfxValue()
// PURPOSE: END scaleform movie method call - this invokes and returns a value
/////////////////////////////////////////////////////////////////////////////////////
GFxValue CScaleformMgr::EndMethodGfxValue(bool bCallInstantly)
{
	GFxValue retValue;
	retValue.SetUndefined();

	if (!bCallInstantly)
	{
		sfAssertf(0, "CScaleformMgr::EndMethodGfxValue can only be called if returning instantly");
		return retValue;
	}

#if __ASSERT
	char cTempString[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();
	safecpy(cTempString, sm_ScaleformMethodBuild[iBuffer].cMethodName, MAX_CHARS_IN_SCALEFORM_METHOD_NAME);
#endif // __ASSERT

	retValue = EndMethodInternal(bCallInstantly, true);

	return retValue;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::EndMethodInternal()
// PURPOSE: internal "end" method call.  Returns a GfxValue that is checked later on
/////////////////////////////////////////////////////////////////////////////////////
GFxValue CScaleformMgr::EndMethodInternal(bool bCallInstantly, bool bStoreReturnedValue)
{
	GFxValue retValue;
	retValue.SetUndefined();

	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();

	if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iMovieId != -1, "no matching CallFunctionBegin called %d", iBuffer))
	{
		GFxValue params[MAX_NUM_PARAMS_IN_SCALEFORM_METHOD];

		for (s32 i = 0; i < sm_ScaleformMethodBuild[iBuffer].iParamCount; i++)
		{
			switch (sm_ScaleformMethodBuild[iBuffer].params[i].param_type)
			{
				case PARAM_TYPE_NUMBER:
				{
					params[i].SetNumber(sm_ScaleformMethodBuild[iBuffer].params[i].ParamNumber);
					break;
				}
				case PARAM_TYPE_BOOL:
				{
					params[i].SetBoolean(sm_ScaleformMethodBuild[iBuffer].params[i].bParamBool);
					break;
				}
				case PARAM_TYPE_STRING:
				{
					if (sm_ScaleformMethodBuild[iBuffer].params[i].pParamString)
					{
						params[i].SetString(sm_ScaleformMethodBuild[iBuffer].params[i].pParamString->c_str());
					}
					else
					{
						params[i].SetString("\0");
					}

					break;
				}
				case PARAM_TYPE_GFXVALUE:
				{
					params[i] = *sm_ScaleformMethodBuild[iBuffer].params[i].pParamGfxValue;
					break;
				}
				default:
				{
					sfAssertf(0, "ScaleformMgr: Invalid param type (%d)", (s32)sm_ScaleformMethodBuild[iBuffer].params[i].param_type);
				}
			}
		}

		retValue = CallMethodInternal(	sm_ScaleformMethodBuild[iBuffer].iMovieId,
										sm_ScaleformMethodBuild[iBuffer].iMovieIdThisInvokeIsLinkedTo,
										sm_ScaleformMethodBuild[iBuffer].iBaseClass,
										sm_ScaleformMethodBuild[iBuffer].cMethodName,
										params,
										sm_ScaleformMethodBuild[iBuffer].iParamCount,
										bCallInstantly,
										bStoreReturnedValue,
										-1,
										sm_ScaleformMethodBuild[iBuffer].ComplexObjectIdToInvoke);

		// reset the values:
		for (s32 i = 0; i < sm_ScaleformMethodBuild[iBuffer].iParamCount; i++)  // ensure any GFXVALUE param alloc is deleted
		{
			if (sm_ScaleformMethodBuild[iBuffer].params[i].param_type == PARAM_TYPE_STRING)
			{
				if (sm_ScaleformMethodBuild[iBuffer].params[i].pParamString)
				{
					sm_ScaleformMethodBuild[iBuffer].params[i].pParamString->Clear();
					delete (sm_ScaleformMethodBuild[iBuffer].params[i].pParamString);
					sm_ScaleformMethodBuild[iBuffer].params[i].pParamString = NULL;
				}
			}

			if (sm_ScaleformMethodBuild[iBuffer].params[i].param_type == PARAM_TYPE_GFXVALUE)
			{
				delete (sm_ScaleformMethodBuild[iBuffer].params[i].pParamGfxValue);
				sm_ScaleformMethodBuild[iBuffer].params[i].pParamGfxValue = NULL;
			}

			sm_ScaleformMethodBuild[iBuffer].params[i].param_type = PARAM_TYPE_INVALID;
		}

		sm_ScaleformMethodBuild[iBuffer].cMethodName[0] = '\0';
		sm_ScaleformMethodBuild[iBuffer].iMovieId = -1;
		sm_ScaleformMethodBuild[iBuffer].iMovieIdThisInvokeIsLinkedTo = -1;
		sm_ScaleformMethodBuild[iBuffer].iParamCount = 0;
		sm_ScaleformMethodBuild[iBuffer].iReturnId = -1;
		sm_ScaleformMethodBuild[iBuffer].iBaseClass = SF_BASE_CLASS_GENERIC;
		sm_ScaleformMethodBuild[iBuffer].ComplexObjectIdToInvoke = INVALID_COMPLEX_OBJECT_ID;
	}

	return retValue;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::AddParamInt()
// PURPOSE: Passes an INT into current building Scaleform movie method call
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::AddParamInt(s32 iParam)
{
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();

	if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iMovieId != -1, "no matching CallFunctionBegin called %d", iBuffer))
	{
		if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iParamCount < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD, "too many params in method call"))
		{
			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].ParamNumber = (Double)iParam;

			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].param_type = PARAM_TYPE_NUMBER;

			sm_ScaleformMethodBuild[iBuffer].iParamCount++;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::AddParamFloat()
// PURPOSE: Passes an FLOAT into current building Scaleform movie method call
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::AddParamFloat(Double fParam)
{
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();

	if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iMovieId != -1, "no matching CallFunctionBegin called %d", iBuffer))
	{
		if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iParamCount < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD, "too many params in method call"))
		{
			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].ParamNumber = fParam;

			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].param_type = PARAM_TYPE_NUMBER;

			sm_ScaleformMethodBuild[iBuffer].iParamCount++;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::AddParamBool()
// PURPOSE: Passes an BOOL into current building Scaleform movie method call
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::AddParamBool(bool bParam)
{
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();

	if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iMovieId != -1, "no matching CallFunctionBegin called %d", iBuffer))
	{
		if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iParamCount < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD, "too many params in method call"))
		{
			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].bParamBool = bParam;

			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].param_type = PARAM_TYPE_BOOL;

			sm_ScaleformMethodBuild[iBuffer].iParamCount++;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::AddParamString()
// PURPOSE: Passes an STRING into current building Scaleform movie method call
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::AddParamString(const char *pParam, bool bConvertToHtml)
{
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();

	if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iMovieId != -1, "no matching CallFunctionBegin called %d", iBuffer))
	{
		if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iParamCount < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD, "too many params in method call"))
		{
			if (bConvertToHtml && pParam)
			{
				char htmlString[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
				CTextConversion::TextToHtml(pParam, htmlString, NELEM(htmlString));
				sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].pParamString = rage_new atString(htmlString);
				// verify that this HTML string that has an IMG in it has had a ref added by whatever is using it, assert if not
				CheckTxdInStringHasRef(sm_ScaleformMethodBuild[iBuffer].iMovieId, sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].pParamString->c_str());
			}
			else
			{
				sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].pParamString = rage_new atString(pParam);
			}

			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].param_type = PARAM_TYPE_STRING;

			sm_ScaleformMethodBuild[iBuffer].iParamCount++;
		}
	}
}

void CScaleformMgr::AddParamString(const char *pParam, s32 iBufferLen, bool bConvertToHtml /* = true */)
{
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();

	if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iMovieId != -1, "no matching CallFunctionBegin called %d", iBuffer))
	{
		if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iParamCount < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD, "too many params in method call"))
		{
			if (bConvertToHtml && pParam)
			{
				char* htmlString = rage_new char[iBufferLen];
				CTextConversion::TextToHtml(pParam, htmlString, iBufferLen);
				sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].pParamString = rage_new atString(htmlString);
				// verify that this HTML string that has an IMG in it has had a ref added by whatever is using it, assert if not
				CheckTxdInStringHasRef(sm_ScaleformMethodBuild[iBuffer].iMovieId, sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].pParamString->c_str());
				delete[] htmlString;
			}
			else
			{
				sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].pParamString = rage_new atString(pParam);
			}

			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].param_type = PARAM_TYPE_STRING;

			sm_ScaleformMethodBuild[iBuffer].iParamCount++;
		}
	}
}

void CScaleformMgr::AddParamLocString(const char* pParam, bool bConvertToHtml /* = true */)
{
	AddParamString(TheText.Get(pParam), bConvertToHtml);
}

void CScaleformMgr::AddParamLocString(u32 hash, bool bConvertToHtml /* = true */)
{
	AddParamString(TheText.Get(hash,"MISSING"), bConvertToHtml);
}

void CScaleformMgr::AddParamFormattedInt(s64 iParam, const char* pPrefix)
{
	char tempValue[64];
	CTextConversion::FormatIntForHumans(iParam, tempValue, 64, pPrefix);
	AddParamString(tempValue, false);
}

void CScaleformMgr::AddParamInt64(s64 iParam)
{
	const u32 STRING_SIZE = 65;
	char tempValue[STRING_SIZE];
	formatf(tempValue, STRING_SIZE, "%" I64FMT "d", iParam);
	AddParamString(tempValue, false);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::AddParamGfxValue()
// PURPOSE: Passes an GFXVALUE into current building Scaleform movie method call
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::AddParamGfxValue(GFxValue param)
{
	s32 iBuffer = CScaleformMgr::GetMethodBuildBufferIndex();

	if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iMovieId != -1, "no matching CallFunctionBegin called %d", iBuffer))
	{
		if (sfVerifyf(sm_ScaleformMethodBuild[iBuffer].iParamCount < MAX_NUM_PARAMS_IN_SCALEFORM_METHOD, "too many params in method call"))
		{
			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].pParamGfxValue = rage_new GFxValue;

			*sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].pParamGfxValue = param;

			sm_ScaleformMethodBuild[iBuffer].params[sm_ScaleformMethodBuild[iBuffer].iParamCount].param_type = PARAM_TYPE_GFXVALUE;

			sm_ScaleformMethodBuild[iBuffer].iParamCount++;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetTxdAndTextureNameFromText()
// PURPOSE: gets a txd and texture name from the passed string
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::GetTxdAndTextureNameFromText(const char *pText, char *pTxdName, char *pTextureName)
{
	if (pText && pTxdName && pTextureName)
	{
		// construct a texture dictionary name and a texture name from the url we get:
		bool bConstructingTxdName = true;
		s32 iCharCount = 0;
		for (s32 i = 6; i < strlen(pText) && (iCharCount < RAGE_MAX_PATH-1); i++)
		{
			if (pText[i] == '/')
			{
				bConstructingTxdName = false;  // move onto the texture name now we have reached the seperator in the full path

				iCharCount = 0;
				continue;
			}

			if (bConstructingTxdName)
			{
				pTxdName[iCharCount++] = pText[i];
				pTxdName[iCharCount] = '\0';
			}
			else
			{
				pTextureName[iCharCount++] = pText[i];
				pTextureName[iCharCount] = '\0';
			}
		}
	}

	return (pTxdName[0] != '\0' && pTextureName[0] != '\0');
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CheckTxdInStringHasRef()
// PURPOSE: Parses the final text string that will be sent to ActionScript and if
//			it has a TXD in it as an HTML tag ensure it has had a ref added.
//			NOTE: We dont add a ref here as its up to the system to add it when
//			it uses it and remove it when finished, otherwise if we did add a ref
//			here we would have to keep it until the movie ended which wouldnt be
//			as optimal.  This will catch misuse.
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::CheckTxdInStringHasRef(s32 iMovieId, const char *pString)
{
	if (!sfVerifyf(IsMovieIdInRange(iMovieId), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iMovieId))
		return;

	if (!pString)
		return;

	s32 iStringLength = istrlen(pString);

	char cTagString[1000];
	bool bInsideBracket = false;
	bool bInsideQuote = false;
	s32 d = 0;

	for (s32 c = 0; c < iStringLength-7; c++)
	{
		if (!bInsideBracket)
		{
			if (pString[c] == '<' &&
				pString[c+1] == 'i' &&
				pString[c+2] == 'm' &&
				pString[c+3] == 'g' &&
				pString[c+4] == ' ' &&
				pString[c+5] == 's' &&
				pString[c+6] == 'r' &&
				pString[c+7] == 'c')
			{
				bInsideBracket = true;
				continue;
			}
		}
		else
		{
			if (pString[c] == '>')
			{
				bInsideBracket = false;
				continue;
			}

			if (pString[c] == '\'')
			{
				if (bInsideBracket)
				{
					bInsideQuote = !bInsideQuote;

					if (bInsideQuote)
					{
						d = 0;
					}
					else  // end of quote
					{
						cTagString[d] = '\0';

						// EXAMPLE: <img src='img://Email_SurfShop_Bail_Bond/Target_SurfShop' />

						// construct a texture dictionary name and a texture name from the url we get:
						char cTxdName[RAGE_MAX_PATH];
						char cTextureName[RAGE_MAX_PATH];

						cTxdName[0] = '\0';
						cTextureName[0] = '\0';

						if (!strncmp(cTagString, "img://", 6))  // only attempt to add a ref if the string in quotes starts with 'img://'
						{
							if (GetTxdAndTextureNameFromText(&cTagString[0], &cTxdName[0], &cTextureName[0]))
							{
								AddTxdRef(iMovieId, cTxdName, NULL, false, false);
							}
						}
					}

					continue;
				}
			}

			if (bInsideQuote)
			{
				cTagString[d++] = pString[c];
			}
		}
	}
}

bool CScaleformMgr::VerifyVarType(GFxValue& var, GFxValue::ValueType requiredType, const char* ASSERT_ONLY(varName), GFxValue& ASSERT_ONLY(movieClip))
{
	if ( !(var.GetType() == requiredType || (requiredType == GFxValue::VT_Object && var.IsObject())))
	{
#if __ASSERT
		GFxValue mcInstanceName;
		if(movieClip.IsDefined())
		{
			movieClip.GetMember("_name", &mcInstanceName);
		}
		else
		{
			mcInstanceName = "undefined";
		}

		sfAssertf(0, "Wrong type for argument \"%s\", expected %s and got %s on MovieClip %s", varName, sfScaleformManager::GetTypeName(requiredType), sfScaleformManager::GetTypeName(var),  mcInstanceName.GetString());
#endif
		return false;
	}

	return true;
}

bool CScaleformMgr::TranslateWindowCoordinateToScaleformMovieCoordinate( float const rawX, float const rawY, s32 const movieId, float& out_x, float& out_y )
{
	GFxMovieView* pView = CScaleformMgr::GetMovieView( movieId );
	if( pView )
	{
		GRectF rawRect( 0.f, 0.f, ACTIONSCRIPT_STAGE_SIZE_X, ACTIONSCRIPT_STAGE_SIZE_Y );
		GRectF screenRect = pView->TranslateToScreen( rawRect );
		GRectF visibleRect = pView->GetVisibleFrameRect();

		float const c_rawWidth = ( rawRect.Right - rawRect.Left );
		float const c_screenWidth = ( screenRect.Right - screenRect.Left );

		float const c_rawHeight = ( rawRect.Bottom - rawRect.Top );
		float const c_screenHeight = ( screenRect.Bottom - screenRect.Top );

		float const c_movieXScale = c_rawWidth / c_screenWidth;
		float const c_movieYScale = c_rawHeight / c_screenHeight;

		float const c_uiWidth = (float)VideoResManager::GetUIWidth();
		float const c_movieXOffset = ( c_screenWidth < c_uiWidth ) ? -screenRect.Left * c_movieXScale : visibleRect.Left;

		float const c_uiHeight = (float)VideoResManager::GetUIHeight();
		float const c_movieYOffset = ( c_screenHeight < c_uiHeight ) ? -screenRect.Top * c_movieYScale : visibleRect.Top;

		out_x = ( rawX * c_movieXScale ) + c_movieXOffset;
		out_y = ( rawY * c_movieYScale ) + c_movieYOffset;
	}

	return pView != NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::ForceCollectGarbage()
// PURPOSE: Runs a gc pass on the movie of the id passed
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::ForceCollectGarbage(s32 iIndex)
{
	if (IsMovieActive(iIndex))
	{
		if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
		{
			// flag to perform garbage collection on renderthread
			sm_ScaleformArray[iIndex].bPerformGarbageCollection.GetUpdateBuf() = true;
		}
		else
		{
			sfDebugf3("CScaleformMgr: CollectGarbage about to be called on movie %s (%d)", sm_ScaleformArray[iIndex].cFilename, iIndex);

			sfScaleformMovieView *pRawMovieView = g_ScaleformStore.GetRawMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));
			if (pRawMovieView)
			{
				pRawMovieView->GetMovieView().ForceCollectGarbage();

				sfDebugf3("CScaleformMgr: CollectGarbage called on movie %s (%d)", sm_ScaleformArray[iIndex].cFilename, iIndex);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::LockMovie()
// PURPOSE: Prevents a movie from being updated or drawn temporarily
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::LockMovie(s32 iIndex)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr:: LockMovie can only be called on the UpdateThread!");
		return;
	}

	if (IsMovieActive(iIndex))
	{
#if __SCALEFORM_CRITICAL_SECTIONS
		gs_ScaleformMovieCS[iIndex].Lock();
#endif // __SCALEFORM_CRITICAL_SECTIONS
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::UnlockMovie()
// PURPOSE: Releases a previously held lock
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::UnlockMovie(s32 iIndex)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr:: UnlockMovie can only be called on the UpdateThread!");
		return;
	}

	if (IsMovieActive(iIndex))
	{
#if __SCALEFORM_CRITICAL_SECTIONS
		gs_ScaleformMovieCS[iIndex].Unlock();
#endif // __SCALEFORM_CRITICAL_SECTIONS
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::UpdateMovieOnly()
// PURPOSE: VERY RARE CASES MAY REQUIRE TO ONLY UPDATE THE MOVIE AND NOT RENDER IT!
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::UpdateMovieOnly(s32 iIndex, float fTimeStep)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr:: RenderMovie can only be called on the RenderThread!");
		return;
	}

	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		if (IsMovieActive(iIndex))
		{
#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
#endif // __SCALEFORM_CRITICAL_SECTIONS

			if (sm_ScaleformArray[iIndex].bNeedsToUpdateThisFrame)
			{
				FlushInvokeList();
				UpdateMovie(iIndex, fTimeStep);
			}
		}
	}
}


void CScaleformMgr::DoNotUpdateThisFrame(s32 iIndex)
{
	if (IsMovieActive(iIndex))
	{
#if __SCALEFORM_CRITICAL_SECTIONS
		SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
#endif // __SCALEFORM_CRITICAL_SECTIONS

		sm_ScaleformArray[iIndex].bNeedsToUpdateThisFrame = false;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RenderMovie()
// PURPOSE: Renders scaleform movie of the id passed
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RenderMovie(s32 iIndex, float fTimeStep, bool bUpdateWhenPaused, bool bForceUpdateBeforeRender, float fAlpha /* = 1.0f */)
{
#define FRAMES_TO_CONSIDER_STILL_RENDERING (3)

#if RSG_PC
	if (((SMultiplayerChat::IsInstantiated() &&
		SMultiplayerChat::GetInstance().IsChatTyping()) ||
		(STextInputBox::IsInstantiated() &&
		STextInputBox::GetInstance().IsActive()))
		|| g_rlPc.IsUiShowing() ) // hide instructional buttons if SCUI is showing ...hints are confusing
	{
		int iMovieID = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_INSTRUCTIONAL_BUTTONS).GetMovieID();
		if(iMovieID == iIndex)
		{
			return;
		}
	}
#endif // RSG_PC

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr:: RenderMovie can only be called on the RenderThread!");
		return;
	}

	if (!sfVerifyf(ms_bFrameBegun, "Missing a call to beginframe?"))
	{
		return;
	}

	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		PF_AUTO_PUSH_TIMEBAR("ScaleformMgr::RenderMovie()");

		bool bSkipWhilePaused = fwTimer::IsGamePaused() && sm_ScaleformArray[iIndex].bDontRenderWhilePaused;

		if (IsMovieActive(iIndex) && bSkipWhilePaused == false)
		{
	#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
	#endif // __SCALEFORM_CRITICAL_SECTIONS

			GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

			if (bForceUpdateBeforeRender || // very special cases (like script) may require a movie to update before its rendered
				(sm_ScaleformArray[iIndex].bNeedsToUpdateThisFrame))
			{
				FlushInvokeList();
				UpdateMovie(iIndex, fTimeStep);
			}

			// make sure global alpha is set to 1 (emissive pass reuses this global)
			CShaderLib::SetGlobalEmissiveScale(1.0f,true);
			CShaderLib::SetGlobalAlpha(fAlpha);
			CShaderLib::UpdateGlobalDevScreenSize();

			if ( !fwTimer::IsGamePaused() || bUpdateWhenPaused  )
			{
	//			UpdateMovie(iIndex, fTimeStep);
				sm_ScaleformArray[iIndex].fCurrentTimestep = fTimeStep;
			}
			else
			{
				sm_ScaleformArray[iIndex].fCurrentTimestep = -1.0f;
			}

			PIX_AUTO_TAGN(0, "Scaleform Movie %s", sm_ScaleformArray[iIndex].cFilename);
			DIAG_CONTEXT_MESSAGE("Drawing Flash Movie %s", sm_ScaleformArray[iIndex].cFilename);
			sfScaleformManager::AutoSetCurrMovieName currMovie(sm_ScaleformArray[iIndex].cFilename);

			sm_ScaleformArray[iIndex].bRender3D.GetRenderBuf() = false;
			sm_ScaleformArray[iIndex].bRender3DSolid.GetRenderBuf() = false;

			sfScaleformMovieView *pRawMovieView = g_ScaleformStore.GetRawMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

			if (pRawMovieView)
			{
	// #if __DEV
	//			sfDisplayf("Rendering SF Movie %s (%d) Pos: %0.2f,%0.2f  Size: %0.2f,%0.2f", sm_ScaleformArray[iIndex].cFilename, iIndex, sm_ScaleformArray[iIndex].vPos.GetRenderBuf().x, sm_ScaleformArray[iIndex].vPos.GetRenderBuf().y, sm_ScaleformArray[iIndex].vSize.GetRenderBuf().x, sm_ScaleformArray[iIndex].vSize.GetRenderBuf().y);
	// #endif // __DEV
				pRawMovieView->Draw();
				sm_ScaleformArray[iIndex].iRendered = FRAMES_TO_CONSIDER_STILL_RENDERING;

#if __BANK
				if( ms_bShowOutlineFor2DMovie )
				{
					struct DebugDrawBaseData
					{
						DebugDrawBaseData( const Vector2& transform, const Color32& color, const atHashWithStringBank& filter  )
							: m_transform(transform)
							, m_color(color)
							, m_nameFilter(filter)
							, m_bDrewSomething(false)
						{};

						const Vector2& m_transform;
						const Color32& m_color;
						const atHashWithStringBank& m_nameFilter;
						bool m_bDrewSomething;
					};

					class DebugDrawMemberVisitor : public GFxValue::ObjectVisitor
					{
					public:
						DebugDrawMemberVisitor(const DebugDrawBaseData& data, Vector2& drawPos, int depth) 
							: m_baseData(data), m_drawPos(drawPos), m_depth(depth)
						{
						}

					private:
						const DebugDrawBaseData& m_baseData;
						Vector2& m_drawPos;
						int m_depth;

						const char* formatObject(const GFxValue& object, atString& buildThis)
						{
							
							     if( object.IsUndefined() )		{ buildThis = "-undefined-"; }
							else if( object.IsNull()	  )		{ buildThis = "-null-"; }
							else if( object.IsBool()      )		{ buildThis = object.GetBool() ? "TRUE" : "FALSE"; }
							else if( object.IsString()    )		{ buildThis = "\""; buildThis += object.GetString(); buildThis += "\""; }
							else if( object.IsNumber()    )
							{
								char temp[32] = ""; 
								Double fNum = object.GetNumber();
								int iNum = int(fNum);
								// check if it's a whole number
								if( fNum == iNum )
									formatf(temp, "%i", iNum);
								else
									formatf(temp, "%.4f", fNum);

								buildThis = temp;
							}
							else if( object.IsArray()     )
							{
								buildThis.Reset();
								buildThis.Reserve(128);
								buildThis = "[";
								int len=object.GetArraySize();
								for(int i=0; i < len; ++i)
								{
									GFxValue temp;
									if( object.GetElement(i, &temp) )
									{
										atString builder;
										buildThis += formatObject(temp, builder);
									}
									else
										buildThis += "-?-";

									// if not at the end
									if( len-i > 1)
										buildThis += ",";
								}
								buildThis += "]";
							}
							else if( object.IsDisplayObject() )
							{
								GFxValue pName;
								if( object.GetMember("_name", &pName ) )
									buildThis = pName.GetString();
								else
									buildThis = "-no name-";
							}
							else if( object.IsObject() )
							{
								if( m_depth < ms_iMaxMemberVariableDepth )
								{
									DebugDrawMemberVisitor memvisitor(m_baseData, m_drawPos, m_depth+1);
									object.VisitMembers(&memvisitor);
								}
								else
								{
									GFxValue pName;
									if( object.GetMember("_name", &pName ) )
									{
										buildThis = ">";
										buildThis += pName.GetString();
										buildThis += "<";
									}
									else
										buildThis = ">Object<";
								}
							}
							else
							{
								buildThis = "-unknown type-";
							}

							return buildThis.c_str();
						}

					public:

						virtual void Visit(const char* name, const GFxValue& object)
						{
							atString curObj, builder;
							if( m_depth > 0 )
							{
							//	grcDebugDraw::Text(m_drawPos, DD_ePCS_NormalisedZeroToOne, m_baseData.m_color, "{" );

								// tab over for any recursive depths
								m_drawPos.x += 0.0125f;
								//m_drawPos.y += 0.0125;
							}

							curObj += name;
							curObj += ":";
							curObj += formatObject(object, builder);
							
							grcDebugDraw::Text(m_drawPos, DD_ePCS_NormalisedZeroToOne, m_baseData.m_color, curObj.c_str() );
							m_drawPos.y += 0.0125f; // so it doesn't overlap the boxes as much.
							if( m_depth > 0 )
							{
								m_drawPos.x -= 0.0125f;
								//grcDebugDraw::Text(m_drawPos, DD_ePCS_NormalisedZeroToOne, m_baseData.m_color, "}" );
								//m_drawPos.y += 0.0125; // so it doesn't overlap the boxes as much.
							}
						}
					};


					class DebugDrawRecursiveVisitor : public GFxValue::ObjectVisitor
					{
					public:
						DebugDrawRecursiveVisitor(DebugDrawBaseData& data, bool bForceDebugDraw, const GMatrix2D& parentMatrix, const GFxValue& parent, Vector2& extents, atString rootPath) 
							: m_baseData(data), m_parentMatrix(parentMatrix), m_parent(parent), m_extents(extents), m_textDrawnSoFar(0), m_bForceDebugDraw(bForceDebugDraw), m_rootPath(rootPath)
						{
						}


						DebugDrawBaseData& m_baseData;
						const GMatrix2D& m_parentMatrix;

						const GFxValue& m_parent;
						Vector2& m_extents;
						int m_textDrawnSoFar;
						bool m_bForceDebugDraw;
						atString m_rootPath;

						virtual void Visit(const char* name, const GFxValue& object)
						{
							// handle wrong type
							if( object.IsUndefined() )
								return;

							if( object.IsDisplayObject())
							{
								GFxValue parent;

								if( object.GetMember("_parent", &parent) )//&& object.GetDisplayInfo(&info) )
								{
									if( parent == m_parent )
									{
										GMatrix2D matrx, dispMatrix;
										GFxValue width, height;
										GFxValue::DisplayInfo info;

										object.GetDisplayMatrix(&dispMatrix);
										object.GetDisplayInfo(&info);
										object.GetMember("_width", &width);
										object.GetMember("_height", &height);

										m_extents.x = Min(m_extents.x, float(info.GetX()));
										m_extents.y = Min(m_extents.y, float(info.GetY()));

										static int iOrder[] = {1,2,3,4};

										for(int i=0; i < 4; ++i)
										{
											switch(iOrder[i])
											{
											case 1 :matrx.AppendTranslation(Float(info.GetX()), Float(info.GetY())); break;
											case 2: matrx.AppendScaling(Float(info.GetXScale())/100.0f, Float(info.GetYScale())/100.0f); break;
											case 3: matrx.AppendRotation(Float(info.GetRotation()) * DtoR); break;
											case 4: matrx.Prepend(m_parentMatrix);	break;
											case 5: matrx.Append(m_parentMatrix);	break;
											case 6: matrx.Append(dispMatrix);		break;
											case 7: matrx.Prepend(dispMatrix);		break;
											case 8: rage::SwapEm(dispMatrix.M_[0][2], dispMatrix.M_[1][2]); break;
											};
										};
									
										Vector2 extents;

										
										
										atString namePlus;

										//if( ms_bFullNames )
										{
											namePlus = m_rootPath;
											namePlus += ".";
										}
										namePlus += name;

										bool bDebugDraw = m_bForceDebugDraw || ( m_baseData.m_nameFilter.IsNull() || !StringWildcardCompare(m_baseData.m_nameFilter.GetCStr(), namePlus,true) );
										bool bChildrenShouldDraw = bDebugDraw && ms_bWildcardNameisRecursive && (m_baseData.m_nameFilter.IsNotNull() && strstr(m_baseData.m_nameFilter.GetCStr(),"*") != NULL);

										DebugDrawRecursiveVisitor visitor(m_baseData, bChildrenShouldDraw, matrx, object, extents, namePlus);
										object.VisitMembers(&visitor);

										// gotta do tail recursion to properly find the size of our box... in theory
										if( bDebugDraw )
										{
											m_baseData.m_bDrewSomething = true;

											matrx.AppendTranslation(extents.x, extents.y);
											Vector2 topLeft(0.0f, 0.0f);
											Vector2 bottomRight( topLeft.x + float(width.GetNumber()), topLeft.y + float(height.GetNumber()));
											Vector2 bottomLeft(topLeft.x, bottomRight.y);
											Vector2 topRight(bottomRight.x, topLeft.y);

											matrx.Transform(&topLeft.x, &topLeft.y);
											matrx.Transform(&bottomRight.x, &bottomRight.y);
											matrx.Transform(&bottomLeft.x, &bottomLeft.y);
											matrx.Transform(&topRight.x, &topRight.y);

											topLeft.Multiply(m_baseData.m_transform);
											bottomRight.Multiply(m_baseData.m_transform);
											bottomLeft.Multiply(m_baseData.m_transform);
											topRight.Multiply(m_baseData.m_transform);


											grcDebugDraw::Quad(topLeft, topRight, bottomRight, bottomLeft, m_baseData.m_color,false,false);

											if( ms_vTextOverride.IsNonZero() )
											{
												topLeft = ms_vTextOverride;
											}
										 
											topLeft.y -= 0.0125f; // so it doesn't overlap the boxes as much.
											grcDebugDraw::Text(topLeft, DD_ePCS_NormalisedZeroToOne, m_baseData.m_color,  ms_bFullNames?namePlus:name );

											topLeft.y += 0.0125f * (++m_textDrawnSoFar);
											if( ms_bShowMemberVariables && m_baseData.m_nameFilter.IsNotNull() )
											{
												if( ms_bDrawStandardVariables )
												{
													atString drawThis;

#define DRAW_VAR(varName, formattedStr)\
	drawThis = #varName;\
	drawThis += ":";\
	drawThis += formattedStr;\
	grcDebugDraw::Text(topLeft, DD_ePCS_NormalisedZeroToOne, m_baseData.m_color, drawThis );\
	topLeft.y += 0.0125f;

#define DRAW_VAR_NUMBER(varName, value)\
	{\
		char temp[32] = "";\
		Double fNum = value;\
		int iNum = int(fNum);\
		if( fNum == iNum )\
			formatf(temp, "%i", iNum);\
		else\
			formatf(temp, "%.4f", fNum);\
		DRAW_VAR(varName, temp);\
	}

#define DRAW_VAR_BOOL(varName, value)\
	DRAW_VAR(varName, value?"TRUE":"FALSE")
													DRAW_VAR_BOOL(_visible, info.GetVisible());
													DRAW_VAR_NUMBER(_x, info.GetX());
													DRAW_VAR_NUMBER(_y, info.GetY());
													DRAW_VAR_NUMBER(_xscale, info.GetXScale());
													DRAW_VAR_NUMBER(_yscale, info.GetYScale());
													DRAW_VAR_NUMBER(_alpha, info.GetAlpha());
												}


												DebugDrawMemberVisitor memvisitor(m_baseData, topLeft, 0);
												object.VisitMembers(&memvisitor);
											}
										}
									}
								}
							}
							
						}
						
					};

					GViewport vp;
					GFxMovieView& movie = pRawMovieView->GetMovieView();
					movie.GetViewport(&vp);

					Color32 colorAsHash( atStringHash(sm_ScaleformArray[iIndex].cFilename) );
					colorAsHash.SetAlpha(255); // no invalid alphas.

					
					Vector2 screenDims( 1.0f/GRCDEVICE.GetWidth(), 1.0f/GRCDEVICE.GetHeight());



					GFxValue root;
					movie.GetVariable(&root, "_root");

					bool bChildrenShouldDraw = ms_OutlineFilter.IsNotNull() && !StringWildcardCompare(ms_OutlineFilter.GetCStr(), sm_ScaleformArray[iIndex].cFilename, true);
					bool bDrewAChild = false;
					if( root.IsDefined() )
					{
						DebugDrawBaseData data(screenDims, colorAsHash, ms_OutlineFilter);
						GMatrix2D identity;
						//identity.AppendScaling(vp.Width, vp.Height);
						Vector2 movieSize(movie.GetMovieDef()->GetWidth(), movie.GetMovieDef()->GetHeight()); 
						static int iOrder[] = {3,5,0};

						for(int i=0; i < 3; ++i)
						{
							switch(iOrder[i])
							{
							case 1:	identity.AppendScaling(screenDims.x, screenDims.y);	break;
							case 2:	identity.PrependScaling(screenDims.x, screenDims.y);	break;
							case 3: identity.AppendScaling(vp.Width/movieSize.x, vp.Height/movieSize.y); break;
							case 4: identity.PrependScaling(vp.Width/movieSize.x, vp.Height/movieSize.y); break;

							case 5: identity.AppendTranslation(Float(vp.Left), Float(vp.Top));	break;
							case 6: identity.PrependTranslation(Float(vp.Left), Float(vp.Top));	break;
							}

						}
						

						
						Vector2 extents;
						DebugDrawRecursiveVisitor visitor(data, bChildrenShouldDraw, identity, root, extents, atString("_root"));
						root.VisitMembers(&visitor);

						bDrewAChild = data.m_bDrewSomething;
					}

					if( ms_OutlineFilter.IsNull() || bChildrenShouldDraw || bDrewAChild )
					{
						// draw a box around each movie's dimensions
						Vector2 topLeft(Float(vp.Left), Float(vp.Top));
						Vector2 bottomRight(Float(vp.Left+vp.Width), Float(vp.Top+vp.Height));
						topLeft.Multiply(screenDims);
						bottomRight.Multiply(screenDims);
						Vector2 bottomLeft(topLeft.x, bottomRight.y);
						Vector2 topRight(bottomRight.x, topLeft.y);
					
						grcDebugDraw::Quad(topLeft, topRight, bottomRight, bottomLeft, colorAsHash,false,false);

						Vector2 centered(topLeft+bottomRight);
						centered *= 0.5f;
						static float yOffset = 0.015f;
						centered.y += iIndex * yOffset;
						grcDebugDraw::Text(centered, DD_ePCS_NormalisedZeroToOne, colorAsHash, sm_ScaleformArray[iIndex].cFilename);
					}
				}
#endif
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RequestRenderMovie3D()
// PURPOSE: Renders scaleform movie of the id passed
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RequestRenderMovie3D(s32 iIndex, bool bRenderSolid)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::RequestRenderMovie3D can only be called on the UpdateThread!");
		return;
	}

	if (IsMovieActive(iIndex))
	{
		sm_ScaleformArray[iIndex].bIgnoreSuperWidescreenScaling.GetUpdateBuf() = true;
		sm_ScaleformArray[iIndex].bRender3D.GetUpdateBuf() = !bRenderSolid;
		sm_ScaleformArray[iIndex].bRender3DSolid.GetUpdateBuf() = bRenderSolid;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::SetMovie3DBrightness()
// PURPOSE: Set brightness for 3d movie
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::SetMovie3DBrightness(s32 iIndex, s32 iBrightness)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::SetMovie3DBrightness can only be called on the UpdateThread!");
		return;
	}

	if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].bRender3D.GetUpdateBuf())
	{
		sm_ScaleformArray[iIndex].iBrightness.GetUpdateBuf() = (u8)iBrightness;
	}
}

bool CScaleformMgr::SetMovieDisplayConfig(s32 iMovieId, eSCALEFORM_BASE_CLASS iBaseClass, SDC flags)
{
	if ( CScaleformMgr::BeginMethod(iMovieId, iBaseClass, "SET_DISPLAY_CONFIG") )
	{
		CScaleformMgr::AddParamInt( ACTIONSCRIPT_STAGE_SIZE_X );
		CScaleformMgr::AddParamInt( ACTIONSCRIPT_STAGE_SIZE_Y );

		// Safe Zone
		float fSafeZoneX[2], fSafeZoneY[2];

#if RSG_DURANGO
		// Fake safe zone on durango since movies on the loading screen all need to be adjusted to the same fake values
		if(IsAnyFlagSet(flags, SDC::UseFakeSafeZoneOnBootup) && CLoadingScreens::IsInitialLoadingScreenActive())
		{
			CHudTools::GetFakeLoadingScreenSafeZone(fSafeZoneX[0], fSafeZoneY[0], fSafeZoneX[1], fSafeZoneY[1]);
		}
		else
#endif
		if( IsAnyFlagSet(flags, SDC::UseUnAdjustedSafeZone) )
		{
			CHudTools::GetMinSafeZone(fSafeZoneX[0], fSafeZoneY[0], fSafeZoneX[1], fSafeZoneY[1]);
		}
		else
		{
			CHudTools::GetMinSafeZoneForScaleformMovies(fSafeZoneX[0], fSafeZoneY[0], fSafeZoneX[1], fSafeZoneY[1]);
		}

		// send as a 0-1 value
		CScaleformMgr::AddParamFloat(fSafeZoneY[0]);
		CScaleformMgr::AddParamFloat(fSafeZoneY[1]);
		CScaleformMgr::AddParamFloat(fSafeZoneX[0]);
		CScaleformMgr::AddParamFloat(fSafeZoneX[1]);

		CScaleformMgr::AddParamBool( IsAnyFlagSet(flags, SDC::ForceWidescreen) ? true : CHudTools::GetWideScreen() );

		CScaleformMgr::AddParamBool(!CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS));

		CScaleformMgr::AddParamBool(CText::IsAsianLanguage());

		// Real screen size
		CScaleformMgr::AddParamFloat( CHudTools::GetUIWidth() );
		CScaleformMgr::AddParamFloat( CHudTools::GetUIHeight() );

		CScaleformMgr::EndMethod();

		return true;
	}

	return false;
}

void CScaleformMgr::SetComplexObjectDisplayConfig( s32 objectId, eSCALEFORM_BASE_CLASS iBaseClass )
{
	if ( CScaleformMgr::BeginMethodOnComplexObject(objectId, iBaseClass, "SET_DISPLAY_CONFIG") )
	{
		CScaleformMgr::AddParamInt( ACTIONSCRIPT_STAGE_SIZE_X );
		CScaleformMgr::AddParamInt( ACTIONSCRIPT_STAGE_SIZE_Y );

		float fSafeZoneX[2], fSafeZoneY[2];

		CHudTools::GetMinSafeZoneForScaleformMovies(fSafeZoneX[0], fSafeZoneY[0], fSafeZoneX[1], fSafeZoneY[1]);
		CScaleformMgr::AddParamFloat(fSafeZoneY[0]);  // send as a 0-1 value
		CScaleformMgr::AddParamFloat(fSafeZoneY[1]);
		CScaleformMgr::AddParamFloat(fSafeZoneX[0]);
		CScaleformMgr::AddParamFloat(fSafeZoneX[1]);

		CScaleformMgr::AddParamBool( CHudTools::GetWideScreen() );
		// hijack what was hidef and make it swap cross/circle
		CScaleformMgr::AddParamBool(CText::IsAsianLanguage());

		// Real screen size
		CScaleformMgr::AddParamFloat( CHudTools::GetUIWidth() );
		CScaleformMgr::AddParamFloat( CHudTools::GetUIHeight() );

		CScaleformMgr::EndMethod();
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetMoviePos()
// PURPOSE: returns the pos of a movie
/////////////////////////////////////////////////////////////////////////////////////
Vector3 CScaleformMgr::GetMoviePos(s32 iIndex)
{
	if (IsMovieActive(iIndex))
	{
		if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		{
			return sm_ScaleformArray[iIndex].vPos.GetRenderBuf();
		}
		else
		{
			return sm_ScaleformArray[iIndex].vPos.GetUpdateBuf();
		}
	}

	return Vector3(0,0,0);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetMovieSize()
// PURPOSE: returns the size of a movie
/////////////////////////////////////////////////////////////////////////////////////
Vector2 CScaleformMgr::GetMovieSize(s32 iIndex)
{
	if (IsMovieActive(iIndex))
	{
		if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		{
			return sm_ScaleformArray[iIndex].vSize.GetRenderBuf();
		}
		else
		{
			return sm_ScaleformArray[iIndex].vSize.GetUpdateBuf();
		}
	}

	return Vector2(0,0);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetMovieFilename()
// PURPOSE: returns the filename of a movie
/////////////////////////////////////////////////////////////////////////////////////
char const *CScaleformMgr::GetMovieFilename(s32 const iIndex)
{
	char const * const c_result =  IsMovieIdInRange( iIndex ) ? sm_ScaleformArray[iIndex].cFilename : "nullptr";
    return c_result;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RenderWorldSpace()
// PURPOSE: Renders scaleform movie of the id passed
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RenderWorldSpace()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr::RenderWorldSpace can only be called on the RenderThread!");
		return;
	}

	bool bNeedEmissivePass = false;

	if (IsAnyMovieRender3D(bNeedEmissivePass) == false)
	{
		return;
	}

	RenderWorldSpaceDiffusePass();

	if (BANK_ONLY(ms_bOverrideEmissiveLevel ||) bNeedEmissivePass)
	{
		RenderWorldSpaceEmissivePass();
	}

	// restore default exit state
	grcStateBlock::SetBlendState(ms_defExitBlendState);
	grcStateBlock::SetDepthStencilState(ms_defExitDepthStencilState);
	grcStateBlock::SetRasterizerState(ms_defExitRasterizerState);

	// set back to false
	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].bRender3D.GetRenderBuf())
		{
			sm_ScaleformArray[iIndex].bRender3D.GetRenderBuf() = false;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RenderWorldSpaceSolid()
// PURPOSE: Renders scaleform movie of the id passed
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RenderWorldSpaceSolid()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr::RenderWorldSpaceSolid can only be called on the RenderThread!");
		return;
	}

	if (IsAnyMovieRender3DSolid() == false)
	{
		return;
	}

	RenderWorldSpaceSolidPass();

	// set back to false
	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].bRender3DSolid.GetRenderBuf())
		{
			sm_ScaleformArray[iIndex].bRender3DSolid.GetRenderBuf() = false;
		}
	}

	// restore default exit state
	grcStateBlock::SetBlendState(ms_defExitBlendState);
	grcStateBlock::SetDepthStencilState(ms_defExitDepthStencilState);
	grcStateBlock::SetRasterizerState(ms_defExitRasterizerState);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RenderWorldSpaceDiffusePass()
// PURPOSE: Renders scaleform movie of the id passed
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RenderWorldSpaceDiffusePass()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr:: RenderWorldSpaceDiffusePass can only be called on the RenderThread!");
		return;
	}

	// make sure global alpha is set to 1 (emissive pass reuses this global)
	CShaderLib::SetGlobalEmissiveScale(1.0f,true);
	CShaderLib::SetGlobalAlpha(1.0f);

#if __PPU
	GBuffer::UnlockTargets();
	GBuffer::LockSingleTarget(GBUFFER_RT_0, TRUE);
#endif

	grcStateBlock::SetBlendState(ms_defDiffuseBlendState);
	grcStateBlock::SetDepthStencilState(ms_defDiffuseDepthStencilState);
	grcStateBlock::SetRasterizerState(ms_defDiffuseRasterizerState);

	// don't let scaleform renderer override our blend state
	sfRendererBase& scRenderer = m_staticMovieMgr->GetRageRenderer();
	scRenderer.OverrideBlendState(true);

	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].bRender3D.GetRenderBuf())
		{
			sfScaleformMovieView *pRawMovieView = g_ScaleformStore.GetRawMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

			PIX_AUTO_TAGN(0, "Scaleform Movie %s", sm_ScaleformArray[iIndex].cFilename);
			DIAG_CONTEXT_MESSAGE("Drawing worldspace flash Movie %s", sm_ScaleformArray[iIndex].cFilename);
			sfScaleformManager::AutoSetCurrMovieName currMovie(sm_ScaleformArray[iIndex].cFilename);

			if (pRawMovieView)
			{
				Mat34V vScaleMat, vTransRotMat, vFullMat;
				Vec3V vScale = Vec3V(sm_ScaleformArray[iIndex].vWorldSize.GetRenderBuf().x, sm_ScaleformArray[iIndex].vWorldSize.GetRenderBuf().y, 1.0f);
				Vec3V vRot = Vec3V(sm_ScaleformArray[iIndex].vRot.GetRenderBuf().x, sm_ScaleformArray[iIndex].vRot.GetRenderBuf().y, sm_ScaleformArray[iIndex].vRot.GetRenderBuf().z);
				Vec3V vTrans = Vec3V(sm_ScaleformArray[iIndex].vPos.GetRenderBuf().x, sm_ScaleformArray[iIndex].vPos.GetRenderBuf().y, sm_ScaleformArray[iIndex].vPos.GetRenderBuf().z);

				Mat34VFromScale(vScaleMat, vScale);
				Mat34VFromEulersYXZ(vTransRotMat, vRot, vTrans);

				Transform(vFullMat, vTransRotMat, vScaleMat);
#if __BANK
				if (ms_bShowOutlineFor3DMovie == true)
				{
					grcWorldMtx(vFullMat);

					grcColor3f(0.0f, 1.0f, 0.0f);
					grcBegin(drawLineStrip, 6);
					grcVertex3f(0.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 1.0f, 0.0f);
					grcVertex3f(1.0f, 1.0f, 0.0f);
					grcVertex3f(1.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 0.0f, 1.0f);
					grcEnd();
				}
				else
#endif
				{
					grcViewport::SetCurrentWorldMtx(vFullMat);
				}

				pRawMovieView->DrawInWorldSpace(*CScaleformMgr::GetMovieMgr());
			}
		}
	}

	// return control of blend state to scaleform
	scRenderer.OverrideBlendState(false);

#if __PPU
	GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
	GBuffer::LockTargets();
#endif

}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RenderWorldSpaceEmissivePass()
// PURPOSE: Renders scaleform movie of the id passed
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RenderWorldSpaceEmissivePass()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr:: RenderWorldSpaceEmissivePass can only be called on the RenderThread!");
		return;
	}

#if __PPU
	GBuffer::UnlockTargets();
	GBuffer::LockSingleTarget(GBUFFER_RT_3, TRUE);
#endif

	grcStateBlock::SetBlendState(ms_defEmissiveBlendState);
	grcStateBlock::SetDepthStencilState(ms_defEmissiveDepthStencilState);
	grcStateBlock::SetRasterizerState(ms_defEmissiveRasterizerState);

	// don't let scaleform renderer override our blend state
	sfRendererBase& scRenderer = m_staticMovieMgr->GetRageRenderer();
	scRenderer.OverrideBlendState(true);

	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].bRender3D.GetRenderBuf())
		{
			sfScaleformMovieView *pRawMovieView = g_ScaleformStore.GetRawMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

			PIX_AUTO_TAGN(0, "Scaleform Movie %s", sm_ScaleformArray[iIndex].cFilename);
			DIAG_CONTEXT_MESSAGE("Drawing worldspace flash Movie %s", sm_ScaleformArray[iIndex].cFilename);
			sfScaleformManager::AutoSetCurrMovieName currMovie(sm_ScaleformArray[iIndex].cFilename);

			if (pRawMovieView)
			{
				Mat34V vScaleMat, vTransRotMat, vFullMat;
				Vec3V vScale = Vec3V(sm_ScaleformArray[iIndex].vWorldSize.GetRenderBuf().x, sm_ScaleformArray[iIndex].vWorldSize.GetRenderBuf().y, 1.0f);
				Vec3V vRot = Vec3V(sm_ScaleformArray[iIndex].vRot.GetRenderBuf().x, sm_ScaleformArray[iIndex].vRot.GetRenderBuf().y, sm_ScaleformArray[iIndex].vRot.GetRenderBuf().z);
				Vec3V vTrans = Vec3V(sm_ScaleformArray[iIndex].vPos.GetRenderBuf().x, sm_ScaleformArray[iIndex].vPos.GetRenderBuf().y, sm_ScaleformArray[iIndex].vPos.GetRenderBuf().z);

				Mat34VFromScale(vScaleMat, vScale);
				Mat34VFromEulersYXZ(vTransRotMat, vRot, vTrans);

				Transform(vFullMat, vTransRotMat, vScaleMat);
#if __BANK
				if (ms_bShowOutlineFor3DMovie == true)
				{
					grcWorldMtx(vFullMat);

					grcColor3f(0.0f, 1.0f, 0.0f);
					grcBegin(drawLineStrip, 6);
					grcVertex3f(0.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 1.0f, 0.0f);
					grcVertex3f(1.0f, 1.0f, 0.0f);
					grcVertex3f(1.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 0.0f, 1.0f);
					grcEnd();
				}
				else
#endif
				{
					grcViewport::SetCurrentWorldMtx(vFullMat);
				}

#if __BANK
				if (ms_bOverrideEmissiveLevel)
				{
					CShaderLib::SetGlobalEmissiveScale(ms_emissiveLevel);
					grcStateBlock::SetBlendState(ms_defEmissiveBlendState);
				}
				else
#endif
				{
					// don't let emissive value go to 1
					CShaderLib::SetGlobalEmissiveScale((float)sm_ScaleformArray[iIndex].iBrightness.GetRenderBuf() / 256.0f);
					grcStateBlock::SetBlendState(ms_defEmissiveBlendState);

					// reset brightness
					sm_ScaleformArray[iIndex].iBrightness.GetRenderBuf() = 0;
				}

				pRawMovieView->DrawInWorldSpace(*CScaleformMgr::GetMovieMgr());
			}
		}
	}

	// return control of blend state to scaleform
	scRenderer.OverrideBlendState(false);
#if __PPU
	GBuffer::UnlockSingleTarget(GBUFFER_RT_0);
	GBuffer::LockTargets();
#endif
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RenderWorldSpaceSolidPass()
// PURPOSE: Renders scaleform movie of the id passed
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RenderWorldSpaceSolidPass()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr:: RenderWorldSpaceSolidPass can only be called on the RenderThread!");
		return;
	}

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CShaderLib::SetGlobalEmissiveScale(1.0f);
	CShaderLib::SetGlobalAlpha(1.0f);

	grcStateBlock::SetBlendState(ms_defDiffuseBlendState);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_LessEqual_Invert);
	grcStateBlock::SetRasterizerState(ms_defDiffuseRasterizerState);

	// sort the movies
	ms_Sorted3DMovies.Clear();
	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].bRender3DSolid.GetRenderBuf())
		{
			CSorted3DMovie movie;
			movie.m_iMovieId = iIndex;
			movie.m_fSqrDist = ms_LastCameraPos.Dist2(sm_ScaleformArray[iIndex].vPos.GetRenderBuf());
			ms_Sorted3DMovies.InsertSorted(movie);
		}
	}

	// render them back to front
	CSorted3DMovieNode* pLastLink = ms_Sorted3DMovies.GetLast()->GetPrevious();
	while(pLastLink != ms_Sorted3DMovies.GetFirst())
	{
		CSorted3DMovie* pCurEntry = &(pLastLink->item);
		pLastLink = pLastLink->GetPrevious();

		s32 iIndex = pCurEntry->m_iMovieId;

		if (iIndex != -1)
		{
			sfScaleformMovieView *pRawMovieView = g_ScaleformStore.GetRawMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

			PIX_AUTO_TAGN(0, "Scaleform Movie %s", sm_ScaleformArray[iIndex].cFilename);
			DIAG_CONTEXT_MESSAGE("Drawing worldspace flash Movie %s", sm_ScaleformArray[iIndex].cFilename);
			sfScaleformManager::AutoSetCurrMovieName currMovie(sm_ScaleformArray[iIndex].cFilename);

			if (pRawMovieView)
			{
				// rotate about center
				Vec3V vOffset(-0.5f, -0.5f, 0.0f);
				Mat34V vOffsetMat;
				Mat34VFromTranslation(vOffsetMat, vOffset);

				Vec3V vScale(sm_ScaleformArray[iIndex].vWorldSize.GetRenderBuf().x, sm_ScaleformArray[iIndex].vWorldSize.GetRenderBuf().y, 1.0f);
				Mat34V vScaleMat;
				Mat34VFromScale(vScaleMat, vScale);

				Vec3V vPos(sm_ScaleformArray[iIndex].vPos.GetRenderBuf().x, sm_ScaleformArray[iIndex].vPos.GetRenderBuf().y, sm_ScaleformArray[iIndex].vPos.GetRenderBuf().z);
				Vec3V vRot(sm_ScaleformArray[iIndex].vRot.GetRenderBuf().x+DEG2RAD(180.0f), sm_ScaleformArray[iIndex].vRot.GetRenderBuf().y, sm_ScaleformArray[iIndex].vRot.GetRenderBuf().z);
				Mat34V vRotPosMat;
				Mat34VFromEulersYXZ(vRotPosMat, vRot, vPos);

				Mat34V vFinalTransMat;
				Transform(vFinalTransMat, vScaleMat, vOffsetMat);
				Transform(vFinalTransMat, vRotPosMat, vFinalTransMat);

#if __BANK
				if (ms_bShowOutlineFor3DMovie == true)
				{
					grcWorldMtx(vFinalTransMat);

					grcColor3f(0.0f, 1.0f, 0.0f);
					grcBegin(drawLineStrip, 6);
					grcVertex3f(0.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 1.0f, 0.0f);
					grcVertex3f(1.0f, 1.0f, 0.0f);
					grcVertex3f(1.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 0.0f, 0.0f);
					grcVertex3f(0.0f, 0.0f, 1.0f);
					grcEnd();
				}
				else
#endif
				{
					grcViewport::SetCurrentWorldMtx(vFinalTransMat);
				}

				pRawMovieView->DrawInWorldSpace(*CScaleformMgr::GetMovieMgr());
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RestartMovie()
// PURPOSE: restart is SHIT and uses more memory to do it.
//			So we remove and re-create the movieview instead
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::RestartMovie(s32 iIndex, bool bForce, bool bQuick)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr:: RestartMovie can only be called on the UpdateThread!");
		return false;
	}

	if (sfVerifyf(IsMovieIdInRange(iIndex), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iIndex))
	{
		if (HasMovieGotAnyState(iIndex))
		{
	#if __SCALEFORM_CRITICAL_SECTIONS
			SYS_CS_SYNC(gs_ScaleformMovieCS[iIndex]);
	#endif // __SCALEFORM_CRITICAL_SECTIONS

			GFxMovieView *pMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

			if (pMovieView)  // we have a movieview
			{
				if (bQuick)
				{
					pMovieView->Restart();  // restart is SHIT and uses more memory to do it... but its faster than the below (fixes 1537754)

					sfDebugf3("ScaleformMgr: Movie %s (%d) has been 'quick' restarted", sm_ScaleformArray[iIndex].cFilename, iIndex);

					return true;
				}

				if ( (!CScaleformMgr::DoesMovieHaveInvokesOrCallbacksPending(iIndex)) || (bForce) )
				{
					// ...  So we remove and re-create the movieview instead, thus clearing all the memory fully - tests show restart is not needed beforehand
					g_ScaleformStore.RemoveMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));
					g_ScaleformStore.CreateMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

					ChangeMovieParams(iIndex, sm_ScaleformArray[iIndex].vOriginalPos.GetUpdateBuf(), sm_ScaleformArray[iIndex].vOriginalSize.GetUpdateBuf(), sm_ScaleformArray[iIndex].vWorldSize.GetUpdateBuf(), Vector3(ORIGIN), sm_ScaleformArray[iIndex].eScaleMode.GetUpdateBuf());

					GFxMovieView *pNewMovieView = g_ScaleformStore.GetMovieView(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

					if (pNewMovieView)
					{
						pNewMovieView->SetBackgroundAlpha(0);
					}

					sfDebugf3("ScaleformMgr: Movie %s (%d) has been 'full' restarted", sm_ScaleformArray[iIndex].cFilename, iIndex);

					return true;
				}
				else
				{
					sfDebugf3("ScaleformMgr: Movie %s (%d) has NOT been restarted because there are buffered invokes/callbacks pending", sm_ScaleformArray[iIndex].cFilename, iIndex);
				}
			}
			else
			{
				sfDebugf3("ScaleformMgr: Movie %s (%d) has NOT been restarted because it has no movieview", sm_ScaleformArray[iIndex].cFilename, iIndex);
			}
		}
		else
		{
			sfDebugf3("ScaleformMgr: Movie %s (%d) has NOT been restarted because it is not in a valid state", sm_ScaleformArray[iIndex].cFilename, iIndex);
		}
	}

	return false;
}

void CScaleformMgr::OverrideOriginalMoviePosition(int iMovieID, Vector2 &vPos)
{
	if (!sfVerifyf(IsMovieIdInRange(iMovieID), "ScaleformMgr: Movie id %d is not in range of a valid Scaleform ID", iMovieID))
		return;

	Vector3 vNewPosition(vPos.x, vPos.y, 0.0f);
	ScaleformMovieStruct &movie = sm_ScaleformArray[iMovieID];

	movie.vOriginalPos.Set(vNewPosition);
}

ScaleformMovieTxd& ScaleformMovieStruct::CreateNewTxdRequest(strLocalIndex iTxdId, const char* pTxdString, const char* uniqueRef, bool reportSuccessToAS)
{
#if __SF_STATS
	if (allRequestedTxds.Find(iTxdId.Get()) < 0)
	{
		allRequestedTxds.PushAndGrow(iTxdId.Get());
	}
#endif

	// Make sure we don't have a request pending for this movie already?

	ScaleformMovieTxd newTxdItem;

	safecpy(newTxdItem.cTxdName, pTxdString, NELEM(newTxdItem.cTxdName));
	safecpy(newTxdItem.cUniqueRefString, uniqueRef);
	newTxdItem.bReportSuccessToActionScript = reportSuccessToAS;
	newTxdItem.iTxdId = iTxdId;
	OUTPUT_ONLY(newTxdItem.iSpamCounter = 30);
	if (g_TxdStore.IsObjectInImage(iTxdId))
	{
		newTxdItem.streamingRequest.Request(iTxdId, g_TxdStore.GetStreamingModuleId(), MOVIE_STREAMING_FLAGS);
	}
	else
	{
		newTxdItem.streamingRequest.Release(); // Just in case
		g_TxdStore.AddRef(strLocalIndex(iTxdId), REF_OTHER);
	}

	PrintTxdRef("CreateNewRequest", iTxdId);

	requestedTxds.PushAndGrow(newTxdItem);
	return requestedTxds.Top();
}


void CScaleformMgr::CallAddTxdRefResponse( s32 iIndex, const char * pTxdString, const char * pUniqueString, bool success )
{
	eSCALEFORM_BASE_CLASS iBaseClass = SF_BASE_CLASS_GENERIC;
	if (iIndex == CNewHud::GetContainerMovieId())
	{
		iBaseClass = SF_BASE_CLASS_HUD;
	}

	if (CScaleformMgr::BeginMethod(iIndex, iBaseClass, "ADD_TXD_REF_RESPONSE"))
	{
		CScaleformMgr::AddParamString(pTxdString);
		CScaleformMgr::AddParamString(pUniqueString);
		CScaleformMgr::AddParamBool(success);
		CScaleformMgr::EndMethod();
	}

	if (CScaleformMgr::BeginMethod(iIndex, iBaseClass, "GET_TXD_RESPONSE"))  // to be removed once AS moves to ADD_TXD_REF_RESPONSE
	{
		CScaleformMgr::AddParamString(pTxdString);
		CScaleformMgr::AddParamString(pUniqueString);
		CScaleformMgr::AddParamBool(success);
		CScaleformMgr::EndMethod();
	}
}

// The intention here is to add a ref to a dictionary that's already loaded, otherwise fail.

void CScaleformMgr::AddTxdRef(s32 iIndex, const char *pTxdString, const char *pUniqueString, bool bGetTxd, bool bAddPendingRefToLoaded)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::AddTxdRef can only be called on the UpdateThread!");
		return;
	}

	// ensure we have some id of some kind to either add the txd ref or to report some errors:
	if (!sfVerifyf(IsMovieIdInRange(iIndex), "Texture '%s' requested on an inactive movie (%d) that has been deleted", pTxdString, iIndex))
	{
		return;
	}

	if (!sfVerifyf(pTxdString, "NULL txd requested on movie %s", sm_ScaleformArray[iIndex].cFilename))
	{
		return;
	}

	strLocalIndex iTxdId = g_TxdStore.FindSlot(pTxdString);

	PrintTxdRef("ADD_TXD_REF", iTxdId);

	bool bIsTxdReady = false;

	if (iTxdId.Get() >= 0)
	{
		// If the TXD is part of an image, check whether it's loaded
		if (g_TxdStore.IsObjectInImage(iTxdId))
		{
			bIsTxdReady = g_TxdStore.HasObjectLoaded(iTxdId);
		}
		// Otherwise, check whether it exists (e.g.: textures downloaded from the cloud)
		else
		{
			bIsTxdReady = (g_TxdStore.Get(iTxdId) != NULL && g_TxdStore.GetNumRefs(iTxdId) > 0);
		}
	}

	if (iTxdId.Get() >= 0 && bIsTxdReady && IsMovieUpdateable(iIndex))
	{
		if (bGetTxd)
		{
			// Everything looks good, add a new temporary reference to the movie
			sfDebugf1("ADD_TXD_REF called on loaded Scaleform movie %s (%d) - txd '%s' ", sm_ScaleformArray[iIndex].cFilename, iIndex, pTxdString);

			// Create a new request just to make sure the txd sticks around until we use a texture from the dict
			sm_ScaleformArray[iIndex].CreateNewTxdRequest(iTxdId, pTxdString, pUniqueString, false);
			CallAddTxdRefResponse(iIndex, pTxdString, pUniqueString, true);
		}
		else
		{
			sfDebugf1("Text IMG tagged passed into Scaleform movie %s (%d) - txd '%s'", sm_ScaleformArray[iIndex].cFilename, iIndex, pTxdString);
			if (bAddPendingRefToLoaded)
			{
				sm_ScaleformArray[iIndex].CreateNewTxdRequest(iTxdId, pTxdString, pUniqueString, false);
			}
		}
	}
	else
	{
		if (bGetTxd)
		{
			sfDebugf1("ADD_TXD_REF called on Scaleform movie %s (%d) with TXD '%s' - but it has failed and has not added a ref. Response will return FALSE and Actionscript should not render this!", sm_ScaleformArray[iIndex].cFilename, iIndex, pTxdString);
			CallAddTxdRefResponse(iIndex, pTxdString, pUniqueString, false);
		}
	}
}


void CScaleformMgr::RemoveTxdRef(s32 iIndex, const char *pTxdString, bool UNUSED_PARAM(bFromActionscript))
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "ScaleformMgr::RemoveTxdRef can only be called on the UpdateThread!");
		return;
	}

	// ensure we have some id of some kind to either add the txd ref or to report some errors:
	if (!IsMovieIdInRange(iIndex))
	{
		if (pTxdString)
		{
			sfAssertf(0, "Texture '%s' called via RemoveTxdRef on an inactive movie (%d) that has been deleted", pTxdString, iIndex);
		}
		else
		{
			sfAssertf(0, "RemoveTxdRef called without any valid properties!");
		}

		return;
	}

	if (iIndex != -1 && pTxdString)
	{
		strLocalIndex iTxdId = g_TxdStore.FindSlot(pTxdString);
		// Check to see if we have a request pending, remove one if so
		for (s32 iTxdReqIdx = 0; iTxdReqIdx < sm_ScaleformArray[iIndex].requestedTxds.GetCount(); ++iTxdReqIdx)
		{
			ScaleformMovieTxd& txdReq = sm_ScaleformArray[iIndex].requestedTxds[iTxdReqIdx];
			if (txdReq.iTxdId == iTxdId)
			{
				PrintTxdRef("REMOVE_TXD_REF Cancelling", iTxdId);
				sfDebugf1("ScaleformMgr: Scaleform movie %s (%d) is no longer referencing txd '%s' - removing from additionTxd array", sm_ScaleformArray[iIndex].cFilename, iIndex, g_TxdStore.GetName(iTxdId));
				sm_ScaleformArray[iIndex].requestedTxds.DeleteFast(iTxdReqIdx);
				PrintTxdRef("REMOVE_TXD_REF Cancelled", iTxdId);
				break; // Just cancel one, in case multiple were queued up
			}
		}
	}
}


bool ScaleformMovieTxd::AlreadyLoaded()
{
	if (g_TxdStore.IsObjectInImage(iTxdId))
	{
		return streamingRequest.HasLoaded();
	}
	else
	{
		return g_TxdStore.Get(iTxdId) != NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CheckIncomingFunctions()
// PURPOSE: listens for events sent from ActionScript
//			Generic methods that are available on all movies, should go in here
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	// public function PLAY_SOUND(String)
	if (methodName == ATSTRINGHASH("PLAY_SOUND",0xb6e1917f))
	{
		if (uiVerifyf(args[1].IsString() && args[2].IsString(), "PLAY_SOUND params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			g_FrontendAudioEntity.PlaySound((char*)args[1].GetString(),(char*)args[2].GetString());
		}
		return;
	}

	// public function VIBRATE_PAD(int Duration, float Intensity, int time_to_ignore_next)
	if (methodName == ATSTRINGHASH("VIBRATE_PAD",0x81e7b69f))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber() && args[3].IsNumber(), "VIBRATE_PAD params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3])))
		{
			CControlMgr::StartPlayerPadShakeByIntensity((s32)args[1].GetNumber(),(float)args[2].GetNumber(), (s32)args[3].GetNumber());
		}

		return;
	}

	// public function FORCE_GARBAGE_COLLECTION(string moviename)
	if (methodName == ATSTRINGHASH("FORCE_GARBAGE_COLLECTION",0xddf62377))
	{
		if (uiVerifyf(args[1].IsString(), "FORCE_GARBAGE_COLLECTION params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			const char* cMovieString = args[1].GetString();

			s32 iMovieIdToCallOn = CScaleformMgr::FindMovieByFilename(cMovieString);

			if (iMovieIdToCallOn != -1)
			{
				ForceCollectGarbage(iMovieIdToCallOn);
			}
		}

		return;
	}

	// public function FORCE_GARBAGE_COLLECTION(string moviename)
	if (methodName == ATSTRINGHASH("FORCE_RESTART_MOVIE",0x2606e81b))
	{
		if (uiVerifyf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "FORCE_RESTART_MOVIE can only be called on the Update Thread"))
		{
			if (uiVerifyf((args[1].IsString()), "FORCE_RESTART_MOVIE params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
			{
				const char* cMovieString = args[1].GetString();

				s32 iMovieIdToCallOn = CScaleformMgr::FindMovieByFilename(cMovieString);

				if (uiVerifyf(IsMovieIdInRange(iMovieIdToCallOn), "FORCE_RESTART_MOVIE - Movie %s is not active", cMovieString))
				{
					// lock the movie as we will be restarting the movie here and also invoking on it instantly with the revised GFXVALUE.

					gRenderThreadInterface.Flush();

					sfDebugf3("ScaleformMgr: Movie %s (%d) is about to be restarted by Actionscript", sm_ScaleformArray[iMovieIdToCallOn].cFilename, iMovieIdToCallOn);

					bool bRestartSuccessful = RestartMovie(iMovieIdToCallOn, true, false);

					eSCALEFORM_BASE_CLASS iBaseClass;

					if (iMovieIdToCallOn == CNewHud::GetContainerMovieId())
					{
						iBaseClass = SF_BASE_CLASS_HUD;
					}
					else
					{
						iBaseClass = SF_BASE_CLASS_GENERIC;
					}

					if (bRestartSuccessful)
					{
						if (!args[2].IsUndefined())
						{
							if (BeginMethod(iMovieIdToCallOn, iBaseClass, "RESTART_MOVIE"))
							{
								AddParamGfxValue(args[2]);
								EndMethod(true);
							}

							sfDebugf3("ScaleformMgr: Movie %s (%d) has been sent response values to actionscript", sm_ScaleformArray[iMovieIdToCallOn].cFilename, iMovieIdToCallOn);
						}
					}
					else
					{
						if (BeginMethod(iMovieIdToCallOn, iBaseClass, "RESTART_MOVIE_FAILED"))
						{
							EndMethod(true);
						}

						sfDebugf3("ScaleformMgr: Movie %s (%d) failed to restart and actionscript told via RESTART_MOVIE_FAILED", sm_ScaleformArray[iMovieIdToCallOn].cFilename, iMovieIdToCallOn);
					}
				}
			}
		}

		return;
	}

	// public function REMOVE_TXD_REF
	if ( (methodName == ATSTRINGHASH("REMOVE_TXD_REF",0xaf7155e6)) || (methodName == ATSTRINGHASH("REMOVE_TXD",0x87a1cc5b)) )
	{
		if (uiVerifyf(args[1].IsString() && args[2].IsString(), "REMOVE_TXD_REF params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			const char* cMovieString = args[1].GetString();
			const char* cTxdString = args[2].GetString();

			s32 iMovieIdToCallOn = CScaleformMgr::FindMovieByFilename(cMovieString);

			if (IsMovieIdInRange(iMovieIdToCallOn))
			{
				RemoveTxdRef(iMovieIdToCallOn, cTxdString, true);
			}
		}

		return;

	}

	// public function REQUEST_TXD_AND_ADD_REF
	if ( (methodName == ATSTRINGHASH("REQUEST_TXD_AND_ADD_REF",0x58700f8f)) || (methodName == ATSTRINGHASH("REQUEST_TXD",0x4caf93aa)) )
	{
		if (uiVerifyf(args[1].IsString() && args[2].IsString(), "REQUEST_TXD_AND_ADD_REF params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			const char* cMovieString = args[1].GetString();
			const char* cTxdString = args[2].GetString();
			const char* cNewUniqueRefString = args[3].IsString() ? args[3].GetString() : "";
			bool bReturnAlreadyLoadedBackToActionScript = false;

			if (args[4].IsBool())
			{
				bReturnAlreadyLoadedBackToActionScript = args[4].GetBool();
			}

			s32 iMovieIdToCallOn = CScaleformMgr::FindMovieByFilename(cMovieString);

			if (IsMovieIdInRange(iMovieIdToCallOn))
			{
				strLocalIndex iTxdId = g_TxdStore.FindSlot(cTxdString);

				// 1st check to ensure its not already there:
				if (iTxdId != -1)
				{
					ScaleformMovieTxd& newTxdItem = sm_ScaleformArray[iMovieIdToCallOn].CreateNewTxdRequest(iTxdId, cTxdString, cNewUniqueRefString, true);
					PrintTxdRef("Requested", iTxdId);

					if (newTxdItem.AlreadyLoaded())
					{
						sfDebugf1("REQUEST_TXD_AND_ADD_REF called on Scaleform movie %s (%d, %s) (already loaded) - txd '%s' (unique string: %s)", sm_ScaleformArray[iMovieIdToCallOn].cFilename, iMovieIdToCallOn, cMovieString, cTxdString, cNewUniqueRefString);

						if (bReturnAlreadyLoadedBackToActionScript)
						{
							eSCALEFORM_BASE_CLASS iBaseClass = SF_BASE_CLASS_GENERIC;
							if (iMovieIdToCallOn == CNewHud::GetContainerMovieId())
							{
								iBaseClass = SF_BASE_CLASS_HUD;
							}

							if (CScaleformMgr::BeginMethod(iMovieIdToCallOn, iBaseClass, "TXD_ALREADY_LOADED"))
							{
								CScaleformMgr::AddParamString(cTxdString);
								CScaleformMgr::AddParamString(cNewUniqueRefString);
								CScaleformMgr::EndMethod();
							}
						}

						return;
					}
					else
					{
						sfDebugf1("REQUEST_TXD_AND_ADD_REF called on Scaleform movie %s (%d, %s) (streaming requested) (unique string: %s)", sm_ScaleformArray[iMovieIdToCallOn].cFilename, iMovieIdToCallOn, cMovieString, cNewUniqueRefString);
					}
				}
				else
				{
					sfAssertf(0, "REQUEST_TXD_AND_ADD_REF movie %s has requested txd '%s' that doesnt exist (unique string: %s)", cMovieString, cTxdString, cNewUniqueRefString);
					return;
				}
			}
			else
			{
				sfAssertf(0, "REQUEST_TXD_AND_ADD_REF movie %s not active", cMovieString);
				return;
			}
		}

		return;
	}

	// public function ADD_TXD_REF
	if ( (methodName == ATSTRINGHASH("ADD_TXD_REF",0x17cd8c55)) || (methodName == ATSTRINGHASH("GET_TXD",0x968fb1bb)) )
	{
		if (uiVerifyf(args[1].IsString() && args[2].IsString(), "ADD_TXD_REF params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			const char* cMovieString = args[1].GetString();
			const char* cTxdString = args[2].GetString();
			const char* cNewUniqueRefString = args[3].IsString() ? args[3].GetString() : "";

			AddTxdRef(CScaleformMgr::FindMovieByFilename(cMovieString, true), cTxdString, cNewUniqueRefString, true, true);
		}

		return;
	}

	// public function CALL_METHOD_ON_MOVIE
	if (methodName == ATSTRINGHASH("CALL_METHOD_ON_MOVIE",0xc7fa6e1b))
	{
		if (uiVerifyf(args[1].IsString() && args[2].IsString() && args[3].IsArray(), "CALL_METHOD_ON_MOVIE params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3])))
		{
			//sfDisplayf("cMovieString = %s", args[1].GetString());

			const char* cMovieString = args[1].GetString();
			const char* cMethodString = args[2].GetString();

			s32 iMovieIdToCallOn = CScaleformMgr::FindMovieByFilename(cMovieString);

			if (iMovieIdToCallOn != -1)
			{
				if (CScaleformMgr::GetMovieView(iMovieIdToCallOn))
				{
					if (CScaleformMgr::BeginMethod(iMovieIdToCallOn, SF_BASE_CLASS_PAUSEMENU, cMethodString))
					{
						for (s32 iArgCount = 0; iArgCount < args[3].GetArraySize(); iArgCount++)
						{
							GFxValue localGfxValue;

							if (args[3].GetElement(iArgCount, &localGfxValue))
							{
								if (localGfxValue.IsBool())
								{
									CScaleformMgr::AddParamBool(localGfxValue.GetBool());
								}
								else
								if (localGfxValue.IsNumber())
								{
									CScaleformMgr::AddParamFloat((float)localGfxValue.GetNumber());
								}
								else
								if (localGfxValue.IsString())
								{
									CScaleformMgr::AddParamString(localGfxValue.GetString());
								}
								else
								{
									sfAssertf(0, "CALL_METHOD_ON_MOVIE movie %s::%s has invalid array element", cMovieString, cMethodString);
								}
							}
						}

						CScaleformMgr::EndMethod();
					}
				}
				else
				{
					sfAssertf(0, "CALL_METHOD_ON_MOVIE movie %s::%s does not have a movie view", cMovieString, cMethodString);
				}
			}
		}

		return;
	}

	// public function SHOULD_DISPLAY_BOX_BACKGROUNDS(String)
	if (methodName == ATSTRINGHASH("SHOULD_DISPLAY_BOX_BACKGROUNDS",0x91b5527d))
	{
		if (uiVerifyf(!args[1].IsUndefined(), "SHOULD_DISPLAY_BOX_BACKGROUNDS params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			GFxValue DisplayObj = args[1];

			if (DisplayObj.IsDisplayObject())
			{
				if (DisplayObj.GFxValue::HasMember("isDaytime"))
				{
					GFxValue boolValue;
					DisplayObj.GFxValue::GetMember("isDaytime", &boolValue);

					bool bBackgroundNeeded = true;

					CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
					if (pPlayerPed && (!pPlayerPed->GetPortalTracker()->IsInsideInterior()) && (CClock::GetHour() < 6 || CClock::GetHour() > 20))
					{
						bBackgroundNeeded = false;  // no background
					}

					boolValue.SetBoolean(bBackgroundNeeded);

					DisplayObj.SetMember("isDaytime", boolValue);
				}
				else
				{
					Assertf(0, "SHOULD_DISPLAY_BOX_BACKGROUNDS called without a valid object");
				}
			}
			else
			{
				Assertf(0, "SHOULD_DISPLAY_BOX_BACKGROUNDS called without a valid object");
			}
		}

		return;
	}

	// public function IS_AMERICAN_BUILD
	if (methodName == ATSTRINGHASH("IS_AMERICAN_BUILD",0xc11edfe6))
	{
		if (sfVerifyf(!args[1].IsUndefined(), "IS_AMERICAN_BUILD params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			GFxValue buildDisplayObj = args[1];

			if (buildDisplayObj.IsObject())
			{
				if (buildDisplayObj.HasMember("isAmerican"))
				{
					GFxValue gfxValue;
					buildDisplayObj.GetMember("isAmerican", &gfxValue);

	#if __XENON
					gfxValue.SetBoolean(XGetLocale() == XC_LOCALE_UNITED_STATES);
	#else
					gfxValue.SetBoolean(sysAppContent::IsAmericanBuild());
	#endif

					buildDisplayObj.SetMember("isAmerican", gfxValue);
				}
				else
				{
					Assertf(0, "IS_AMERICAN_BUILD called with object that doesnt have isAmerican member");
				}
			}
			else
			{
				Assertf(0, "IS_AMERICAN_BUILD called with an invalid object");
			}
		}

		return;
	}

	// public function SET_TEXT_WITH_TRANSLATION(String, GFxValue)
	if (methodName == ATSTRINGHASH("SET_TEXT_WITH_TRANSLATION",0x44f1c660))
	{
		if (uiVerifyf(args[1].IsString() && (!args[2].IsUndefined()), "SET_TEXT_WITH_TRANSLATION params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			if (args[1].GetString())
			{
				char *pGxtText = TheText.Get((char*)args[1].GetString());

				char cHtmlFormattedTextString[MAX_CHARS_FOR_TEXT_STRING_IN_PARAM];
				CTextConversion::TextToHtml(pGxtText, cHtmlFormattedTextString, NELEM(cHtmlFormattedTextString));

				GFxValue textField = args[2];

				bool bUseHtml = false;

				if ( (!args[3].IsUndefined()) && (args[3].IsBool()) )
				{
					bUseHtml = args[3].GetBool();
				}

				if (textField.IsDisplayObject())
				{
					if (bUseHtml)
					{
						textField.SetTextHTML(cHtmlFormattedTextString);
					}
					else
					{
						textField.SetText(cHtmlFormattedTextString);
					}
				}
				else
				{
					Assertf(0, "SET_TEXT_WITH_TRANSLATION called with %s but without a valid text field", (char*)args[1].GetString());
				}
			}
			else
			{
				Assertf(0, "SET_TEXT_WITH_TRANSLATION called without a valid text token ID");
			}
		}

		return;
	}

	if (methodName == ATSTRINGHASH("SET_FORMATTED_TEXT_WITH_ICONS",0x75a08773))
	{
		Verifyf(CMovieClipText::SetFormattedTextWithIcons(args), "SET_FORMATTED_TEXT_WITH_ICONS failed!!");
		return;
	}

	if (methodName == ATSTRINGHASH("SET_FORMATTED_TEXT_WITH_ICONS_EXPLICIT",0x92a219f9) )
	{
		Verifyf(CMovieClipText::SetFormattedTextWithIcons(args, true), "SET_FORMATTED_TEXT_WITH_ICONS_EXPLICIT failed!!");
		return;
	}

	// public function GET_HUD_COLOUR
	if (methodName == ATSTRINGHASH("GET_HUD_COLOUR",0x63f66a0b))
	{
		if (uiVerifyf(args[1].IsNumber() && (!args[2].IsUndefined()), "GET_HUD_COLOUR params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			if (args[1].IsNumber())
			{
				eHUD_COLOURS iHudColour = (eHUD_COLOURS)(s32)args[1].GetNumber();

				if (iHudColour > HUD_COLOUR_INVALID && iHudColour < HUD_COLOUR_MAX_COLOURS)
				{
					GFxValue colourDisplayObj = args[2];

					if (colourDisplayObj.IsObject() &&
						colourDisplayObj.HasMember("r") &&
						colourDisplayObj.HasMember("g") &&
						colourDisplayObj.HasMember("b") &&
						colourDisplayObj.HasMember("a"))
					{
						GFxValue colourValue[4];
						colourDisplayObj.GetMember("r", &colourValue[0]);
						colourDisplayObj.GetMember("g", &colourValue[1]);
						colourDisplayObj.GetMember("b", &colourValue[2]);
						colourDisplayObj.GetMember("a", &colourValue[3]);

						CRGBA colour = CHudColour::GetRGBA(iHudColour);

						colourValue[0].SetNumber(colour.GetRed());
						colourValue[1].SetNumber(colour.GetGreen());
						colourValue[2].SetNumber(colour.GetBlue());
						colourValue[3].SetNumber((s32)(colour.GetAlphaf() * 100.0f));

						// set the new values back onto the object:
						colourDisplayObj.SetMember("r", colourValue[0]);
						colourDisplayObj.SetMember("g", colourValue[1]);
						colourDisplayObj.SetMember("b", colourValue[2]);
						colourDisplayObj.SetMember("a", colourValue[3]);
					}
					else
					{
						Assertf(0, "GET_HUD_COLOUR called without a valid RGBA object");
					}
				}
				else
				{
					Assertf(0, "GET_HUD_COLOUR called with an invalid HUD_COLOUR id");
				}
			}
			else
			{
				Assertf(0, "GET_HUD_COLOUR called without a correct params");
			}
		}

		return;
	}

	// public function SET_DISPLAY_CONFIG
	if (methodName == ATSTRINGHASH("SET_DISPLAY_CONFIG",0x15acbeee))
	{
		if (sfVerifyf(!args[1].IsUndefined(), "SET_DISPLAY_CONFIG params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			GFxValue configDisplayObj = args[1];

			if (configDisplayObj.IsObject() &&
				configDisplayObj.HasMember("screenWidth") &&
				configDisplayObj.HasMember("screenHeight") &&
				configDisplayObj.HasMember("safeTop") &&
				configDisplayObj.HasMember("safeBottom") &&
				configDisplayObj.HasMember("safeLeft") &&
				configDisplayObj.HasMember("safeRight") &&
				configDisplayObj.HasMember("isWideScreen"))
			{
				GFxValue gfxValue[11];
				configDisplayObj.GetMember("screenWidth", &gfxValue[0]);
				configDisplayObj.GetMember("screenHeight", &gfxValue[1]);
				configDisplayObj.GetMember("safeTop", &gfxValue[2]);
				configDisplayObj.GetMember("safeBottom", &gfxValue[3]);
				configDisplayObj.GetMember("safeLeft", &gfxValue[4]);
				configDisplayObj.GetMember("safeRight", &gfxValue[5]);
				configDisplayObj.GetMember("isWideScreen", &gfxValue[6]);
				configDisplayObj.GetMember("isCircleAccept", &gfxValue[7]);
				configDisplayObj.GetMember("isAsian", &gfxValue[8]);

				float fSafeZoneX[2], fSafeZoneY[2];

				CHudTools::GetMinSafeZoneForScaleformMovies(fSafeZoneX[0], fSafeZoneY[0], fSafeZoneX[1], fSafeZoneY[1]);

				const int scaleformStageWidth = ACTIONSCRIPT_STAGE_SIZE_X;
				const int scaleformStageHeight = ACTIONSCRIPT_STAGE_SIZE_Y;
				gfxValue[0].SetNumber(scaleformStageWidth);
				gfxValue[1].SetNumber(scaleformStageHeight);

				gfxValue[2].SetNumber(fSafeZoneY[0]);
				gfxValue[3].SetNumber(fSafeZoneY[1]);
				gfxValue[4].SetNumber(fSafeZoneX[0]);
				gfxValue[5].SetNumber(fSafeZoneX[1]);

				gfxValue[6].SetBoolean(CHudTools::GetWideScreen());
				gfxValue[7].SetBoolean(!CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS));
				gfxValue[8].SetNumber(CText::IsAsianLanguage());

				
				if(configDisplayObj.HasMember("actualScreenWidth") &&
					configDisplayObj.HasMember("actualScreenHeight"))
				{
					configDisplayObj.GetMember("actualScreenWidth", &gfxValue[9]);
					configDisplayObj.GetMember("actualScreenHeight", &gfxValue[10]);

					gfxValue[9].SetNumber(CHudTools::GetUIWidth());
					gfxValue[10].SetNumber(CHudTools::GetUIHeight());
				}

				// set the new values back onto the object:
				configDisplayObj.SetMember("screenWidth", gfxValue[0]);
				configDisplayObj.SetMember("screenHeight", gfxValue[1]);
				configDisplayObj.SetMember("safeTop", gfxValue[2]);
				configDisplayObj.SetMember("safeBottom", gfxValue[3]);
				configDisplayObj.SetMember("safeLeft", gfxValue[4]);
				configDisplayObj.SetMember("safeRight", gfxValue[5]);
				configDisplayObj.SetMember("isWideScreen", gfxValue[6]);
				configDisplayObj.SetMember("isCircleAccept", gfxValue[7]);
				configDisplayObj.SetMember("isAsian", gfxValue[8]);

				if(configDisplayObj.HasMember("actualScreenWidth") &&
					configDisplayObj.HasMember("actualScreenHeight"))
				{
					configDisplayObj.SetMember("actualScreenWidth", gfxValue[9]);
					configDisplayObj.SetMember("actualScreenHeight", gfxValue[10]);
				}
			}
			else
			{
				Assertf(0, "SET_DISPLAY_CONFIG called with an invalid object");
			}
		}

		return;
	}

	// public function DEBUG_PRINT_MEMORY_STATS
	if (methodName == ATSTRINGHASH("DEBUG_PRINT_MEMORY_STATS", 0x6a73b0af))
	{
#if __SF_STATS
		if (uiVerifyf(args[1].IsString(), "DEBUG_PRINT_MEMORY_STATS params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			char cMovieString[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];
			safecpy(cMovieString, args[1].GetString(), NELEM(cMovieString));

			s32 iMovieIdToCallOn = CScaleformMgr::FindMovieByFilename(cMovieString);

			if (iMovieIdToCallOn != -1)
			{
				DebugPrintMemoryStats(iMovieIdToCallOn);
			}
		}
#endif
		return;
	}
}



////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsAnyMovieRender3D()
// PURPOSE: check if any movie has been flagged to be rendered in the scene
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsAnyMovieRender3D(bool& bNeedEmissivePass)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr::IsAnyMovieRender3D can only be called on the RenderThread!");
		return false;
	}

	bool bIsAnyMovieRender3D = false;

	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].bRender3D.GetRenderBuf())
		{
			bIsAnyMovieRender3D = true;

			if (sm_ScaleformArray[iIndex].iBrightness.GetRenderBuf() > 0)
			{
				bNeedEmissivePass = true;
				break;
			}
		}
	}
	return bIsAnyMovieRender3D;
}

////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::IsAnyMovieRender3DSolid()
// PURPOSE: check if any movie has been flagged to be rendered in the scene
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::IsAnyMovieRender3DSolid()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "ScaleformMgr::IsAnyMovieRender3DSolid can only be called on the RenderThread!");
		return false;
	}

	bool bIsAnyMovieRender3DSolid = false;

	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].bRender3DSolid.GetRenderBuf())
		{
			bIsAnyMovieRender3DSolid = true;
		}
	}
	return bIsAnyMovieRender3DSolid;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::AutoRespondIfNeeded()
// PURPOSE: checks for an :onComplete: tag at the end of the params and invokes
//			the method to call
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::AutoRespondIfNeeded(eSCALEFORM_BASE_CLASS iBaseClass, s32 iMovieCalledFrom, const char *pAutoRespondString)
{
	if ( (pAutoRespondString) && (pAutoRespondString[0]) )
	{
		if (pAutoRespondString[0] == ':')
		{
			if (strlen(pAutoRespondString) > 12)
			{
				if (!strncmp(":onComplete:", pAutoRespondString, 12))
				{
					const char *pMethodNameToInvoke = &pAutoRespondString[12];

					CallMethod(iMovieCalledFrom, iBaseClass, pMethodNameToInvoke);

					sfDebugf1("ScaleformMgr: Response sent back to Actionscript via %s", pMethodNameToInvoke);
				}
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::ExecuteCallbackMethod()
// PURPOSE: executes the callback method
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::ExecuteCallbackMethod(s32 iMovieCalledFrom, const char* methodName, const GFxValue* args, s32 iArgCount)
{
	if ( (iMovieCalledFrom == -1) || (IsMovieIdInRange(iMovieCalledFrom) && IsMovieUpdateable(iMovieCalledFrom)) )  // only execute if movie is active
	{
		s32 iBufferInUse;

		if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
		{
			iBufferInUse = 0;
		}
		else if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
		{
			iBufferInUse = 1;
		}
		else
		{
			iBufferInUse = 2; // Loading thread
		}

		ms_bExternalInterfaceCalled[iBufferInUse] = true;

		if (sfVerifyf(args[0].IsNumber() && iArgCount >= 1, "ScaleformMgr: Method Name '%s' with %d params (type %d) is not called correctly by ActionScript", methodName, iArgCount, args[0].GetType()))
		{
			// filter out the DEBUG_LOG line showing up if we've turned it on (cleaner output)

			atHashWithStringBank uMethodName(methodName);

#if __BANK
			if( !ms_bFilterAwayDebugLog || uMethodName != ATSTRINGHASH("DEBUG_LOG",0xf40f6c4d) )
#endif
			{
				if (iMovieCalledFrom != -1)
				{
					sfDebugf1("ScaleformMgr: Scaleform ActionScript called method: '%d.%s' by movie %s (%d)", (s32)args[0].GetNumber(), methodName, sm_ScaleformArray[iMovieCalledFrom].cFilename, iMovieCalledFrom);
				}
				else
				{
					sfDebugf1("ScaleformMgr: Scaleform ActionScript called method: '%d.%s' from Initialise()", (s32)args[0].GetNumber(), methodName);
				}
			}

			eSCALEFORM_BASE_CLASS iBaseClass = eSCALEFORM_BASE_CLASS( s32(args[0].GetNumber()) );

#if __BANK
			// public function DEBUG_LOG(String)
			if (uMethodName == ATSTRINGHASH("DEBUG_LOG",0xf40f6c4d))
			{
				if (uiVerifyf(args[1].IsString(), "DEBUG_LOG params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
				{
					if (iMovieCalledFrom != -1)
					{
						sfDebugf1("SF HUD (AS) %s: %s", sm_ScaleformArray[iMovieCalledFrom].cFilename, args[1].GetString());
					}
					else
					{
						sfDebugf1("SF HUD (AS) from Initialise() %s", args[1].GetString());
					}
				}
			}
			else
#endif // __BANK
			{
				switch( iBaseClass )
				{
					case SF_BASE_CLASS_GENERIC:
						CScaleformMgr::CheckIncomingFunctions(uMethodName, args);
						break;

					case SF_BASE_CLASS_SCRIPT:
					case SF_BASE_CLASS_WEB:
						CScriptHud::CheckIncomingFunctions(uMethodName, args);
						break;

					case SF_BASE_CLASS_HUD:
						CNewHud::CheckIncomingFunctions(uMethodName, args);
						break;

					case SF_BASE_CLASS_MINIMAP:
						CMiniMap_Common::CheckIncomingFunctions(uMethodName, args);
						break;

					case SF_BASE_CLASS_PAUSEMENU:
						CPauseMenu::CheckIncomingFunctions(uMethodName, args, iArgCount);
						break;

					case SF_BASE_CLASS_STORE:
						cStoreScreenMgr::CheckIncomingFunctions(uMethodName, args);
						break;

					case SF_BASE_CLASS_GAMESTREAM:
						CGameStreamMgr::CheckIncomingFunctions(uMethodName, args);
						break;

					case SF_BASE_CLASS_VIDEO_EDITOR:
//						CVideoEditor::CheckIncomingFunctions(uMethodName, args);
						break;
					case SF_BASE_CLASS_MOUSE:
						WIN32PC_ONLY( CMousePointer::CheckIncomingFunctions(uMethodName, args, iMovieCalledFrom); )
						break;
					case SF_BASE_CLASS_TEXT_INPUT:
						WIN32PC_ONLY( CTextInputBox::CheckIncomingFunctions(uMethodName, args); )
						break;
					default:
						uiAssertf(0,"Unhandled class %f for %s! Didn't know what to do with it!", args[0].GetNumber(), methodName);
				}
			}

			// if required, send out the response to tell Actionscript this has been processed
			if (args[iArgCount-1].IsString())
			{
				AutoRespondIfNeeded(iBaseClass, iMovieCalledFrom, args[iArgCount-1].GetString());
			}
		}

		ms_bExternalInterfaceCalled[iBufferInUse] = false;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::HasExternalInterfaceBeenCalled()
// PURPOSE: returns if external interface has been called
/////////////////////////////////////////////////////////////////////////////////////
bool CScaleformMgr::HasExternalInterfaceBeenCalled()
{
	s32 iBufferInUse;

	if (CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
	{
		iBufferInUse = 0;
	}
	else if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		iBufferInUse = 1;
	}
	else
	{
		iBufferInUse = 2;
	}

	return (ms_bExternalInterfaceCalled[iBufferInUse]);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	sfCallGameFromFlash::Callback
// PURPOSE: retrieves data coming back from ActionScript.  This then calls each
//			part of the code (script, hud etc) so they can do their individual checks
//			on these
/////////////////////////////////////////////////////////////////////////////////////
void sfCallGameFromFlash::Callback(GFxMovieView *pMovieView, const char* methodName, const GFxValue* args, UInt argCount)
{
	//
	// find out what movie this callback came from:
	//
	s32 iMovieCalledFrom = -1;

	if (pMovieView)
	{
		for (s32 i = 0; i < CScaleformMgr::sm_ScaleformArray.GetCount(); i++)
		{
			if (CScaleformMgr::IsMovieUpdateable(i))
			{
				if (CScaleformMgr::GetMovieView(i) == pMovieView)
				{
					iMovieCalledFrom = i;
				}
			}
		}
	}

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		// we need to ensure no arguments include a display object - if they do, then we need to exceute this on the
		// renderthread as it will be Actionscript trying to modify a display object with a pointer to it.  everything
		// else is buffered up and executed on the update thread
		bool bExecuteNow = false;

		// DEBUG_LOG in HUD gets invoked straight away as we always need the output when it is processed
		if ((eSCALEFORM_BASE_CLASS)(s32)args[0].GetNumber() == SF_BASE_CLASS_HUD && (!stricmp(methodName, "DEBUG_LOG")))
		{
			bExecuteNow = true;
		}

 		// CALL_METHOD_ON_MOVIE gets invoked straight away as we are on the RT here and want to invoke this on the RT too so no point in buffering up
 		if ((eSCALEFORM_BASE_CLASS)(s32)args[0].GetNumber() == SF_BASE_CLASS_GENERIC && (!stricmp(methodName, "CALL_METHOD_ON_MOVIE")))
 		{
 			bExecuteNow = true;
 		}

		for (s32 i = 1; (!bExecuteNow) && i < argCount; i++)
		{
			if (args[i].IsDisplayObject())
			{
				bExecuteNow = true;
				break;
			}
		}

		if (CScaleformMgr::IsMovieUpdateable(iMovieCalledFrom))  // only add or execute if we have a valid movie id
		{
			if (bExecuteNow)
			{
				CScaleformMgr::ExecuteCallbackMethod(iMovieCalledFrom, methodName, args, argCount);
			}
			else
			{
				CScaleformMgr::AddCallbackMethodToBuffer(iMovieCalledFrom, methodName, args, argCount);
			}
		}
	}
	else
	{
		// if called on UT and no movie logged in system yet = we can assume this is called from Initialise(), therefore invoke straight away on UT
		if (pMovieView && iMovieCalledFrom == -1)
		{
			CScaleformMgr::ExecuteCallbackMethod(-1, methodName, args, argCount);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// __BANK - all debug stuff shown appear under here
/////////////////////////////////////////////////////////////////////////////////////
#if __BANK
void CScaleformMgr::ShowFontCacheTexture()
{
	grcTexture* tex = CScaleformMgr::GetMovieMgr()->GetRageRenderer().GetFontCacheTexture();
	if (tex)
	{
		CDebugTextureViewerInterface::SelectPreviewTexture(tex, "Scaleform Font Cache", true);
	}
}

void ReloadPreallocations()
{
	CScaleformMgr::LoadPreallocationInfo(SCALEFORM_PREALLOC_XML_FILENAME);
}

void TurnOnOutlines()
{
	CScaleformMgr::ms_bShowOutlineFor2DMovie = true;
}

void CScaleformMgr::UpdateWatchedMovie()
{
	for(int i = 0; i < g_ScaleformStore.GetCount(); i++)
	{
		CScaleformDef* pDef = g_ScaleformStore.GetSlot(strLocalIndex(i));
		if (pDef)
		{
			CScaleformMovieObject* pObj = g_ScaleformStore.Get(strLocalIndex(i));
			if (pObj)
			{
				if (stristr(pDef->m_cFullFilename, CScaleformStore::sm_WatchedMovieSubstr))
				{
					ms_watchedResource = &pObj->GetRawMovie()->GetMovie();
					return;
				}
			}
		}
	}
	ms_watchedResource = NULL;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::InitWidgets()
// PURPOSE: creates the scaleform bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank(SCALEFORM_BANK_NAME);

	bank.AddToggle("Debug Output from Scaleform (dbg)", &ms_bDebugOutputOn, &DebugTurnOnOffOutputForAllMovies);
	bank.AddToggle("   Filter Away DEBUG_LOG line when debug output is on", &ms_bFilterAwayDebugLog);

	bank.AddToggle("Dump debug info", &CScaleformMgr::ms_bPrintDebugInfoLog);
	bank.AddToggle("Verify AS Methods", &CScaleformMgr::ms_bVerifyActionScriptMethods);
	bank.AddButton("Show Font Cache Texture", datCallback(CFA(ShowFontCacheTexture)));

	bank.AddButton("Reload Preallocations", datCallback(CFA(ReloadPreallocations)));

	bank.AddToggle("Show Debug Outline For 2D Movies", &CScaleformMgr::ms_bShowOutlineFor2DMovie);
	bank.AddToggle("Show Debug Outline For 3D Movies", &CScaleformMgr::ms_bShowOutlineFor3DMovie);
	bank.AddToggle("Show Member Variables", &CScaleformMgr::ms_bShowMemberVariables);
	bank.AddToggle("Show full names", &CScaleformMgr::ms_bFullNames);
	
	bank.AddToggle("Draw standard variables (_x, _y, etc)", &ms_bDrawStandardVariables);
	bank.AddVector("Text Position Override", &ms_vTextOverride, 0.0f, 1.0f, 0.01f, NullCB, "Use to ensure text appears on screen, like if the transform matrices fail (which they always do currently)");
	bank.AddSlider("Recursive Object Depth", &ms_iMaxMemberVariableDepth, 0, 10, 1);
	
	bank.AddText("Outline Filter", &ms_OutlineFilter, datCallback(CFA(TurnOnOutlines)));
	bank.AddToggle("Matching filter applies to children", &ms_bWildcardNameisRecursive);

	bank.AddToggle("Show performance info", &ms_bShowPerformanceInfo);
	bank.AddToggle("Show preallocation info", &ms_bShowPreallocationInfo);
	bank.AddToggle("Show _all_ preallocation info", &ms_bShowAllPreallocationInfo);
	bank.AddToggle("Show reference counts", &ms_bShowRefCountInfo);

	bank.AddButton("Output list of Active Movies to Log", &OutputListOfActiveScaleformMoviesToLog);
	bank.AddText("Track references to movie named:", CScaleformStore::sm_WatchedMovieSubstr, NELEM(CScaleformStore::sm_WatchedMovieSubstr), datCallback(CFA(CScaleformMgr::UpdateWatchedMovie)));

	bank.AddSeparator();

	bank.AddSlider("Debug Scaleform Movie Pos", &ms_vDebugMoviePos, 0.0f, 1.0f, 0.001f, &ReadjustDebugMovie);
	bank.AddSlider("Debug Scaleform Movie Size", &ms_vDebugMovieSize, 0.0f, 1.0f, 0.001f, &ReadjustDebugMovie);

	bank.AddText("Load Debug Scaleform Movie", ms_cDebugMovieName, sizeof(ms_cDebugMovieName), false, &LoadDebugMovie);
	bank.AddText("Call Debug Scaleform Method Method", ms_cDebugMethod, sizeof(ms_cDebugMethod), false, &CallDebugMethod);
	bank.AddText("Loaded Scaleform Movie", ms_cDebugLoadedMovieName, sizeof(ms_cDebugLoadedMovieName), true);
	bank.AddButton("Load Debug Scaleform Movie", &LoadDebugMovie);
	bank.AddButton("Call Debug Scaleform Movie Method", &CallDebugMethod);
	bank.AddButton("Unload Debug Scaleform Movie", &UnloadDebugMovie);

	bank.AddSeparator();

	// 	bank.AddButton("Output all Scaleform movie memory usage", &LoadAllMoviesAndGetMemoryInfo);

	//  bank.AddSeparator();

	ms_cDebugIgnoreMethod[0] = '\0';
	bank.AddText("Ignore Scaleform Method", ms_cDebugIgnoreMethod, sizeof(ms_cDebugIgnoreMethod), false);

	bank.AddSeparator();

	bank.AddToggle("Show Extra debug info", &ms_bShowExtraDebugInfo);

	bank.AddSeparator();

	bank.AddToggle("Override Brightness Level for 3D Movies", &ms_bOverrideEmissiveLevel);
	bank.AddSlider("Brightness Level", &ms_emissiveLevel, 0.0f, 1.0f, 0.001f, NullCB);

	GetMovieMgr()->AddWidgets(bank);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::ShutdownWidgets()
// PURPOSE: removes the hud bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::ShutdownWidgets()
{
	bkBank *bank = BANKMGR.FindBank(SCALEFORM_BANK_NAME);

	if (bank)
	{
		bank->Destroy();
	}
}

#if __SF_STATS

ScaleformMemoryStats::ScaleformMemoryStats()
: m_MovieDefTotal(0)
, m_MovieDefUsed(0)
, m_MovieViewTotal(0)
, m_MovieViewUsed(0)
, m_PreallocTotal(0)
, m_PreallocUsed(0)
, m_PreallocPeak(0)
, m_Overalloc(0)
, m_MPSpecific(false)
{
}

void AccumulateSFHeapStats(GMemoryHeap* heap, size_t& footprint, size_t& inUse)
{
	if (!heap)
	{
		return;
	}
	footprint += heap->GetTotalFootprint();
	inUse += heap->GetTotalUsedSpace();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GatherMemoryStats()
// PURPOSE: fills out a ScaleformMemoryStats object with all the stats for a given movie
// NOTES:	All memory counts here are in bytes
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::GatherMemoryStats(s32 iIndex, ScaleformMemoryStats& stats)
{
	if (IsMovieActive(iIndex))
	{
		CScaleformMovieObject* movieObj = g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

		sfPreallocatedMemoryWrapper* memory = CScaleformMgr::GetMovieMgr()->GetAllocatorForArena(movieObj->m_MemoryArena);

		if (memory)
		{

			stats.m_PreallocTotal = memory->FindPreallocatedSize();
			stats.m_Overalloc = memory->FindExtraAllocationSize();
			stats.m_PeakAllowed = memory->m_PeakAllowed;

			for(int i = 0; i < memory->m_PreallocatedStats.GetMaxCount(); i++)
			{
				if (memory->m_PreallocatedStats[i].m_Size > 0)
				{
					stats.m_PreallocUsed += memory->m_PreallocatedStats[i].m_Count * memory->m_PreallocatedStats[i].m_Size;
					stats.m_PreallocPeak += memory->m_PreallocatedStats[i].m_HighCount * memory->m_PreallocatedStats[i].m_Size;
				}
			}
			stats.m_PreallocUsed *= memory->m_Granularity;
			stats.m_PreallocPeak *= memory->m_Granularity;
		}

		GFxMovieDef& movieDef = movieObj->GetRawMovie()->GetMovie();
		//AccumulateSFHeapStats(movieDef.GetBindDataHeap(), stats.m_MovieDefTotal, stats.m_MovieDefUsed);
		AccumulateSFHeapStats(movieDef.GetLoadDataHeap(), stats.m_MovieDefTotal, stats.m_MovieDefUsed);
		AccumulateSFHeapStats(movieDef.GetImageHeap(), stats.m_MovieDefTotal, stats.m_MovieDefUsed);

		CScaleformDef* movieStoreDef = g_ScaleformStore.GetSlot(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));
		if (movieStoreDef)
		{
			if (movieStoreDef->HasSeperateMultiplayerPrealloc() && movieStoreDef->m_PreallocationInfoMP.GetVirtualSize() == stats.m_PreallocTotal)
			{
				stats.m_MPSpecific = true;
			}
		}

		sfScaleformMovieView* movieView = movieObj->GetRawMovieView();
		if (movieView)
		{
			AccumulateSFHeapStats(movieView->GetMovieView().GetHeap(), stats.m_MovieViewTotal, stats.m_MovieViewUsed);
		}
	}
}


void CScaleformMgr::PrintPreallocInfo( s32 iIndex, int& linesPrinted )
{
	if (linesPrinted % 20 == 0)
	{
		if (linesPrinted > 0)
		{
			grcDebugDraw::AddDebugOutputEx(false, "|---------------------------------------------------------------------------------------------------------------------------|");
		}
		grcDebugDraw::AddDebugOutputEx(false, "|  Name                 | Gran | Base | s e q h |  1 |  2 |  4 |  8 | 16 | | Gran | Base | s e q h |  1 |  2 |  4 |  8 | 16 |");
	}

	datResourceInfo rscUsed;
	size_t granUsed = 0;
	sysMemSet(&rscUsed, 0x0, sizeof(rscUsed));
	CScaleformDef* movieStoreDef = g_ScaleformStore.GetSlot(strLocalIndex(iIndex));
	if (movieStoreDef)
	{
		rscUsed = (NetworkInterface::IsGameInProgress()) ? movieStoreDef->m_PreallocationInfoMP : movieStoreDef->m_PreallocationInfo;
		granUsed = NetworkInterface::IsGameInProgress() ? movieStoreDef->m_GranularityKb_MP : movieStoreDef->m_GranularityKb;
	}

	datResourceInfo rscSuggested;
	sysMemSet(&rscSuggested, 0x0, sizeof(rscSuggested));
	size_t granSuggested = 0;

	CScaleformMovieObject* movieObj = g_ScaleformStore.Get(strLocalIndex(iIndex));
	if (movieObj)
	{
		sfPreallocatedMemoryWrapper* memory = CScaleformMgr::GetMovieMgr()->GetAllocatorForArena(movieObj->m_MemoryArena);

		memory->ComputeCurrentPreallocationNeeds(rscSuggested, granSuggested);
	}

	Color32 textColor = (linesPrinted % 2 == 0) ? Color_white : Color_PaleGreen;
	linesPrinted++;
	grcDebugDraw::AddDebugOutputEx(false, textColor, "| %21.21s | %4d | %4d | %1.0d %1.0d %1.0d %1.0d |%3.0d |%3.0d |%3.0d |%3.0d |%3.0d | | %4d | %4d | %1.0d %1.0d %1.0d %1.0d |%3.0d |%3.0d |%3.0d |%3.0d |%3.0d |",
		movieStoreDef->m_name.GetCStr(),
		granUsed, rscUsed.GetVirtualChunkSize() / 1024,
		rscUsed.Virtual.HasTail16, rscUsed.Virtual.HasTail8, rscUsed.Virtual.HasTail4, rscUsed.Virtual.HasTail2,
		rscUsed.Virtual.BaseCount,
		rscUsed.Virtual.Head2Count, rscUsed.Virtual.Head4Count,	rscUsed.Virtual.Head8Count,	rscUsed.Virtual.Head16Count,
		granSuggested / 1024, rscSuggested.GetVirtualChunkSize() / 1024,
		rscSuggested.Virtual.HasTail16, rscSuggested.Virtual.HasTail8, rscSuggested.Virtual.HasTail4, rscSuggested.Virtual.HasTail2,
		rscSuggested.Virtual.BaseCount,
		rscSuggested.Virtual.Head2Count, rscSuggested.Virtual.Head4Count, rscSuggested.Virtual.Head8Count, rscSuggested.Virtual.Head16Count
		);
}

#endif



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::GetMemoryUsage()
// PURPOSE: returns amount of memory the movie current consumes
/////////////////////////////////////////////////////////////////////////////////////
s32 CScaleformMgr::GetMemoryUsage(const s32 iIndex)
{
	if (IsMovieActive(iIndex))
	{
		ScaleformMemoryStats stats;
		GatherMemoryStats(iIndex, stats);

		return (s32)(stats.m_PreallocUsed / 1024);
	}

	return 0;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::DisplayPerformanceInfo()
// PURPOSE: shows performance info for each of the scaleform movies
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::DisplayPerformanceInfo()
{
#if __SF_STATS
	grcDebugDraw::AddDebugOutputEx(false, "/----- Performance Information ---------------------------------------------------------------------------------------------------------------\\");
	grcDebugDraw::AddDebugOutputEx(false, "|  Name                 |  Update (avg)  | UpdateAS (avg) |   Game  (avg)  |  Draw   (avg)  |  Tess   (avg)  |  Prims  (avg)  |  Tris   (avg) |");
	int linesPrinted = 0;
	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex))
		{
			CScaleformMovieObject* movieObj = g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

			sfScaleformMovieView* movieView = movieObj->GetRawMovieView();

			if (!movieView)
			{
				continue;
			}
			Color32 textColor = (linesPrinted % 2 == 0) ? Color_white : Color_PaleGreen;
			if( ms_bShowOutlineFor2DMovie )
			{
				textColor.Set(atStringHash(sm_ScaleformArray[iIndex].cFilename));
				textColor.SetAlpha(255);
			}
			linesPrinted++;
			grcDebugDraw::AddDebugOutputEx(false, textColor, "| %21.21s | %6.3f %6.3f  | %6.3f %6.3f  | %6.3f %6.3f  | %6.3f %6.3f  | %6.3f %6.3f  | %6.0d %6.0d  | %6.0d %6.0d |", sm_ScaleformArray[iIndex].cFilename,
				movieView->m_StatUpdate.last, movieView->m_StatUpdate.avg,
				movieView->m_StatUpdateAs.last, movieView->m_StatUpdateAs.avg,
				movieView->m_StatGame.last, movieView->m_StatGame.avg,
				movieView->m_StatDraw.last, movieView->m_StatDraw.avg,
				movieView->m_StatTess.last, movieView->m_StatTess.avg,
				(int)movieView->m_StatPrims.last, (int)movieView->m_StatPrims.avg,
				(int)movieView->m_StatTris.last, (int)movieView->m_StatTris.avg
				);
		}
	}
	grcDebugDraw::AddDebugOutputEx(false, "\\---------------------------------------------------------------------------------------------------------------------------------------------/");

	grcDebugDraw::AddDebugOutputEx(false, "");

	grcDebugDraw::AddDebugOutputEx(false, "/----- Memory Information ---------------------------------------------------------------------------------------------------------------------\\");
	grcDebugDraw::AddDebugOutputEx(false, "|                       |               Scaleform Internal               | |                        Rage Resource Memory                       |");
	grcDebugDraw::AddDebugOutputEx(false, "|  Name                 | Def Total |  Def Used | View Total | View Used | | Prealloc | In Use |RealPeak| Overalloc| Pre+Over | XML Peak| Room |");

	linesPrinted = 0;
	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex))
		{
			ScaleformMemoryStats stats;
			GatherMemoryStats(iIndex, stats);

			Color32 textColor = (linesPrinted % 2 == 0) ? Color_white : Color_PaleGreen;
			linesPrinted++;
			if (stats.m_PreallocTotal + stats.m_Overalloc > stats.m_PeakAllowed)
			{
				textColor = Color_red;
			}
			grcDebugDraw::AddDebugOutputEx(false, textColor, "| %21.21s | %9.0d | %9.0d | %10.0d | %9.0d | | %8.0d%c| %6.0d | %6.0d | %8.0d | %8.0d | %7.0d | %4d |",
				sm_ScaleformArray[iIndex].cFilename,
				stats.m_MovieDefTotal / 1024, stats.m_MovieDefUsed / 1024, stats.m_MovieViewTotal / 1024, stats.m_MovieViewUsed / 1024,
				stats.m_PreallocTotal / 1024, stats.m_MPSpecific ? '*' : ' ', stats.m_PreallocUsed / 1024, stats.m_PreallocPeak / 1024, stats.m_Overalloc / 1024,
				(stats.m_PreallocTotal + stats.m_Overalloc) / 1024, (stats.m_PeakAllowed) / 1024,
				((s32)stats.m_PeakAllowed - (s32)(stats.m_PreallocPeak + stats.m_Overalloc)) / 1024);
		}
	}
	grcDebugDraw::AddDebugOutputEx(false, "\\----------------------------------------------------------------------------------------------------------------------------------------------/");

	grcDebugDraw::AddDebugOutputEx(false, "");


	if (ms_bShowPreallocationInfo)
	{
		grcDebugDraw::AddDebugOutputEx(false, "/----- Prealloc Information ------------------------------------------------------------------------------------------------\\");
		grcDebugDraw::AddDebugOutputEx(false, "|                       |                  Specified in XML              | |                   Recommended                  |");

		linesPrinted = 0;

		if (ms_bShowAllPreallocationInfo)
		{
			for (s32 iIndex = 0; iIndex < g_ScaleformStore.GetNumUsedSlots(); iIndex++)
			{
				PrintPreallocInfo(iIndex, linesPrinted);
			}
		}
		else
		{
			for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
			{
				if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].iMovieId >= 0)
				{
					PrintPreallocInfo(sm_ScaleformArray[iIndex].iMovieId, linesPrinted);
				}
			}

		}

		grcDebugDraw::AddDebugOutputEx(false, "\\---------------------------------------------------------------------------------------------------------------------------/");

		grcDebugDraw::AddDebugOutputEx(false, "");
	}

	if (ms_bShowRefCountInfo)
	{
		grcDebugDraw::AddDebugOutputEx(false, "/----- RefCounts-----------------------------------------------------\\");
		grcDebugDraw::AddDebugOutputEx(false, "|                       |  Rsc  |  TXD  | TexInt | Movie | MovieView |");

		linesPrinted = 0;

		bool displayAddtlTextureDetail = true;

		for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
		{
			if (IsMovieActive(iIndex) && sm_ScaleformArray[iIndex].iMovieId >= 0)
			{
				Color32 textColor = (linesPrinted % 2 == 0) ? Color_white : Color_PaleGreen;
				linesPrinted++;

				s32 gfxRefs, texRefs, movieRefs, movieViewRefs, texInternal;
				gfxRefs = texRefs = movieRefs = movieViewRefs = texInternal = 0;

				CScaleformMovieObject* movieObj = g_ScaleformStore.Get(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));
				if (movieObj)
				{
					gfxRefs = g_ScaleformStore.GetNumRefs(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));

					sfScaleformMovie* sfMovie = movieObj->GetRawMovie();
					if (sfMovie)
					{
						GFxMovieDef& movie = movieObj->GetRawMovie()->GetMovie();
						movieRefs = movie.GetRefCount();

						if (&movie == ms_watchedResource)
						{
							textColor = Color_orange;
							displayAddtlTextureDetail = true;
						}
					}

					sfScaleformMovieView* sfMovieView = movieObj->GetRawMovieView();
					if (sfMovieView)
					{
						GFxMovieView& movieView = movieObj->GetRawMovieView()->GetMovieView();
						movieViewRefs = movieView.GetRefCount();
					}
				}

				CScaleformDef* movieDef = g_ScaleformStore.GetSlot(strLocalIndex(sm_ScaleformArray[iIndex].iMovieId));
				if (movieDef)
				{
					s32 texDictIdx = movieDef->m_iTextureSlot.Get();
					if (texDictIdx >= 0 && g_TxdStore.IsValidSlot(strLocalIndex(texDictIdx)))
					{
						texRefs = g_TxdStore.GetNumRefs(strLocalIndex(texDictIdx));

						pgDictionary<grcTexture>* texDict = g_TxdStore.Get(strLocalIndex(texDictIdx));
						if (texDict)
						{
							for(u32 iTexIdx = 0; iTexIdx < texDict->GetCount(); iTexIdx++)
							{
								texInternal += texDict->GetEntry(iTexIdx)->GetRefCount();
							}
							texInternal -= texDict->GetCount();
						}

					}
				}

				grcDebugDraw::AddDebugOutputEx(false, textColor, "| %21.21s | %5.0d | %5.0d | %6.0d | %5.0d | %9.0d |",
					sm_ScaleformArray[iIndex].cFilename, gfxRefs, texRefs, texInternal, movieRefs, movieViewRefs);


				if (displayAddtlTextureDetail && sm_ScaleformArray[iIndex].requestedTxds.GetCount() > 0)
				{
					grcDebugDraw::AddDebugOutputEx(false, "|    /- Texture Requests ----------------------------------------------------\\");
					grcDebugDraw::AddDebugOutputEx(false, "|    |        Name        |      RefStr     |  Wait |  ID   |  TXD  | TexInt |");
					for(int iTxdIndex = 0; iTxdIndex < sm_ScaleformArray[iIndex].requestedTxds.GetCount(); iTxdIndex++)
					{
						ScaleformMovieTxd& txd = sm_ScaleformArray[iIndex].requestedTxds[iTxdIndex];
						int internal = 0;
						strLocalIndex iTxdId = txd.iTxdId;
						if (g_TxdStore.IsValidSlot(iTxdId))
						{
							texRefs = g_TxdStore.GetNumRefs(iTxdId);

							pgDictionary<grcTexture>* texDict = g_TxdStore.Get(iTxdId);
							if (texDict)
							{
								for(u32 iTexIdx = 0; iTexIdx < texDict->GetCount(); iTexIdx++)
								{
									internal += texDict->GetEntry(iTexIdx)->GetRefCount();
								}
								internal -= texDict->GetCount();
							}
						}

						// Show the right end of the string, if it's too long
						size_t texNameLen = strlen(g_TxdStore.GetName(iTxdId));
						const char* texNamePtr = g_TxdStore.GetName(iTxdId);
						if (texNameLen > 18)
						{
							texNamePtr += (texNameLen - 18);
						}

						grcDebugDraw::AddDebugOutputEx(false, "|    | %18.18s | %15.15s |   %c   | %5.0d | %5.0d | %6.0d |",
							texNamePtr, txd.cUniqueRefString, txd.AlreadyLoaded() ? 'y' : ' ', iTxdId, texRefs, internal);
					}
					grcDebugDraw::AddDebugOutputEx(false, "|    \\-----------------------------------------------------------------------/");
				}

				if (displayAddtlTextureDetail && sm_ScaleformArray[iIndex].allRequestedTxds.GetCount() > 0)
				{
					grcDebugDraw::AddDebugOutputEx(false, "|    /- All Extra Textures--------------------------------------\\    |");
					grcDebugDraw::AddDebugOutputEx(false, "|    |        Name        |  ID   | Live |  References          |    |");
					for(int iTxdIndex = 0; iTxdIndex < sm_ScaleformArray[iIndex].allRequestedTxds.GetCount(); iTxdIndex++)
					{
						strLocalIndex iTxdId = strLocalIndex(sm_ScaleformArray[iIndex].allRequestedTxds[iTxdIndex]);
						bool live = g_TxdStore.GetSafeFromIndex(iTxdId) != NULL;

						// Show the right end of the string, if it's too long
						size_t texNameLen = strlen(g_TxdStore.GetName(iTxdId));
						const char* texNamePtr = g_TxdStore.GetName(iTxdId);
						if (texNameLen > 18)
						{
							texNamePtr += (texNameLen - 18);
						}

						char buf[32];
						if (g_TxdStore.IsValidSlot(iTxdId))
						{
							g_TxdStore.GetRefCountString(iTxdId, buf, NELEM(buf));
						}
						else
						{
							formatf(buf, "     <N/A>     ");
						}

						grcDebugDraw::AddDebugOutputEx(false, "|    | %18.18s | %5.0d |  %c   | %s      |    |",
							texNamePtr, iTxdId, live ? 'y' : ' ', buf);
					}
					grcDebugDraw::AddDebugOutputEx(false, "|    \\----------------------------------------------------------/    |");
				}

			}
		}
		grcDebugDraw::AddDebugOutputEx(false, "\\--------------------------------------------------------------------/");

		grcDebugDraw::AddDebugOutputEx(false, "");

	}

	grcDebugDraw::AddDebugOutputEx(false, "/----- Global Memory Information ------------------------------\\");
	size_t curr = 0, peak = 0, debugCurr = 0, debugPeak = 0;
	const char* lastAllocator = NULL;
	CScaleformMgr::GetMovieMgr()->GetGlobalHeapSizes(curr, peak, debugCurr, debugPeak, lastAllocator);
	grcDebugDraw::AddDebugOutputEx(false, "|  Rage heap = %6dk, peak = %6dk.                        |", curr/1024, peak/1024, lastAllocator);
	grcDebugDraw::AddDebugOutputEx(false, "|    Last movie to allocate: %25s         |", lastAllocator);
	GFxLoader* loader = CScaleformMgr::GetMovieMgr()->GetLoader();

	grcDebugDraw::AddDebugOutputEx(false, "|    Mesh cache = %6dk, limit = %6dk                     |", loader->GetMeshCacheManager()->GetNumBytes() / 1024, loader->GetMeshCacheManager()->GetMeshCacheMemLimit() / 1024);
	grcDebugDraw::AddDebugOutputEx(false, "|    Font vector cache - max glyphs = %6dk                  |", loader->GetFontCacheManager()->GetMaxVectorCacheSize());

	float cacheUtilization = (float)loader->GetFontCacheManager()->ComputeUsedArea() / (float)loader->GetFontCacheManager()->ComputeTotalArea();
	grcDebugDraw::AddDebugOutputEx(false, "|    Font texture glyphs = %6d, utilization = %6.2f%%       |", loader->GetFontCacheManager()->GetNumRasterizedGlyphs(), cacheUtilization * 100.0f);
	grcDebugDraw::AddDebugOutputEx(false, "|    Non-resourced textures = %6dk                          |", sfTextureBase::sm_TotalRuntimeImageSizes / 1024);
	grcDebugDraw::AddDebugOutputEx(false, "|  Debug heap = %6dk, peak = %6dk                        |", debugCurr/1024, debugPeak/1024);

	grcDebugDraw::AddDebugOutputEx(false, "\\--------------------------------------------------------------/");

	grcDebugDraw::AddDebugOutputEx(false, "");

#endif
}

#if __SF_STATS
void CScaleformMgr::DebugPrintMemoryStats(s32 iIndex)
{
	if (IsMovieActive(iIndex))
	{
		ScaleformMemoryStats stats;
		GatherMemoryStats(iIndex, stats);

		uiDisplayf("%s: MovieDef: %.3fk in-use (%" SIZETFMT "dk total), MovieView %.3fk in-use (%" SIZETFMT "dk total). Prealloc: %" SIZETFMT "dk in-use (%" SIZETFMT "dk total, %" SIZETFMT "dk peak), Overalloc: %" SIZETFMT "dk",
			sm_ScaleformArray[iIndex].cFilename,
			(float)stats.m_MovieDefUsed / 1024.0f, stats.m_MovieDefTotal / 1024,
			(float)stats.m_MovieViewUsed / 1024.0f, stats.m_MovieViewTotal / 1024,
			stats.m_PreallocUsed / 1024, stats.m_PreallocTotal / 1024, stats.m_PreallocPeak / 1024, stats.m_Overalloc / 1024);
	}

}
#endif

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::DebugTurnOnOffOutput()
// PURPOSE: Toggles output on/off in all movies that support such a feature
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::DebugTurnOnOffOutput(s32 iIndex, bool bWasJustInitialized /*= true*/)
{
	// if the movie was just created, and we don't want debug, don't set it
	if(bWasJustInitialized && !ms_bDebugOutputOn)
		return;

	if ( CScaleformMgr::IsMovieActive(iIndex) && !IsChildMovie(iIndex) && iIndex != CText::GetFontMovieId() && GetMovieView(iIndex) )
	{
		eSCALEFORM_BASE_CLASS iBaseClass = SF_BASE_CLASS_GENERIC;

		if (iIndex == CNewHud::GetContainerMovieId())
		{
			iBaseClass = SF_BASE_CLASS_HUD;
		}

		if (CScaleformMgr::BeginMethod(iIndex, iBaseClass, "SHOW_DEBUG"))
		{
			CScaleformMgr::AddParamBool(ms_bDebugOutputOn);
			CScaleformMgr::EndMethod(true);
		}
	}
}

void CScaleformMgr::DebugTurnOnOffOutputForAllMovies()
{
	for(int iIndex=0; iIndex < sm_ScaleformArray.GetCount(); ++iIndex)
	{
		DebugTurnOnOffOutput(iIndex, false);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::OutputListOfActiveScaleformMoviesToLog()
// PURPOSE: output a list of all currently active scaleform movies to the log
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::OutputListOfActiveScaleformMoviesToLog()
{
	sfDisplayf("----------- SCALEFORM MOVIES CURRENTLY ACTIVE -----------");

	for (s32 iIndex = 0; iIndex < sm_ScaleformArray.GetCount(); iIndex++)
	{
		if (IsMovieActive(iIndex))
		{
			sfDisplayf("%d  - %d : %s", iIndex, sm_ScaleformArray[iIndex].iMovieId, sm_ScaleformArray[iIndex].cFilename);
		}
	}

	sfDisplayf("---------------------------------------------------------");
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::ReadjustDebugMovie()
// PURPOSE: readjusts size/pos of the debug scaleform movie from rag widget
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::ReadjustDebugMovie()
{
	if (IsMovieActive(ms_iDebugMovieId))
	{
		ChangeMovieParams(ms_iDebugMovieId, ms_vDebugMoviePos, ms_vDebugMovieSize, GFxMovieView::SM_ExactFit);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::LoadDebugMovie()
// PURPOSE: loads a debug scaleform movie from rag widget
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::LoadDebugMovie()
{
	if (ms_iDebugMovieId != SF_INVALID_MOVIE)
	{
		CScaleformMgr::UnloadDebugMovie();
	}

	ms_iDebugMovieId = CScaleformMgr::CreateMovieAndWaitForLoad(ms_cDebugMovieName, ms_vDebugMoviePos, ms_vDebugMovieSize);
	UpdateLoadedDebugMovieName();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::UnloadDebugMovie()
// PURPOSE: removes the debug scaleform movie
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::UnloadDebugMovie()
{
	if (IsMovieIdInRange(ms_iDebugMovieId))
	{
		CScaleformMgr::RequestRemoveMovie(ms_iDebugMovieId);
		ms_iDebugMovieId = SF_INVALID_MOVIE;
		UpdateLoadedDebugMovieName();
	}
}

void CScaleformMgr::UpdateLoadedDebugMovieName()
{
	const char* debugMovieName = (ms_iDebugMovieId == SF_INVALID_MOVIE) ? "N/A" : CScaleformMgr::GetMovieFilename(ms_iDebugMovieId);
	safecpy(ms_cDebugLoadedMovieName, debugMovieName, sizeof(ms_cDebugLoadedMovieName));
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::CallDebugMethod()
// PURPOSE: call method on the scaleform movie method
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::CallDebugMethod()
{
	if (uiVerifyf(IsMovieActive(ms_iDebugMovieId), "Trying to call debug method without first loading movie"))
	{
		char cMethodName[256];
		sscanf(ms_cDebugMethod, "%s", cMethodName);
		if (BeginMethod(ms_iDebugMovieId, SF_BASE_CLASS_GENERIC, cMethodName))
		{
			PassDebugMethodParams(ms_cDebugMethod);
			EndMethod();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::PassDebugMethodParams()
// PURPOSE: parses the debug method string and sends through the debug params
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::PassDebugMethodParams(const char *pDebugMethodString)
{
	atString fullInput(pDebugMethodString);
	atArray<atString> results;
	fullInput.Split(results, ' ',true);

	// start at 1 to skip the name of the function
	for (s32 iParamCount = 1; iParamCount < results.GetCount(); iParamCount++)
	{
		const atString& curString = results[iParamCount];

		if (!stricmp(curString, "TRUE"))
		{
			CScaleformMgr::AddParamBool(TRUE);
		} 
		else if (!stricmp(curString, "FALSE"))
		{
			CScaleformMgr::AddParamBool(FALSE);
		}
		else  if( isdigit( curString[0] ) || curString[0] == '-' || curString[0] == '+')// assume number
		{
			int iNumber = int(atoi(curString));
			double fNumber = atof(curString);
			if( fNumber == double(iNumber) )
			{
				CScaleformMgr::AddParamInt(iNumber);
			}
			else
			{
				CScaleformMgr::AddParamFloat(fNumber);
			}
		}
		else if (curString[0] == '"' )
		{
			atString builder;
			bool bHashInstead = curString[1] == '%';

			builder += curString+(bHashInstead ? 2 : 1 ); // remove the quote (or the % as well)
			// because there may be spaces in our string, scan ahead until we find the next closing "
			while( builder[ builder.GetLength() - 1] != '"' && ++iParamCount < results.GetCount() )
			{
				builder += " "; // restore spaces
				builder += results[iParamCount];
			}

			// truncate final quote (loop above may have run out of params)
			if( builder[ builder.GetLength() - 1] == '"' )
				builder.Truncate(builder.GetLength()-1);

			if( bHashInstead )
			{
				CScaleformMgr::AddParamInt(atStringHash(builder.c_str()));
			}
			else
			{
				CScaleformMgr::AddParamString(builder.c_str(), builder.GetLength(),false);
			}
		}
		else // just send as bare string
		{
			CScaleformMgr::AddParamString(curString, curString.GetLength());
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CScaleformMgr::RenderDebugMovie()
// PURPOSE: renders the debug movie if one exists
/////////////////////////////////////////////////////////////////////////////////////
void CScaleformMgr::RenderDebugMovie()
{
	if (IsMovieActive(ms_iDebugMovieId))
	{
		RenderMovie(ms_iDebugMovieId);
	}
}

strLocalIndex CScaleformMgr::GetMovieStreamingIndex(s32 iIndex)
{
	if (!IsMovieIdInRange(iIndex))
		return strLocalIndex(strLocalIndex::INVALID_INDEX);

	return strLocalIndex( sm_ScaleformArray[iIndex].iMovieId );
}

#endif //BANK

// eof
