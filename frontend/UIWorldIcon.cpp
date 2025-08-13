
// rage
#include "fwsys/timer.h"

// game
#include "Camera/CamInterface.h"
#include "Camera/Helpers/Frame.h"
#include "Camera/viewports/ViewportManager.h"
#include "Frontend/ui_channel.h"
#include "FrontEnd/UIWorldIcon.h"
#include "Frontend/PauseMenu.h"
#include "Peds/ped.h"
#include "Peds/PedFactory.h"
#include "Scene/world/GameWorld.h"
#include "Script/Commands_graphics.h"
#include "Streaming/streaming.h"
#include "Vfx/VfxHelper.h"

#define BG_TEXTURE "In_World_Circle"
#define ICONS_FOLDER "MPInventory"
#define INVALID_QUERY (0)


//OPTIMISATIONS_OFF();

const grcViewport* UIWorldIcon::sm_pViewport = NULL;
bank_float UIWorldIcon::sm_iconWidth = 0.01f;
bank_float UIWorldIcon::sm_iconHeight = 0.01f;

bank_float UIWorldIcon::sm_borderWidth = 0.0f; // deprecated
bank_float UIWorldIcon::sm_borderHeight = 0.0f; // deprecated

bank_float UIWorldIcon::sm_pedHeight = 0.50f;
bank_float UIWorldIcon::sm_vehicleHeight = 0.3f;
bank_float UIWorldIcon::sm_entityHeight = 0.50f;
bank_float UIWorldIcon::sm_aboveScreenBoundry = -0.05f; // deprecated

bank_float UIWorldIcon::sm_maxScaleDistance = 44.72f; // deprecated
bank_float UIWorldIcon::sm_minScaleDistance = 14.14f; // deprecated
bank_float UIWorldIcon::sm_scaleAtMaxDistance = 0.30f; // deprecated
bank_float UIWorldIcon::sm_scaleAtMinDistance = 1.00f; // deprecated

bank_bool UIWorldIcon::sm_defaultShowBG = false; // deprecated
bank_bool UIWorldIcon::sm_defaultShouldClamp = false; // deprecated
bank_bool UIWorldIcon::sm_defaultVisibility = true;
bank_s32 UIWorldIcon::sm_defaultLifetime = 2;
bank_u8 UIWorldIcon::sm_renderDelay = 4;

bank_float UIWorldIcon::sm_distanceToShowText = 20.0f; // deprecated
bank_float UIWorldIcon::sm_defaultTextScale = 0.400f; // deprecated
bank_float UIWorldIcon::sm_defaultTextPosOffsetX = 4.500f; // deprecated
bank_float UIWorldIcon::sm_defaultTextPosOffsetY = -3.020f; // deprecated
bank_u8 UIWorldIcon::sm_defaultTextBGAlpha = 255; // deprecated
bank_bool UIWorldIcon::sm_defaultUseTextBG = true; // deprecated
atFinalHashString UIWorldIcon::sm_defaultIconText(""); // deprecated

#if __BANK
const char* s_testIcon = "MP_SpecItem_Plane";
CSprite2d s_preloadSprite;
bool s_autoPlaceIconsOnPeds = false;
bool s_autoPlaceIconsOnWorld = false;
int s_debugIconColorR = 240;
int s_debugIconColorG = 200;
int s_debugIconColorB = 80;
int s_debugIconColorA = 255;
int s_debugIconBGColorR = 240;
int s_debugIconBGColorG = 200;
int s_debugIconBGColorB = 80;
int s_debugIconBGColorA = 255;
#endif




UIWorldIcon::UIWorldIcon()
{
	Clear();

	m_visibility = false;
	m_flaggedForDeletion = false;
}

UIWorldIcon::~UIWorldIcon()
{
	Clear();
}

void UIWorldIcon::Init(CEntity* pEnt, const char* pIconTexture, int id, scrThreadId scriptId)
{
	m_Entity = pEnt;
	m_entityPos = m_Entity->GetPreviousPosition();
	m_hasEntity = true;
	m_scriptId = scriptId;

	InitCommon(pIconTexture);

	m_id = id;
}

