#if __BANK

// rage
#include "grcore/image.h"
#include "grcore/debugdraw.h"
#include "grmodel/shadergroup.h"
#include "string/stringutil.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"

// framework
#include "fwscene/search/Search.h"
#include "fwsys/fileexts.h"
#include "streaming/packfilemanager.h"

// game
#include "Shaders/ShaderEdit.h"
#include "core/game.h"
#include "scene/world/GameWorld.h"
#include "scene/Entity.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "streaming/streaming.h"
#include "input/mouse.h"
#include "Debug/Editing/ShaderEditing.h"
#include "debug/DebugScene.h"
#include "debug/GtaPicker.h"
#include "debug/textureviewer/textureviewer.h"
#include "peds/Ped.h"
#include "peds/rendering/PedVariationDS.h"
#include "peds/rendering/PedVariationPack.h"
#include "peds/rendering/PedVariationStream.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "system/controlMgr.h"

RENDER_OPTIMISATIONS();

#define NUM_TEXTUREDEBUGMODE 5

static const char* textureDebugMode[NUM_TEXTUREDEBUGMODE] =	{	"Density",
																"Top Mip usage",
																"Color Per Mip",
																"Color Per Resolution",
																"Color Per Size"
															};

static const char* textureDebugTxd[NUM_TEXTUREDEBUGMODE] =	{	"platform:/textures/debdensity",
																"platform:/textures/debtopLevel",
																"platform:/textures/debpermip",
																"platform:/textures/debperres",
																"platform:/textures/debpersize"
															};

static s32 textureDebugTxdSlot[NUM_TEXTUREDEBUGMODE] =		{	-1,
																-1,
																-1,
																-1,
																-1
															};

#if __DEV
#define NUM_FORCETEXTURESIZEMODE 12

static const char* textureSizeMode[NUM_FORCETEXTURESIZEMODE] = {	"Off",
																	"1",
																	"2",
																	"4",
																	"8",
																	"16",
																	"32",
																	"64",
																	"128",
																	"256",
																	"512",
																	"1024" };
#endif // __DEV

#define NAME_OF_SHADER_EDIT_PICKER "Shader Edit"

static atMap<grcTexture*,int> s_activeTextureRefCounts;
static int                    s_activeTextureCount;
static int                    s_activeTextureDataSize;

static void CShaderEdit_ActiveTextureRefAdd(grcTexture* oldTexture, grcTexture* newTexture)
{
	if (oldTexture)
	{
		if (s_activeTextureRefCounts.Access(oldTexture))
		{
			int& refCount = s_activeTextureRefCounts[oldTexture];
			Assertf(refCount > 0, "CShaderEdit_ActiveTextureRefAdd: old texture refCount=%d, expected >0", refCount);
			refCount--;
		}		
	}

	if (newTexture)
	{
		if (s_activeTextureRefCounts.Access(newTexture))
		{
			int& refCount = s_activeTextureRefCounts[newTexture];
			Assertf(refCount > 0, "CShaderEdit_ActiveTextureRefAdd: new texture refCount=%d, expected >0", refCount);
			refCount++;
		}
		else
		{
			s_activeTextureRefCounts[newTexture] = 1;
			s_activeTextureCount++;
			s_activeTextureDataSize += newTexture->GetPhysicalSize()/1024;
		}
	}
}

// used by texture viewer shader edit integration
void CShaderEdit::ActiveTextureRefAdd   (grcTexture* texture) { CShaderEdit_ActiveTextureRefAdd(NULL, texture); }
void CShaderEdit::ActiveTextureRefRemove(grcTexture* texture) { CShaderEdit_ActiveTextureRefAdd(texture, NULL); }

static void CShaderEdit_ActiveTextureRefsUpdate()
{
	atMap<grcTexture*,int>::Iterator it = s_activeTextureRefCounts.CreateIterator();
	atArray<grcTexture*> keysToDelete; // let's be safe about this

	for (it.Start(); !it.AtEnd(); it.Next())
	{
		int& refCount = it.GetData();

		if (refCount <= 0)
		{
			refCount--;

			if (refCount < 3) // release after 3 frames
			{
				keysToDelete.PushAndGrow(it.GetKey());
			}
		}
	}

	for (int i = 0; i < keysToDelete.GetCount(); i++)
	{
		grcTexture* key = keysToDelete[i];
		s_activeTextureRefCounts.Delete(key);
		s_activeTextureCount--;
		s_activeTextureDataSize -= key->GetPhysicalSize()/1024;
		key->Release();
	}
}