void UIWorldIcon::Init(const Vector3& pos, const char* pIconTexture, int id, scrThreadId scriptId)
{
	m_entityPos = pos;
	m_hasEntity = false;
	m_scriptId = scriptId;

	InitCommon(pIconTexture);

	m_id = id;
}

void UIWorldIcon::InitCommon(const char* pIconTexture)
{
	m_texture = pIconTexture;

	m_visibility = sm_defaultVisibility;
	m_lifetime = sm_defaultLifetime;
	m_flaggedForDeletion = false;

	m_icon.SetTexture(m_texture.c_str(), ICONS_FOLDER);
	m_icon.SetSize(sm_iconWidth, sm_iconHeight);

	SetColor(Color32(0xFFFFFFFF));
}


void UIWorldIcon::Clear()
{
	m_id = INVALID_WORLD_ICON_ID;
	m_scriptId = THREAD_INVALID;

	m_Entity = NULL;
	m_entityPos.Zero();

	m_texture = NULL;
	m_icon.Clear();

	m_visibility = false;
	m_flaggedForDeletion = false;
	m_hasEntity = false;
	
	m_lifetime = 0;
	m_renderDelay = sm_renderDelay;
}

void UIWorldIcon::Update()
{
	if(!IsValid())
	{
		return;
	}

	CEntity* pEnt = m_Entity.Get();

	if(!IsFlaggedForDeletion() && m_visibility && (!m_hasEntity || pEnt) && AssertVerify(sm_pViewport))
	{
		Vector3 pos = m_entityPos;
		if(m_hasEntity && uiVerify(pEnt))
		{
			pos = pEnt->GetPreviousPosition();

			if( pEnt->GetIsTypePed() )
			{
				const CPed* pPed = reinterpret_cast<const CPed*>(pEnt);
				s32 sBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pPed->GetSkeletonData(), (BONETAG_HEAD));
				if(AssertVerify(sBoneIndex != -1))
				{
					Matrix34 mHeadMtx;
					pPed->GetGlobalMtx(sBoneIndex, mHeadMtx);
					pos = mHeadMtx.d;
				}

				pos.z += sm_pedHeight;
			}
			else if(pEnt->GetIsTypeVehicle())
			{
				CVehicle *pVeh = static_cast<CVehicle*>(pEnt);
				if((pVeh->InheritsFromBike() || pVeh->InheritsFromQuadBike() || pVeh->InheritsFromAmphibiousQuadBike()) && pVeh->GetFragmentComponentIndex(VEH_CHASSIS) > -1)
				{
					const phBoundComposite* pBoundComp = pVeh->GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds();
					pos.z += pBoundComp->GetBound(pVeh->GetFragmentComponentIndex(VEH_CHASSIS))->GetBoundingBoxMax().GetZf() + sm_vehicleHeight;
				}
				else
				{
					pos.z += pEnt->GetBaseModelInfo()->GetBoundingBoxMax().z + sm_vehicleHeight;
				}
			}
			else
			{
				pos.z += pEnt->GetBaseModelInfo()->GetBoundingBoxMax().z + sm_entityHeight;
			}
		}

		m_icon.SetPosition(pos);
		m_icon.Update();
	}
}