#if __DEV
static int& CShaderEdit_ActiveTextureRefsGetCount()
{
	return s_activeTextureCount;
}

static int& CShaderEdit_ActiveTextureRefsGetDataSize()
{
	return s_activeTextureDataSize;
}
#endif // __DEV

void CShaderEdit::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		g_ShaderEdit::Instantiate();
		
		for(int i=0;i<NUM_TEXTUREDEBUGMODE;i++)
		{
			const strIndex si = strPackfileManager::RegisterIndividualFile(atVarString("%s.%s", textureDebugTxd[i], TXD_FILE_EXT));
			if (si.IsValid())
			{
				textureDebugTxdSlot[i] = g_TxdStore.FindSlot(textureDebugTxd[i]).Get();
			}
		}
	}
}

void CShaderEdit::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		g_ShaderEdit::Destroy();
	}
}

void CShaderEdit::Update()
{
	//if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG_ALT))
	//{
	//	m_bPickerEnabled = !m_bPickerEnabled;
	//	g_ShaderEdit::GetInstance().PickerToggle();
	//}

	if (!g_PickerManager.IsCurrentOwner(NAME_OF_SHADER_EDIT_PICKER))
	{
		if (m_bPickerEnabled)
			m_bPickerEnabled = false;

		return;
	}

	CEntity *pCurrentEntity = GetCurrentEntity();
	SetInteriorExteriorFlagsForPicker();

	rmcDrawable *pPrevious = m_pSelected;
	m_pSelected = (pCurrentEntity != NULL) ? pCurrentEntity->GetDrawable() : NULL;

	// Setup new set of widgets and backup current data
	if (m_pSelected != pPrevious)
	{
		bkBank* liveEditBank = CDebug::GetLiveEditBank();
		RegenShaderWidgets(liveEditBank);
	}
}

void CShaderEdit::Flush()
{
	g_strStreamingInterface->RequestFlush(true);
}

void CShaderEdit::ForceApply()
{
	g_ShaderEdit::GetInstance().m_applyGlobal = true; 
}

#if __XENON && __DEV
namespace rage {
	extern u32 g_MaxTextureSize;
};
void CShaderEdit::ApplyMaxTextureSize()
{
	g_MaxTextureSize = g_ShaderEdit::GetInstance().GetMaxTexturesize();
}
#endif // __XENON && __DEV

CShaderEdit::CShaderEdit() : m_pSelected(NULL), 
							 m_applyGlobal(false),
							 m_bPickerEnabled(false),

							 m_bTextureDebug(false),
							 m_bAutoApply(false),
							 m_bMatchPickerInteriorExteriorFlagsToPlayer(true),
							 m_textureDebugMode(0),
#if __DEV
							 m_forceMaxTextureSize(0),
							 m_activeTextureRefsUpdate(true),
#endif // __DEV
							 m_pUpdatedSlot(TEX_NONE),
							 m_bRestrictTextureSize(true)
{
	memset(m_pPrevGlobalTex,0,sizeof(grcTexture*)*CShaderEdit::TEX_COUNT);
	memset(m_pGlobalTex,0,sizeof(grcTexture*)*CShaderEdit::TEX_COUNT);
	
	m_pShaderGroup = NULL;
	m_pLiveEditGroup = NULL;
}

CShaderEdit::~CShaderEdit()
{
}

static void UpdateTextureFunc(const grcTexture* texture, const char* name, int modelIndex)
{
	SHADEREDIT_VERBOSE_ONLY(Displayf("> UpdateTextureFunc(tex=\"%s\", name=\"%s\", mi=%d)", texture->GetName(), name, modelIndex));
	CDebugTextureViewerInterface::UpdateTextureForShaderEdit(texture, name, modelIndex);
}