void UIWorldIcon::AddToDrawList(bool isPlayedAlive)
{
	if(!IsValid())
	{
		return;
	}

	if (!CVfxHelper::ShouldRenderInGameUI())
	{
		return;
	}

	if(!IsFlaggedForDeletion() && isPlayedAlive && m_visibility && (m_renderDelay == 0) && AssertVerify(sm_pViewport))
	{
		m_icon.AddToDrawList();

		//if(m_textLabel.GetHash() != 0 && m_icon.HasTexture() && m_cameraDistance <= sm_distanceToShowText)
		//{
		//	CTextLayout iconText;
		//	iconText.SetScale(Vector2(sm_defaultTextScale, sm_defaultTextScale));
		//	iconText.SetWrap(Vector2(-0.5f, 1.5f));
		//	iconText.SetBackground(sm_defaultUseTextBG);
		//	iconText.SetBackgroundColor(CRGBA(0, 0, 0, sm_defaultTextBGAlpha));
		//	iconText.SetColor(CRGBA(0xFFFFFFFF));

		//	Vector2 textPos = m_entityScreenSpacePos;
		//	textPos.x += (m_icon.GetWidth()*0.5f);
		//	textPos.y -= (iconText.GetCharacterHeight()*0.5f);

		//	if(AssertVerify(sm_pViewport))
		//	{
		//		textPos.x += sm_defaultTextPosOffsetX/(float)sm_pViewport->GetWidth();
		//		textPos.y += sm_defaultTextPosOffsetY/(float)sm_pViewport->GetHeight();
		//	}

		//	const char* pTextLabel = NULL;
		//	OUTPUT_ONLY(pTextLabel = m_textLabel.GetCStr();)
		//	DLC(CRenderTextDC, (iconText, textPos, TheText.Get(m_textLabel.GetHash(), pTextLabel)));
		//}
	}

	if(0 < m_renderDelay)
	{
		--m_renderDelay;
	}

	if(m_lifetime != WORLD_ICON_KEEP_RENDERING
		&& !fwTimer::IsUserPaused())
	{
		if(--m_lifetime <= 0)
		{
			FlagForDeletion();
		}
	}
}

void UIWorldIcon::UpdateViewport()
{
	sm_pViewport = NULL;
	for (int i=0; i<gVpMan.GetNumViewports(); i++)
	{
		CViewport* pVp = gVpMan.GetViewport(i);
		if (pVp && pVp->IsActive() && pVp->IsUsedForNetworking())
		{
			sm_pViewport = &pVp->GetGrcViewport();
			break;
		}
	}
}

void UIWorldIconManager::Open()
{
	if(!SUIWorldIconManager::IsInstantiated())
	{
		SUIWorldIconManager::Instantiate();
	}
}

void UIWorldIconManager::Close()
{
	if(SUIWorldIconManager::IsInstantiated())
	{
		SUIWorldIconManager::Destroy();
	}
}

void UIWorldIconManager::UpdateWrapper()
{
	if(SUIWorldIconManager::IsInstantiated())
	{
		SUIWorldIconManager::GetInstance().Update();
	}
}

void UIWorldIconManager::AddItemsToDrawListWrapper()
{
	if(SUIWorldIconManager::IsInstantiated())
	{
		SUIWorldIconManager::GetInstance().AddItemsToDrawList();
	}
}

UIWorldIconManager::UIWorldIconManager()
{
	m_currentId = 0;
	RemoveAll();
}

UIWorldIconManager::~UIWorldIconManager()
{
	RemoveAll();
}

void UIWorldIconManager::Update()
{
#if __BANK
	if(s_autoPlaceIconsOnPeds || s_autoPlaceIconsOnWorld)
	{
		if(!s_preloadSprite.HasTexture())
		{
			strLocalIndex txdSlot = strLocalIndex(g_TxdStore.FindSlot(ICONS_FOLDER));

			if(AssertVerify(s_testIcon) && txdSlot.Get() >= 0)
			{
				if(CStreaming::HasObjectLoaded(txdSlot, g_TxdStore.GetStreamingModuleId()))
				{
					g_TxdStore.PushCurrentTxd();
					g_TxdStore.SetCurrentTxd(txdSlot);
					s_preloadSprite.SetTexture(s_testIcon);
					g_TxdStore.PopCurrentTxd();
				}
				else
				{
					CStreaming::LoadObject(txdSlot, g_TxdStore.GetStreamingModuleId(), 0);
				}
			}
		}
	}
	else
	{
		s_preloadSprite.Delete();
	}


	if(s_autoPlaceIconsOnPeds)
	{
		CPed* pPlayer = CPedFactory::GetLastCreatedPed();
		if(pPlayer)
		{
			UIWorldIcon* pIcon = Find(pPlayer);

			if(!pIcon)
			{
				pIcon = Add(pPlayer, s_testIcon, THREAD_INVALID);
				if(pIcon)
				{
					Color32 color;
					color.SetRed(s_debugIconColorR);
					color.SetGreen(s_debugIconColorG);
					color.SetBlue(s_debugIconColorB);
					color.SetAlpha(s_debugIconColorA);
					pIcon->SetColor(color);

					color.SetRed(s_debugIconBGColorR);
					color.SetGreen(s_debugIconBGColorG);
					color.SetBlue(s_debugIconBGColorB);
					color.SetAlpha(s_debugIconBGColorA);
					pIcon->SetBGColor(color);
				}
			}
		}
	}
	else if(s_autoPlaceIconsOnWorld)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer)
		{
			UIWorldIcon* pIcon = Add(pPlayer->GetPreviousPosition() + Vector3(50, 50, 10), s_testIcon, THREAD_INVALID);
			if(pIcon)
			{
				Color32 color;
				color.SetRed(s_debugIconColorR);
				color.SetGreen(s_debugIconColorG);
				color.SetBlue(s_debugIconColorB);
				color.SetAlpha(s_debugIconColorA);
				pIcon->SetColor(color);

				color.SetRed(s_debugIconBGColorR);
				color.SetGreen(s_debugIconBGColorG);
				color.SetBlue(s_debugIconBGColorB);
				color.SetAlpha(s_debugIconBGColorA);
				pIcon->SetBGColor(color);
			}
		}
	}
#endif // __BANK


	RemovePendingDeletions();

	UIWorldIcon::UpdateViewport();

	for(int i=0; i<m_worldIcons.size(); ++i)
	{
		m_worldIcons[i].Update();
	}

	CNetGamePlayer* pPlayer = NetworkInterface::GetLocalPlayer();
	if(pPlayer && pPlayer->GetPlayerPed())
	{
		m_isPlayerAlive = !pPlayer->GetPlayerPed()->IsDead();
	}
}

void UIWorldIconManager::AddItemsToDrawList()
{
	if(!CPauseMenu::IsActive())
	{
		for(int i=0; i<m_worldIcons.size(); ++i)
		{
			m_worldIcons[i].AddToDrawList(m_isPlayerAlive);
		}
	}
}

UIWorldIcon* UIWorldIconManager::Add(CEntity* pEnt, const char* pTextureName, scrThreadId scriptId)
{
	UIWorldIcon* pIcon = Find(pEnt);
	int iconId = GetNextId(pIcon);

	if(pIcon)
	{
		pIcon->Init(pEnt, pTextureName, iconId, scriptId);
	}

	return pIcon;
}

UIWorldIcon* UIWorldIconManager::Add(const Vector3& pos, const char* pTextureName, scrThreadId scriptId)
{
	UIWorldIcon* pIcon = Find(pos);
	int iconId = GetNextId(pIcon);

	if(pIcon)
	{
		pIcon->Init(pos, pTextureName, iconId, scriptId);
	}

	return pIcon;
}

void UIWorldIconManager::Remove(CEntity* pEnt)
{
	for(int i=0; i<m_worldIcons.size(); ++i)
	{
		if(m_worldIcons[i].IsEntity(pEnt))
		{
			m_worldIcons[i].FlagForDeletion();
		}
	}
}

void UIWorldIconManager::Remove(const Vector3& pos)
{
	for(int i=0; i<m_worldIcons.size(); ++i)
	{
		if(!m_worldIcons[i].HasEntity() && m_worldIcons[i].IsAtPosition(pos))
		{
			m_worldIcons[i].FlagForDeletion();
		}
	}
}

void UIWorldIconManager::Remove(int iconId)
{
	if(iconId != INVALID_WORLD_ICON_ID)
	{
		for(int i=0; i<m_worldIcons.size(); ++i)
		{
			if(m_worldIcons[i].GetId() == iconId)
			{
				m_worldIcons[i].FlagForDeletion();
			}
		}
	}
}