static grcTexture* grcTexture_CustomLoadFunc(const char* filename)
{
	SHADEREDIT_VERBOSE_ONLY(Displayf("> grcTexture_CustomLoadFunc(filename=\"%s\")", filename));
	grcTexture* newTexture = NULL;

	// Note that we have to call "InvalidateIndividualFile" to refresh the streaming system's
	// view of this file (size etc.) since it has just been changed, the naming of this function
	// is a bit odd but it has to be called _before_ RegisterIndividualFile.

	SHADEREDIT_VERBOSE_ONLY(Displayf(">   strPackfileManager::InvalidateIndividualFile"));
	strPackfileManager::InvalidateIndividualFile( filename );

	SHADEREDIT_VERBOSE_ONLY(Displayf(">   strPackfileManager::RegisterIndividualFile"));
	strIndex index = strPackfileManager::RegisterIndividualFile( filename );

	SHADEREDIT_VERBOSE_ONLY(Displayf(">   strStreamingEngine::GetInfo().RequestObject(index=%d, STRFLAG_PRIORITY_LOAD)", index.Get()));
	const bool bRequested = strStreamingEngine::GetInfo().RequestObject( index, STRFLAG_PRIORITY_LOAD );

	SHADEREDIT_VERBOSE_ONLY(Displayf(">   strStreamingEngine::GetLoader().LoadAllRequestedObjects(true)"));
	strStreamingEngine::GetLoader().LoadAllRequestedObjects( true );

	SHADEREDIT_VERBOSE_ONLY(Displayf(">   loading .."));
	if (bRequested)
	{
		const fwTxd* txd = g_TxdStore.GetSafeFromIndex( strLocalIndex(g_TxdStore.GetObjectIndex(index)) );

		if (txd)
		{
			const grcTexture* srcTexture = txd->GetCount() > 0 ? txd->GetEntry(0) : NULL;

			if (srcTexture)
			{
				char name[RAGE_MAX_PATH] = "";
				strcpy(name, strrpbrk(filename, "/\\", filename - 1) + 1); // strip path from TXD name
				if (strrchr(name, '.')) { strrchr(name, '.')[0] = '\0'; } // strip extension
				strcat(name, "/");
				strcat(name, striskip(srcTexture->GetName(), "pack:/")); // append texture name
				if (strrchr(name, '.')) { strrchr(name, '.')[0] = '\0'; } // strip extension

				SHADEREDIT_VERBOSE_ONLY(Displayf(">   cloning texture \"%s\" %dx%d -> \"%s\" ..", srcTexture->GetName(), srcTexture->GetWidth(), srcTexture->GetHeight(), name));
				newTexture = srcTexture->Clone(name);
				SHADEREDIT_VERBOSE_ONLY(Displayf(">   cloned ok."));

				if (newTexture == NULL)
				{
					Displayf("grcTexture_CustomLoadFunc: grcTexture::Clone failed!");
				}
			}
			else
			{
				Displayf("grcTexture_CustomLoadFunc: no texture in TXD!");
			}
		}
		else
		{
			Displayf("grcTexture_CustomLoadFunc: g_TxdStore.GetSafeFromIndex failed!");
		}
	}
	else
	{
		Displayf("grcTexture_CustomLoadFunc: strStreamingInfoManager::RequestObject failed!");
	}

	strStreamingEngine::GetInfo().RemoveObject( index );
	strStreamingEngine::GetInfo().UnregisterObject( index );

	return newTexture;
}

CEntity *CShaderEdit::GetCurrentEntity()
{
	CEntity *pCurrentEntity = NULL;
	if (g_PickerManager.IsCurrentOwner(NAME_OF_SHADER_EDIT_PICKER) || DebugEditing::ShaderEditing::IsEnabled())
	{
		pCurrentEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	}

	return pCurrentEntity;
}

void CShaderEdit::SetInteriorExteriorFlagsForPicker()
{
	if (!m_bMatchPickerInteriorExteriorFlagsToPlayer)
	{
		return;
	}

	bool bSearchExteriors = true;
	bool bSearchInteriors = true;
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		if (pPlayerPed->GetIsInInterior())
		{
			bSearchExteriors = false;
		}
		else
		{
			bSearchInteriors = false;
		}
	}

	fwIntersectingEntitiesPickerSettings ShaderEditIntersectingEntitiesSettings(bSearchExteriors, bSearchInteriors, false, 100);
	g_PickerManager.SetPickerSettings(INTERSECTING_ENTITY_PICKER, &ShaderEditIntersectingEntitiesSettings);
}

void CShaderEdit::TakeControlOfShaderEditPicker()
{
	fwPickerManagerSettings ShaderEditPickerManagerSettings(INTERSECTING_ENTITY_PICKER, true, false, 
		ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_ANIMATED_BUILDING|ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED|ENTITY_TYPE_MASK_OBJECT, false);
	g_PickerManager.SetPickerManagerSettings(&ShaderEditPickerManagerSettings);

	CGtaPickerInterfaceSettings ShaderEditPickerInterfaceSettings(false, true, false, false);
	ShaderEditPickerInterfaceSettings.m_bDisplayWireframe = false; // don't show wireframe for shader edit, since we need to see the material clearly
	g_PickerManager.SetPickerInterfaceSettings(&ShaderEditPickerInterfaceSettings);

	SetInteriorExteriorFlagsForPicker();

	fwWorldProbePickerSettings ShaderEditWorldProbeSettings(
		ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE
		|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_RAGDOLL_TYPE
		|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE
		|ArchetypeFlags::GTA_PLANT_TYPE|ArchetypeFlags::GTA_PROJECTILE_TYPE
		|ArchetypeFlags::GTA_EXPLOSION_TYPE|ArchetypeFlags::GTA_PICKUP_TYPE
		|ArchetypeFlags::GTA_WEAPON_TEST|ArchetypeFlags::GTA_CAMERA_TEST
		|ArchetypeFlags::GTA_AI_TEST|ArchetypeFlags::GTA_SCRIPT_TEST|ArchetypeFlags::GTA_WHEEL_TEST
		|ArchetypeFlags::GTA_GLASS_TYPE|ArchetypeFlags::GTA_RIVER_TYPE);
	g_PickerManager.SetPickerSettings(WORLD_PROBE_PICKER, &ShaderEditWorldProbeSettings);

	g_PickerManager.TakeControlOfPickerWidget(NAME_OF_SHADER_EDIT_PICKER);
	g_PickerManager.ResetList(false);
}

void CShaderEdit::OnLiveEditToggle()
{
	if (DebugEditing::ShaderEditing::IsEnabled())
	{
		TakeControlOfShaderEditPicker();
	}
	else
	{
		g_PickerManager.SetEnabled(false);
	}
}



void CShaderEdit::InitWidgets()
{
	grcTexture::SetCustomLoadFunc(grcTexture_CustomLoadFunc, TXD_FILE_EXT); // should this happen here?
	grcTexChangeData::SetUpdateTextureFunc(UpdateTextureFunc);

	DebugEditing::ShaderEditing::InitCommands();

	bkBank* liveEditBank = CDebug::GetLiveEditBank();
	m_pLiveEditGroup = liveEditBank->PushGroup("Shader Edit");
		AddWidgets(liveEditBank);
	liveEditBank->PopGroup();
}

void CShaderEdit::AddWidgets(bkBank *bk)
{
	bk->AddToggle("Enable picking", &m_bPickerEnabled, datCallback(MFA(CShaderEdit::PickerToggle), (datBase*)this));
	//bk->AddButton("Use Picker for Shader Edit", datCallback(MFA(CShaderEdit::TakeControlOfShaderEditPicker), (datBase*)this));
	bk->AddToggle("Match Picker Interior/Exterior Flags To Player", &m_bMatchPickerInteriorExteriorFlagsToPlayer);

	CGtaPickerInterface *pGtaPickerInterface = (CGtaPickerInterface*) g_PickerManager.GetInterface();
	bk->AddSlider("Bounding Box Opacity", &pGtaPickerInterface->GetReferenceToBoundingBoxOpacity(), 0, 255, 1);

#if __DEV
	bk->AddToggle("Active Texture Refs Update", &m_activeTextureRefsUpdate);
	bk->AddSlider("Active Texture Count", &CShaderEdit_ActiveTextureRefsGetCount(), 0, 999, 0);
	bk->AddSlider("Active Texture Data (KB)", &CShaderEdit_ActiveTextureRefsGetDataSize(), 0, 99999, 0);
#endif // __DEV

	bk->AddTitle("");

	bk->AddToggle("Enabled Live Editing", &DebugEditing::ShaderEditing::m_Enabled, datCallback(MFA(CShaderEdit::OnLiveEditToggle), (datBase*)this) );
	bk->AddToggle("Isolate selected object", &pGtaPickerInterface->GetReferenceToShowOnlySelectedEntityFlag());

	bk->PushGroup("Global Texture");
	bk->AddButton("Load Global Texture", datCallback(CFA1(CShaderEdit::LoadGlobalTexture), this), "Load Global Texture");
	bk->AddToggle("Restrict Texture Replace by Width", &m_bRestrictTextureSize);
	bk->AddButton("Flush scene", datCallback(CFA1(Flush), this));
	bk->PopGroup();

	RegenShaderWidgets(bk);
}