void UIWorldIconManager::RemovePendingDeletions()
{
	for(int i=0; i<m_worldIcons.size(); ++i)
	{
		UIWorldIcon& rIcon = m_worldIcons[i];
		if(rIcon.IsValid())
		{
			if(rIcon.IsFlaggedForDeletion())
			{
				rIcon.Clear();
			}
			// If there's a valid Id, then the script should exist.
			else if(rIcon.GetScriptId() != THREAD_INVALID && scrThread::GetThread(rIcon.GetScriptId()) == NULL)
			{
				rIcon.Clear();
			}
		}
	}
}

UIWorldIcon* UIWorldIconManager::Find(CEntity* pEnt)
{
	for(int i=0; i<m_worldIcons.size(); ++i)
	{
		if(m_worldIcons[i].IsValid() && m_worldIcons[i].IsEntity(pEnt))
		{
			return &m_worldIcons[i];
		}
	}

	return NULL;
}

UIWorldIcon* UIWorldIconManager::Find(const Vector3& pos)
{
	for(int i=0; i<m_worldIcons.size(); ++i)
	{
		if(m_worldIcons[i].IsValid() && !m_worldIcons[i].HasEntity() && m_worldIcons[i].IsAtPosition(pos))
		{
			return &m_worldIcons[i];
		}
	}

	return NULL;
}

UIWorldIcon* UIWorldIconManager::Find(int iconId)
{
	if(iconId != INVALID_WORLD_ICON_ID)
	{
		for(int i=0; i<m_worldIcons.size(); ++i)
		{
			if(m_worldIcons[i].GetId() == iconId)
			{
				return &m_worldIcons[i];
			}
		}
	}

	return NULL;
}

void UIWorldIconManager::RemoveAll()
{
	for(int i=0; i<m_worldIcons.size(); ++i)
	{
		m_worldIcons[i].Clear();
	}
}

int UIWorldIconManager::GetNextId()
{
	bool found = false;

	do
	{
		if(++m_currentId > 1000000)
		{
			m_currentId = 0;
		}
		
		found = false;

		for(int i=0; i<m_worldIcons.size(); ++i)
		{
			if(m_worldIcons[i].GetId() == m_currentId)
			{
				found = true;
			}
		}
	}while(found);

	return m_currentId;
}

int UIWorldIconManager::GetNextId(UIWorldIcon*& pIcon)
{
	int iconId = INVALID_WORLD_ICON_ID;

	if(pIcon)
	{
		iconId = pIcon->GetId();
	}
	else
	{
		for(int i=0; i<m_worldIcons.size(); ++i)
		{
			if(!m_worldIcons[i].IsValid())
			{
				pIcon = &m_worldIcons[i];
				iconId = GetNextId();
			}
		}
	}

	return iconId;
}

#if __BANK
void UIWorldIconManager::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (!pBank)  // create the bank if not found
	{
		pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
	}

	if (pBank)
	{
		pBank->AddButton("Create World Icon widgets", &UIWorldIconManager::CreateBankWidgets);
	}
}