void CShaderEdit::RegenShaderWidgets(bkBank *bk)
{
	bool unset = false;

	if (m_pShaderGroup != NULL)
	{
		bk->DeleteGroup(*m_pShaderGroup);
		m_pShaderGroup = NULL;
		bk->SetCurrentGroup(*m_pLiveEditGroup);
		unset = true;
	}

	m_pShaderGroup = bk->PushGroup("Shaders", true);
		PopulateWidgets(bk);
	bk->PopGroup();

	if (unset)
	{
		bk->UnSetCurrentGroup(*m_pLiveEditGroup);
	}
}

void CShaderEdit::AddRendererWidgets()
{
	// Texture Analyser
	bkBank *renderer = BANKMGR.FindBank("Renderer");
	renderer->PushGroup("Texture Analyser");
#if __DEV
	renderer->AddCombo(	"Force maximum texture size", 
		(int*)&m_forceMaxTextureSize,
		NUM_FORCETEXTURESIZEMODE,
		textureSizeMode
#if __XENON
		,0,ApplyMaxTextureSize
#endif // __XENON
		);

	renderer->AddSeparator();
#endif // __DEV

	renderer->AddToggle("Enable Texture Debug",&m_bTextureDebug);
	renderer->AddToggle("Enable Auto Apply", &m_bAutoApply);
	renderer->AddCombo("Mode", &m_textureDebugMode,NUM_TEXTUREDEBUGMODE,textureDebugMode);
	renderer->AddButton("Apply", datCallback(CFA1(ForceApply), this));
	renderer->AddButton("Flush scene", datCallback(CFA1(Flush), this));
	renderer->PopGroup();
}

void CShaderEdit::PickerToggle()
{
	if (m_bPickerEnabled)
	{
		TakeControlOfShaderEditPicker();
	}
	else
	{
		g_PickerManager.SetEnabled(false);
		g_PickerManager.TakeControlOfPickerWidget("Default Picker");
	}
}

static atArray<grcTexChangeData>		sTexUpdates(0,64);
static atArray<grcSamplerStateEditData>	ms_SamplerStateEditData(0,32);	// 32 editable samplers for a start

static void ShaderEditWireframeEnable()
{
	CRenderPhaseDebugOverlayInterface::DrawEnabledRef() = true;
	CRenderPhaseDebugOverlayInterface::ColourModeRef() = DOCM_SHADER_EDIT;
}

void CShaderEdit::PopulateWidgets(bkBank* bk)
{
	if (m_pSelected)
	{
		// AddWidgets_WithLoad populates this with relevant info
		// so first clear it
		sTexUpdates.ResetCount();
		ms_SamplerStateEditData.ResetCount();

		grmShaderGroup *shaderGroup = &m_pSelected->GetShaderGroup();
		bkGroup *lastAdded = NULL;

		int totalTexture = 0;
		for (u32 i = 0; i < shaderGroup->GetCount(); i++)
		{
			grmShader *shader = shaderGroup->GetShaderPtr(i);
			grcInstanceData *data = &shader->GetInstanceData();
			
			totalTexture += data->GetNumTextures();
		}
		
		if( sTexUpdates.GetCapacity() < totalTexture )
		{
			sTexUpdates.Reset();
			sTexUpdates.Reserve(totalTexture);
		}
		

		for (u32 i = 0; i < shaderGroup->GetCount(); i++)
		{
			grmShader *shader = shaderGroup->GetShaderPtr(i);
			atString name(shader->GetName());

			// If we got a diffuse texture, use it in the name
			grcEffectVar varId = shader->LookupVar("diffuseTex", FALSE);
			if(varId)
			{
				char texName[256];
				grcTexture *diffuseTex = NULL;
				shader->GetVar(varId, diffuseTex);

				if (diffuseTex != NULL)
				{
					formatf(texName, sizeof(texName), "%s", diffuseTex->GetName());
					fiAssetManager::RemoveExtensionFromPath(texName, sizeof(texName), texName);

					name += " - ";
					name += fiAssetManager::FileName(texName);
				}
			}

			lastAdded = bk->PushGroup(name, true);
			{
				grcInstanceData *data = &shader->GetInstanceData();

				bk->AddToggle("Flash", &data->UserFlags, grmShader::FLAG_FLASH_INSTANCES);
				bk->AddToggle("Disable", &data->UserFlags, grmShader::FLAG_DISABLE_INSTANCES);
				bk->AddToggle("Draw wireframe", &data->UserFlags, grmShader::FLAG_DRAW_WIREFRAME, &ShaderEditWireframeEnable);

				CEntity *pCurrentEntity = GetCurrentEntity();
				const int modelIndex = pCurrentEntity ? pCurrentEntity->GetModelIndex() : fwModelId::MI_INVALID;
				data->AddWidgets_WithLoad(*bk, sTexUpdates, modelIndex, &ms_SamplerStateEditData);
			}
			bk->PopGroup();
		}

		if (lastAdded)
		{
			lastAdded->SetFocus();
		}
	}
}