void UIWorldIconManager::CreateBankWidgets()
{
	static bool bBankCreated = false;

	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if ((!bBankCreated) && (bank))
	{
		UIWorldIconManager::Open();

		datCallback defaultUpdate(CFA(UIWorldIconManager::BankResetIconDefaultsWrapper));

		bank->AddSeparator();
		bank->AddToggle("Add Icons to Peds", &s_autoPlaceIconsOnPeds);
		bank->AddToggle("Add Icons to World", &s_autoPlaceIconsOnWorld);

		bank->AddToggle("Show Icons", &UIWorldIcon::sm_defaultVisibility, defaultUpdate);
		//bank->AddToggle("Show BG", &UIWorldIcon::sm_defaultShowBG, defaultUpdate);
		bank->AddSlider("Lifetime", &UIWorldIcon::sm_defaultLifetime, -1, 10, 1, defaultUpdate);
		bank->AddSlider("Render Delay", &UIWorldIcon::sm_renderDelay, 0, 255, 1);

		bank->AddSeparator();
		bank->AddSlider("Ped Height", &UIWorldIcon::sm_pedHeight, -10.0f, 10.0f, 0.001f);
		bank->AddSlider("Vehicle Height", &UIWorldIcon::sm_vehicleHeight, -10.0f, 10.0f, 0.001f);
		bank->AddSlider("Entity Height", &UIWorldIcon::sm_entityHeight, -10.0f, 10.0f, 0.001f);
		//bank->AddSlider("Abovescreen arrow limit", &UIWorldIcon::sm_aboveScreenBoundry, -0.5f, 0.5f, 0.001f);

		//bank->AddSeparator();
		//bank->AddSlider("Max Scale Distance", &UIWorldIcon::sm_maxScaleDistance, 0.0f, 30000.0f, 0.01f);
		//bank->AddSlider("Min Scale Distance", &UIWorldIcon::sm_minScaleDistance, 0.0f, 30000.0f, 0.01f);
		//bank->AddSlider("Scale At Max Distance", &UIWorldIcon::sm_scaleAtMaxDistance, 0.0f, 2.0f, 0.01f);
		//bank->AddSlider("Scale At Min Distance", &UIWorldIcon::sm_scaleAtMinDistance, 0.0f, 2.0f, 0.01f);

		bank->AddSeparator();
		bank->AddSlider("Icon Width Scaler", &UIWorldIcon::sm_iconWidth, 0, 10, 0.001f);
		bank->AddSlider("Icon Height Scaler", &UIWorldIcon::sm_iconHeight, 0, 10, 0.001f);

		//bank->AddSeparator();
		//bank->AddSlider("Screen Border Width", &UIWorldIcon::sm_borderWidth, 0, 1, 0.01f);
		//bank->AddSlider("Screen Border Height", &UIWorldIcon::sm_borderHeight, 0, 1, 0.01f);

		//bank->AddSeparator();
		//bank->AddSlider("Far distance that Text is Visible", &UIWorldIcon::sm_distanceToShowText, 0, 1000, 1.0f);
		//bank->AddSlider("Text Scale", &UIWorldIcon::sm_defaultTextScale, 0, 5, 0.01f);
		//bank->AddSlider("Text Pos Offset X", &UIWorldIcon::sm_defaultTextPosOffsetX, -50, 50, 0.01f);
		//bank->AddSlider("Text Pos Offset Y", &UIWorldIcon::sm_defaultTextPosOffsetY, -50, 50, 0.01f);
		//bank->AddToggle("Text has BG", &UIWorldIcon::sm_defaultUseTextBG);
		//bank->AddSlider("Text BG Alpha", &UIWorldIcon::sm_defaultTextBGAlpha, 0, 255, 1);
		//bank->AddText("Text", &UIWorldIcon::sm_defaultIconText);

		bank->AddSeparator();
		bank->AddSlider("Icon Color Red", &s_debugIconColorR, 0, 255, 1);
		bank->AddSlider("Icon Color Green", &s_debugIconColorG, 0, 255, 1);
		bank->AddSlider("Icon Color Blue", &s_debugIconColorB, 0, 255, 1);
		bank->AddSlider("Icon Color Alpha", &s_debugIconColorA, 0, 255, 1);
		bank->AddSlider("Icon BG Color Red", &s_debugIconBGColorR, 0, 255, 1);
		bank->AddSlider("Icon BG Color Green", &s_debugIconBGColorG, 0, 255, 1);
		bank->AddSlider("Icon BG Color Blue", &s_debugIconBGColorB, 0, 255, 1);
		bank->AddSlider("Icon BG Color Alpha", &s_debugIconBGColorA, 0, 255, 1);

		bBankCreated = true;
	}
}

void UIWorldIconManager::BankResetIconDefaultsWrapper()
{
	SUIWorldIconManager::GetInstance().BankResetIconDefaults();
}

void UIWorldIconManager::BankResetIconDefaults()
{
	for(int i=0; i<m_worldIcons.size(); ++i)
	{
		m_worldIcons[i].m_visibility = UIWorldIcon::sm_defaultVisibility;
		m_worldIcons[i].m_lifetime = UIWorldIcon::sm_defaultLifetime;
	}
}

#endif // __BANK
//eof