const atArray<grcTexChangeData>& CShaderEdit::GetChangeData() const
{
	return sTexUpdates;
}

void CShaderEdit::FindChangeData(const char* shaderName, const char* channel, const char* previousTextureName, atArray<grcTexChangeData*>& dataArray)
{
	dataArray.clear();

	for(int arrayIndex = 0; arrayIndex < sTexUpdates.GetCount(); ++arrayIndex)
	{
		grcTexChangeData& data = sTexUpdates[arrayIndex];
		if ( strstr( shaderName, data.m_instance->GetShaderName() ) != NULL )
		{
			if ( channel == NULL || stricmp(channel, data.m_instance->GetVariableName(data.m_varNum)) == 0)
			{
				if ( data.m_instance->HasTexture(data.m_varNum, previousTextureName) == true )
				{
					dataArray.PushAndGrow(&data);
				}
			}
		}
	}
}

void CShaderEdit::ApplyUpdates()
{
	if(!m_bPickerEnabled)
		return;

#if MULTIPLE_RENDER_THREADS
	if (m_pSelected)
	{
		grmShaderGroup *shaderGroup = &m_pSelected->GetShaderGroup();
		for (u32 i = 0; i < shaderGroup->GetCount(); i++)
		{
			grmShader *shader = shaderGroup->GetShaderPtr(i);
			grcInstanceData *data = &shader->GetInstanceData();
			data->CopyDataToMultipleThreads();
		}
	}
#endif

	DEV_ONLY(if (m_activeTextureRefsUpdate))
	{
		CShaderEdit_ActiveTextureRefsUpdate();
	}

	if( m_bTextureDebug && (m_applyGlobal || m_bAutoApply) )
	{
		// load if not already in memory
		strLocalIndex txdSlot = strLocalIndex(textureDebugTxdSlot[m_textureDebugMode]);

		if (g_TxdStore.Get(txdSlot) == NULL)
		{
			CStreaming::LoadObject(txdSlot, g_TxdStore.GetStreamingModuleId());
			g_TxdStore.AddRef(txdSlot, REF_OTHER);
		}
		
		const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
		for (u32 i = 0; i < maxModelInfos; i++)
		{
			CBaseModelInfo* modelInfo = (CBaseModelInfo*) CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));
			if (modelInfo)
			{
				fwTxd *texDict = g_TxdStore.Get(txdSlot);
				for(int i=0;i<texDict->GetCount();i++)
				{
					grcTexture *tex = texDict->GetEntry(i);
					textureSlot slot = GetTextureSlot(tex->GetWidth());
					UpdateTexture(modelInfo->GetDrawable(), slot, tex);
				}
			}	
		}

        UpdatePeds(txdSlot, TEX_NONE, NULL);

		fwTxd* texDict = g_TxdStore.Get(txdSlot);
		for (s32 i = 0; i < texDict->GetCount(); ++i)
		{
			grcTexture* tex = texDict->GetEntry(i);
			textureSlot slot = GetTextureSlot(tex->GetWidth());
            UpdateHDVehicles(slot, tex);
		}

		m_applyGlobal = false;

		return; // Texture Debug disable normal update, so let's just return
	}

	// Do the local update
	for (int i = 0; i < sTexUpdates.GetCount(); i++)
	{
		if (sTexUpdates[i].m_ready)
		{
			grcTexture* oldTexture = NULL;
			grcTexture* newTexture = NULL;

			const grcInstanceData& instance = *sTexUpdates[i].m_instance;
			const grcEffectVar var = instance.GetBasis().GetVarByIndex(sTexUpdates[i].m_varNum);

			instance.GetBasis().GetVar(instance, var, oldTexture);
			sTexUpdates[i].UpdateTextureParams();
			instance.GetBasis().GetVar(instance, var, newTexture);

			CShaderEdit_ActiveTextureRefAdd(oldTexture, newTexture);
		}
	}

	// Do all global changes
	if (m_applyGlobal && m_pGlobalTex != NULL)
	{
		const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
		for (u32 i = 0; i < maxModelInfos; i++)
		{
			CBaseModelInfo* modelInfo = (CBaseModelInfo*) CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));
			if (modelInfo)
			{
				UpdateTexture(modelInfo->GetDrawable(), m_pUpdatedSlot, m_pGlobalTex[m_pUpdatedSlot]);
			}	
		}

        UpdatePeds(strLocalIndex(-1), m_pUpdatedSlot, m_pGlobalTex[m_pUpdatedSlot]);

        UpdateHDVehicles(m_pUpdatedSlot, m_pGlobalTex[m_pUpdatedSlot]);

		if( m_pPrevGlobalTex[m_pUpdatedSlot] )
		{
			m_pPrevGlobalTex[m_pUpdatedSlot]->Release();
			m_pPrevGlobalTex[m_pUpdatedSlot] = NULL;
		}

		m_pUpdatedSlot = TEX_NONE;
		m_applyGlobal = false;
	}
}

void CShaderEdit::UpdateHDVehicles(textureSlot slot, grcTexture* dbgTex)
{
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 count = (s32)pool->GetSize();
	while (count--)
	{
		CVehicle* veh = pool->GetSlot(count);
		if (!veh || !veh->GetVehicleModelInfo())
			continue;

        if (!veh->GetIsCurrentlyHD())
            continue;

        gtaFragType* frag = veh->GetVehicleModelInfo()->GetHDFragType();
        if (!frag)
            continue;

        UpdateTexture(frag->GetCommonDrawable(), slot, dbgTex);
	}
}

void CShaderEdit::UpdatePeds(strLocalIndex txdSlot, textureSlot slot, grcTexture* dbgTex)
{
    if (txdSlot.IsInvalid() && !dbgTex)
        return;

	CPed::Pool* pedPool = CPed::GetPool();
	s32 count = pedPool->GetSize();
	while (count--)
	{
		CPed* ped = pedPool->GetSlot(count);
		if (!ped || !ped->GetPedModelInfo())
			continue;

		if (ped->GetPedModelInfo()->GetIsStreamedGfx())
		{
			CPedStreamRenderGfx* gfx = ped->GetPedDrawHandler().GetPedRenderGfx();
			if (!gfx)
				continue;

			// streamed
			for (s32 i = 0; i < PV_MAX_COMP; ++i)
			{
				grcTexture* tex = gfx->GetTexture(i);
				if (!tex)
					continue;

                if (dbgTex)
				{
					if (slot == GetTextureSlot(tex->GetWidth()))
					{
						UpdatePedTexture(gfx->GetDrawable(i), dbgTex);
					}
				}
				else
				{
                    fwTxd* texDict = g_TxdStore.Get(txdSlot);
                    for (s32 f = 0; f < texDict->GetCount(); ++f)
                    {
                        grcTexture* dbgTex = texDict->GetEntry(f);
                        if (GetTextureSlot(dbgTex->GetWidth()) == GetTextureSlot(tex->GetWidth()))
                        {
                            UpdatePedTexture(gfx->GetDrawable(i), dbgTex);
                        }
                    }
				}
			}
		}
		else
		{
			// pack
			CPedModelInfo* pmi = ped->GetPedModelInfo();

			strLocalIndex DwdIndex = strLocalIndex(pmi->GetPedComponentFileIndex());
			u32 txdIdx = g_DwdStore.GetParentTxdForSlot(DwdIndex).Get();
			fwTxd* pLookupTxd = g_TxdStore.Get(strLocalIndex(txdIdx));
			if (!pLookupTxd)
				continue;

			const CPedVariationData& varData = ped->GetPedDrawHandler().GetVarData();
			if (pmi->GetVarInfo())
			{
				ALIGNAS(8) static char texLookupString[20]  = "XXXX_Diff_000_X_XXX";

				// go through all the component slots and draw the component for this ped
				for (s32 i = 0; i < PV_MAX_COMP; ++i)
				{
					u32 drawblIdx = varData.m_aDrawblId[i];
					if (drawblIdx == PV_NULL_DRAWBL)
						continue;

					CPVDrawblData* pDrawblData = pmi->GetVarInfo()->GetCollectionDrawable(i, drawblIdx);
					if (pDrawblData && (pDrawblData->HasActiveDrawbl()))
					{
						Dwd* pDwd = g_DwdStore.Get(strLocalIndex(pmi->GetPedComponentFileIndex()));
						u32 dwHash = 0;
						rmcDrawable* pDrawable = CPedVariationPack::ExtractComponent(pDwd, (ePedVarComp)(i), drawblIdx,  &dwHash, varData.m_aDrawblAltId[i]);

						CPedVariationPack::BuildDiffuseTextureName(texLookupString, (ePedVarComp)(i), drawblIdx, 'a' + (char)varData.m_aTexId[i], pmi->GetVarInfo()->GetRaceType(i, drawblIdx, varData.m_aTexId[i], pmi));
						u32 texHash = fwTxd::ComputeHash(texLookupString);
						grcTexture*	pLookupTex = pLookupTxd->Lookup(texHash);

                        if (dbgTex)
						{
							if (slot == GetTextureSlot(pLookupTex->GetWidth()))
							{
								UpdatePedTexture(pDrawable, dbgTex);
							}
						}
						else
						{
                            fwTxd* texDict = g_TxdStore.Get(txdSlot);
                            for (s32 f = 0; f < texDict->GetCount(); ++f)
                            {
                                grcTexture* dbgTex = texDict->GetEntry(f);
                                if (GetTextureSlot(dbgTex->GetWidth()) == GetTextureSlot(pLookupTex->GetWidth()))
                                {
                                    UpdatePedTexture(pDrawable, dbgTex);
                                }
                            }
						}
					}
				}
			}
		}
	}
}

void CShaderEdit::SetGlobalTexture(grcTexture *tex, textureSlot slot)
{ 
	m_pUpdatedSlot = slot; 
	m_pPrevGlobalTex[slot] = m_pGlobalTex[slot]; 
	m_pGlobalTex[slot] = tex; 
	m_applyGlobal = true; 
}

CShaderEdit::textureSlot CShaderEdit::GetTextureSlot(int width)
{
	textureSlot slot = TEX_GLOBAL;
	
	if( m_bRestrictTextureSize )
	{
		switch(width)
		{
			case 16:
				slot = TEX_16;
				break;
			case 32:
				slot = TEX_32;
				break;
			case 64:
				slot = TEX_64;
				break;
			case 128:
				slot = TEX_128;
				break;
			case 256:
				slot = TEX_256;
				break;
			case 512:
				slot = TEX_512;
				break;
			case 1024:
				slot = TEX_1024;
				break;
			case 2048:
				slot = TEX_2048;
				break;
			default:
				slot = TEX_GLOBAL;
				break;
		}
	}
	
	return slot;
}


void CShaderEdit::LoadGlobalTexture(CShaderEdit *instance)
{
	char filename[RAGE_MAX_PATH];
	bool fileSelected = BANKMGR.OpenFile(filename, sizeof(filename), "*.dds", false, "Open DDS");

	// Open file with the given name
	if (fileSelected)
	{
		grcTexture *tex = grcTextureFactory::GetInstance().Create(filename);
		textureSlot slot = instance->GetTextureSlot(tex->GetWidth());
		instance->SetGlobalTexture(tex,slot);
	}
}


void CShaderEdit::UpdateTexture(rmcDrawable* drawable, textureSlot texSlot, grcTexture *tex)
{
	if (drawable)
	{
		grmShaderGroup *shaderGroup = &drawable->GetShaderGroup();

		for (u32 j = 0; j < shaderGroup->GetCount(); j++)
		{
			grmShader *shader = shaderGroup->GetShaderPtr(j);
			const char *diffuseSampler[] = {"diffuseTex",
											"DiffuseTexture_layer0",
											"DiffuseTexture_layer1",
											"DiffuseTexture_layer2",
											"DiffuseTexture_layer3",
											"DiffuseTexture_layer4",
											"DiffuseTexture_layer5",
											"DiffuseTexture_layer6",
											"DiffuseTexture_layer7" };
											
			for(int sampId = 0; sampId < 9; sampId++)
			{
				grcEffectVar varId = shader->LookupVar(diffuseSampler[sampId], false);
				if(varId)
				{
					grcTexture *originalTexture = NULL;
					shader->GetVar(varId,originalTexture);
					if( originalTexture  )
					{
						textureSlot slot = GetTextureSlot(originalTexture->GetWidth());
						if(slot == texSlot)
						{
							shader->SetVar(varId, tex);
						}
					}
				}
			}
		}
	}
}

void CShaderEdit::UpdatePedTexture(rmcDrawable* drawable, grcTexture* tex)
{
	if (drawable)
	{
		grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();

		for (u32 j = 0; j < shaderGroup->GetCount(); j++)
		{
			grmShader* shader = shaderGroup->GetShaderPtr(j);
			grcEffectVar varId = shader->LookupVar("diffuseTex", false);
			if(varId)
			{
				shader->SetVar(varId, tex);
			}
		}
	}
}

#endif // __BANK
