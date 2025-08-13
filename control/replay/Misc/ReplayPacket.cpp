//=====================================================================================================
// name:		ReplayPacket.cpp
//=====================================================================================================

#include "control/replay/Misc/ReplayPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "camera/caminterface.h"
#include "camera/replay/ReplayDirector.h"

#include "control/replay/ReplayInternal.h"
#include "frontend/NewHud.h"
#include "game/clock.h"
#include "game/weather.h"
#include "vehicles/vehiclepopulation.h"

#if !RSG_ORBIS
#include "renderer/WaterUpdateDynamicCommon.h"
#endif // !RSG_ORBIS
#include "renderer/water.h"
#include "renderer/PostProcessFXHelper.h"
#include "Renderer/occlusion.h"
#include "Renderer/PostScan.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"

#include "TimeCycle/TimeCycle.h"
#include "scene/MapChange.h"

#include "scene/InstancePriority.h"

#include "Vfx/Misc/DistantLights.h"
#include "Vfx\VfxHelper.h"
#include "Vfx/Systems/VfxWheel.h"
#include "Vfx/Systems/VfxPed.h"

//CompileTimeAssert(DynamicWaterGridSize == DYNAMICGRIDELEMENTS);

extern CVfxWheel g_vfxWheel;

//////////////////////////////////////////////////////////////////////////
void CPacketBase::Print() const
{
	replayDebugf1("Packet ID:	%u", m_PacketID);
}


//////////////////////////////////////////////////////////////////////////
void CPacketBase::PrintXML(FileHandle handle) const
{
	char str[1024];
	snprintf(str, 1024, "\t\t<ID>%u</ID>\n", m_PacketID);
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<ShouldPreload>%s</ShouldPreload>\n", ShouldPreload() ? "True" : "False");
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
CPacketBase* CPacketBase::TraversePacketChain(u32 count, bool ASSERT_ONLY(validateLastPacket)) const
{
	CPacketBase *pPacketPtr = (CPacketBase *)this;
	replayAssertf(pPacketPtr->ValidatePacket(), "CPacketBase::TraversePacketChain()...Invalid packet encountered.");

	while(count--)
	{
		CPacketBase *pNextPacket = (CPacketBase * )((u8 *)pPacketPtr + pPacketPtr->GetPacketSize());
	#if __ASSERT
		if(count != 0)
			replayAssertf(pNextPacket->ValidatePacket(), "CPacketBase::TraversePacketChain()...Invalid packet encountered.");
		else if(validateLastPacket == true)
			replayAssertf(pNextPacket->ValidatePacket(), "CPacketBase::TraversePacketChain()...Invalid packet encountered.");
	#endif // __ASSERT

		pPacketPtr = pNextPacket;
	}
	return pPacketPtr;
}

//////////////////////////////////////////////////////////////////////////
CPacketEvent::CPacketEvent(const eReplayPacketId packetID, tPacketSize size, tPacketVersion version)
	: CPacketBase(packetID, size, version)
	REPLAY_GUARD_ONLY(, m_HistoryGuard(REPLAY_GUARD_HISTORY))
{
	m_GameTime	= 0;
	m_unused = 0;
}


//////////////////////////////////////////////////////////////////////////
void CPacketEvent::Store(const eReplayPacketId packetID, tPacketSize size, tPacketVersion version)
{
	CPacketBase::Store(packetID, size, version);
	REPLAY_GUARD_ONLY(m_HistoryGuard = REPLAY_GUARD_HISTORY;)
	m_GameTime	= 0;
	m_unused = 0;
}


//////////////////////////////////////////////////////////////////////////
void CPacketEvent::PrintXML(FileHandle handle) const
{
	CPacketBase::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<GameTime>%u</GameTime>\n", m_GameTime);
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<IsPlayed>%s</IsPlayed>\n", IsPlayed() == true ? "True" : "False");
	CFileMgr::Write(handle, str, istrlen(str));

	if(IsCancelled())
	{
		snprintf(str, 1024, "\t\t<IsCancelled>True</IsCancelled>\n");
		CFileMgr::Write(handle, str, istrlen(str));
	}

	if(IsInitialStatePacket())
	{
		snprintf(str, 1024, "\t\t<IsInitialStatePacket>True</IsInitialStatePacket>\n");
		CFileMgr::Write(handle, str, istrlen(str));
	}
}


//////////////////////////////////////////////////////////////////////////
tPacketVersion	CPacketFrame_V1 = 1;
tPacketVersion	CPacketFrame_V2 = 2;
tPacketVersion	CPacketFrame_V3 = 3;
void CPacketFrame::Store(FrameRef frame)
{
	PACKET_EXTENSION_RESET(CPacketFrame);
	CPacketBase::Store(PACKETID_FRAME, sizeof(CPacketFrame), CPacketFrame_V3);

	m_frameRef				= frame;

	m_nextFrameDiffBlock	= 0;
	m_nextFrameOffset		= 0;
	m_prevFrameDiffBlock	= 0;
	m_previousFrameOffset	= 0;

	m_offsetToEvents		= 0;

	m_frameFlags			= 0;

	m_populationValues.m_innerBandRadiusMin = CVehiclePopulation::GetKeyholeShape().GetInnerBandRadiusMin();
	m_populationValues.m_innerBandRadiusMax = CVehiclePopulation::GetKeyholeShape().GetInnerBandRadiusMax();
	m_populationValues.m_outerBandRadiusMin = CVehiclePopulation::GetKeyholeShape().GetOuterBandRadiusMin();
	m_populationValues.m_outerBandRadiusMax = CVehiclePopulation::GetKeyholeShape().GetOuterBandRadiusMax();

	m_timeWrapValues.m_timeWrapUI = fwTimer::GetTimeWarpUI();
	m_timeWrapValues.m_timeWrapScript = fwTimer::GetTimeWarpScript();
	m_timeWrapValues.m_timeWrapCamera = fwTimer::GetTimeWarpCamera();
	m_timeWrapValues.m_timeWrapSpecialAbility = fwTimer::GetTimeWarpSpecialAbility();

	m_entityInstancePriority = CInstancePriority::GetCurrentPriority();

	m_disableArtificialLights = CRenderer::GetDisableArtificialLights();
	m_disableArtificialVehLights = CRenderer::GetDisableArtificialVehLights();
	m_useNightVision = PostFX::GetGlobalUseNightVision();
	m_useThermalVision = RenderPhaseSeeThrough::GetState();
	m_unusedBits = 0;
	m_unused[0] = 0;
	m_unused[1] = 0;
	m_unused[2] = 0;
}


//////////////////////////////////////////////////////////////////////////
void CPacketThermalVisionValues::Store()
{
	CPacketBase::Store(PACKETID_THERMALVISIONVALUES, sizeof(CPacketThermalVisionValues));

	m_fadeStartDistance = RenderPhaseSeeThrough::GetFadeStartDistance();
	m_fadeEndDistance = RenderPhaseSeeThrough::GetFadeEndDistance();
	m_maxThickness = RenderPhaseSeeThrough::GetMaxThickness();

	m_minNoiseAmount = RenderPhaseSeeThrough::GetMinNoiseAmount();
	m_maxNoiseAmount = RenderPhaseSeeThrough::GetMaxNoiseAmount();
	m_hiLightIntensity = RenderPhaseSeeThrough::GetHiLightIntensity();
	m_hiLightNoise = RenderPhaseSeeThrough::GetHiLightNoise();

	m_colorNear = RenderPhaseSeeThrough::GetColorNear().GetColor();
	m_colorFar = RenderPhaseSeeThrough::GetColorFar().GetColor();
	m_colorVisibleBase = RenderPhaseSeeThrough::GetColorVisibleBase().GetColor();
	m_colorVisibleWarm = RenderPhaseSeeThrough::GetColorVisibleWarm().GetColor();
	m_colorVisibleHot = RenderPhaseSeeThrough::GetColorVisibleHot().GetColor();

	for(int i = 0; i < 4; ++i)
	{
		m_heatScale[i] = RenderPhaseSeeThrough::GetHeatScale(i);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketThermalVisionValues::Extract() const
{
	RenderPhaseSeeThrough::SetFadeStartDistance(m_fadeStartDistance);
	RenderPhaseSeeThrough::SetFadeEndDistance(m_fadeEndDistance);
	RenderPhaseSeeThrough::SetMaxThickness(m_maxThickness);

	RenderPhaseSeeThrough::SetMinNoiseAmount(m_minNoiseAmount);
	RenderPhaseSeeThrough::SetMaxNoiseAmount(m_maxNoiseAmount);
	RenderPhaseSeeThrough::SetHiLightIntensity(m_hiLightIntensity);
	RenderPhaseSeeThrough::SetHiLightNoise(m_hiLightNoise);

	RenderPhaseSeeThrough::SetColorNear(Color32(m_colorNear));
	RenderPhaseSeeThrough::SetColorFar(Color32(m_colorFar));
	RenderPhaseSeeThrough::SetColorVisibleBase(Color32(m_colorVisibleBase));
	RenderPhaseSeeThrough::SetColorVisibleWarm(Color32(m_colorVisibleWarm));
	RenderPhaseSeeThrough::SetColorVisibleHot(Color32(m_colorVisibleHot));

	for(int i = 0; i < 4; ++i)
	{
		RenderPhaseSeeThrough::SetHeatScale(i, m_heatScale[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketFrame::PrintXML(FileHandle handle) const
{
	char str[1024];
	snprintf(str, 1024, "\t\t<m_frameRef>%u:%u</m_frameRef>\n", m_frameRef.GetBlockIndex(), m_frameRef.GetFrame());
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
void CPacketGameTime::Store(u32 gameTime)
{ 
	PACKET_EXTENSION_RESET(CPacketGameTime);
	CPacketBase::Store(PACKETID_GAMETIME, sizeof(CPacketGameTime));

	m_GameTimeInMs		= gameTime;
	m_SystemTime		= fwTimer::GetSystemTimeInMilliseconds();
	SetPadU8(TimeScaleU8, (u8)(fwTimer::GetTimeWarp() * FLOAT_1_0_TO_U8));
}


//////////////////////////////////////////////////////////////////////////
void CPacketGameTime::PrintXML(FileHandle handle) const
{
	char str[1024];
	snprintf(str, 1024, "\t\t<GameTimeInMs>%u</GameTimeInMs>\n", GetGameTime());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<SystemTime>%u</SystemTime>\n", GetSystemTime());
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<TimeScale>%f</TimeScale>\n", GetTimeScale());
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
void CPacketClock::Store()
{
	PACKET_EXTENSION_RESET(CPacketClock);
	CPacketBase::Store(PACKETID_CLOCK, sizeof(CPacketClock));

	m_hours		= (u8)CClock::GetHour();
	m_mins		= (u8)CClock::GetMinute();
	m_seconds	= (u8)CClock::GetSecond();
}


//////////////////////////////////////////////////////////////////////////
tPacketVersion g_PacketWeather_v1 = 1;
tPacketVersion g_PacketWeather_v2 = 2;

void CPacketWeather::Store()
{ 
	PACKET_EXTENSION_RESET(CPacketWeather);
	//CompileTimeAssert(sizeof(CPacketWeather::WindSettings) == sizeof(rage::WindSettings));
	CPacketBase::Store(PACKETID_WEATHER, sizeof(CPacketWeather), g_PacketWeather_v2);

	m_OldWeatherType	= g_weather.GetPrevTypeIndex();
	m_NewWeatherType	= g_weather.GetNextTypeIndex();
	m_WeatherInterp		= g_weather.GetInterp();
	m_random			= g_weather.GetRandom();
	m_randomCloud		= g_weather.GetRandomCloud();
	m_wetness			= g_weather.GetWetnessNoWaterCameraCheck();
	m_wind				= g_weather.GetWind();

	m_forceOverTimeTypeIndex	=	g_weather.GetOverTimeType();
	m_overTimeInterp			=	g_weather.GetOverTimeInterp();

	m_overTimeTransistionLeft	= g_weather.GetOverTimeLeftTime();
	m_overTimeTransistionTotal	= g_weather.GetOverTimeTotalTime();
	

	for(int i = 0; i < numWindType; ++i)
	{
		sysMemCpy(&m_windSettings[i], &WIND.GetWindSettings(i), sizeof(WindSettings));
		sysMemCpy(&m_gustState[i], &WIND.GetGustState(i), sizeof(GustState));

		const rage::WindState& windState = WIND.GetWindState(i);
		m_windState[i].vBaseVelocity.Store(windState.vBaseVelocity);
		m_windState[i].vGustVelocity.Store(windState.vGustVelocity);
		m_windState[i].vGlobalVariationVelocity.Store(windState.vGlobalVariationVelocity);
	}

	m_useSnowFootVfx = g_vfxPed.GetUseSnowFootVfxWhenUnsheltered();
	m_useSnowWheelVfx = g_vfxWheel.GetUseSnowWheelVfxWhenUnsheltered();

	m_unusedBits = 0;
	m_unused[0] = m_unused[1] = m_unused[2] = 0;
}


//////////////////////////////////////////////////////////////////////////
void CPacketWeather::Extract() const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketWeather) == 0);

	float overTimeTransistionLeft = 0.0f;
	float overTimeTransistionTotal = 0.0f;
	if( GetPacketVersion() >= g_PacketWeather_v1)
	{
		overTimeTransistionLeft = m_overTimeTransistionLeft;
		overTimeTransistionTotal = m_overTimeTransistionTotal;
	}

	g_weather.SetFromReplay(GetOldWeatherType(), GetNewWeatherType(), GetWeatherInterp(), GetWeatherRandom(), GetWeatherRandomCloud(), GetWeatherWetness(),
							m_forceOverTimeTypeIndex, m_overTimeInterp, overTimeTransistionLeft, overTimeTransistionTotal);
	g_weather.SetWind(GetWeatherWind());

	for(int i = 0; i < numWindType; ++i)
	{
		sysMemCpy(&WIND.GetWindSettings(i), &m_windSettings[i], sizeof(WindSettings));
		sysMemCpy(&WIND.GetGustState(i), &m_gustState[i], sizeof(GustState));

		rage::WindState& windState = WIND.GetWindState(i);
		m_windState[i].vBaseVelocity.Load(windState.vBaseVelocity);
		m_windState[i].vGustVelocity.Load(windState.vGustVelocity);
		m_windState[i].vGlobalVariationVelocity.Load(windState.vGlobalVariationVelocity);
	}

	if(GetPacketVersion() >= g_PacketWeather_v2)
	{
		g_vfxPed.SetUseSnowFootVfxWhenUnsheltered(m_useSnowFootVfx);
		g_vfxWheel.SetUseSnowWheelVfxWhenUnsheltered(m_useSnowWheelVfx);
	}
}


//////////////////////////////////////////////////////////////////////////
// Now unused, left for backwards compatibility. Store no longer called, extract may be with old clips though.
// NOTE: To Be Removed at the next packet flattening, now handled by CPacketTimeCycleModifier
void CPacketScriptTCModifier::Store()
{
	PACKET_EXTENSION_RESET(CPacketScriptTCModifier);
	CPacketBase::Store(PACKETID_TC_SCRIPT_MODIFIER, sizeof(CPacketScriptTCModifier));

	/*
	m_bSniperScope = CNewHud::IsSniperSightActive();
	g_timeCycle.GetTimeCycleKeyframeData(m_keyData);
	*/
}

void CPacketScriptTCModifier::Extract() const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketScriptTCModifier) == 0);
	//	g_timeCycle.SetTimeCycleKeyframeData(m_keyData, m_bSniperScope);
}

//////////////////////////////////////////////////////////////////////////

const tPacketVersion CPacketTimeCycleModifier_Version1 = 1;
const tPacketVersion CPacketTimeCycleModifier_Version2 = 2;
const tPacketVersion CPacketTimeCycleModifier_Version3 = 3;
void CPacketTimeCycleModifier::Store()
{
	PACKET_EXTENSION_RESET(CPacketTimeCycleModifier);
	CPacketBase::Store(PACKETID_TIMECYCLE_MODIFIER, sizeof(CPacketTimeCycleModifier), CPacketTimeCycleModifier_Version3);

	m_bSniperScope = ( CNewHud::IsSniperSightActive() || CVfxHelper::IsPlayerInAccurateModeWithScope() ) == true ? 1 : 0;

	m_bAnimPostFxActive		= AnimPostFXManagerReplayEffectLookup::AreAllGameCamStacksIdle(false) == false ? 1 : 0;
	m_bPausePostFxActive	= PAUSEMENUPOSTFXMGR.AreAllPostFXAtIdle() == false ? 1 : 0;

	m_unUsedBits = 0;
	sysMemSet(m_unUsedBytes, 0, sizeof(m_unUsedBytes));

	m_NoOfTCKeys = 0;
	m_NoOfDefaultTCKeys = 0;

	int modIndex = g_timeCycle.GetScriptModifierIndex();
	if( modIndex != -1 )
	{
		m_ScriptModIndex = g_timeCycle.GetModifierKey(modIndex);
	}
	
	modIndex = g_timeCycle.GetTransitionScriptModifierIndex();
	if( modIndex != -1 )
	{
		m_TransistionModIndex = g_timeCycle.GetModifierKey(modIndex);
	}

	CTimeCycle::TCNameAndValue *pKeys = GetTCValues();
	m_NoOfTCKeys = g_timeCycle.GetTimeCycleKeyframeData(pKeys);
	AddToPacketSize(sizeof(CTimeCycle::TCNameAndValue)*m_NoOfTCKeys);

	CTimeCycle::TCNameAndValue *pDefaultKeys = GetTCDefaultValues();
	m_NoOfDefaultTCKeys = g_timeCycle.GetTimeCycleKeyframeData(pDefaultKeys, true);
	AddToPacketSize(sizeof(CTimeCycle::TCNameAndValue)*m_NoOfDefaultTCKeys);
}

void CPacketTimeCycleModifier::Extract() const
{
	//Don't extract time cycle whilst replay is loading a jump
	if(CPauseMenu::GetPauseRenderPhasesStatus())
	{
		return;
	}

	//Apply the normal TC values
	CTimeCycle::TCNameAndValue *pKeys = GetTCValues();	
	g_timeCycle.SetTimeCycleKeyframeData(pKeys, m_NoOfTCKeys, m_bSniperScope);

	if( GetPacketVersion() >= CPacketTimeCycleModifier_Version1 )
	{
		if( ShouldUseDefaultData() )
		{
			//overwrite the normal tc values with ones with out the fx in.
			CTimeCycle::TCNameAndValue *pDefaultKeys = GetTCDefaultValues();
			g_timeCycle.SetTimeCycleKeyframeData(pDefaultKeys, m_NoOfDefaultTCKeys, m_bSniperScope, true);
		}
	}
}

bool CPacketTimeCycleModifier::ShouldUseDefaultData() const
{
	bool bdisableFx = g_timeCycle.ShouldDisableScripFxForFreeCam(m_ScriptModIndex, m_TransistionModIndex);
	bool bgameCameraActive = camInterface::GetReplayDirector().IsRecordedCamera();

	//override the TC frames with default ones if any of the specified script fx is enabled.
	//and a replay modifier is enabled.
	if( g_timeCycle.GetReplayModifierHash() > 0 && bdisableFx )
	{
		return true;
	}

	if( GetPacketVersion() == CPacketTimeCycleModifier_Version2 )
	{
		//Use the default modifiers if any pause or anim post fx are up.
		if( IsAnimPostFxActive() || IsPausePostFxActive() )
		{
			return true;
		}
	}

	//override values if we aren't under water.
	bool isUnderWater =  Water::IsCameraUnderwater();
	if( isUnderWater )
	{
		return true;
	}

	//Always use the normal TC values if the game camera is applied.
	if( bgameCameraActive )
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

atFixedArray<CPacketMapChange::MapChange, REPLAY_MAP_CHANGE_INITIAL_STATE_SIZE> CPacketMapChange::ms_ActiveMapChangesUponPlayStart;


CPacketMapChange::CPacketMapChange()
: CPacketBase(PACKETID_MAP_CHANGE, sizeof(CPacketMapChange))
{
}


CPacketMapChange::~CPacketMapChange()
{

}

//====================================================================================

void CPacketMapChange::Store()
{
	PACKET_EXTENSION_RESET(CPacketMapChange);
	CPacketBase::Store(PACKETID_MAP_CHANGE, sizeof(CPacketMapChange));

	m_NoOfMapChanges = 0;
	MapChange *pMapChanges = GetMapChanges();
	m_NoOfMapChanges = g_MapChangeMgr.GetNoOfActiveMapChanges();

	// Retrieve current state.
	CollectActiveMapChanges(pMapChanges);

	AddToPacketSize(sizeof(MapChange)*m_NoOfMapChanges);
}


void CPacketMapChange::Extract() const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketMapChange) == 0);
	MapChange *pInPacket = GetMapChanges();

	// Attain the state in the packet.
	AttainState(pInPacket, m_NoOfMapChanges);
}

//====================================================================================


void CPacketMapChange::OnBeginPlaybackSession()
{
	replayAssertf(g_MapChangeMgr.GetNoOfActiveMapChanges() < REPLAY_MAP_CHANGE_INITIAL_STATE_SIZE, "CPacketMapChange::OnEntry()...Too many active map changes upon play start!");

	MapChange *pTemp = Alloca(MapChange, REPLAY_MAP_CHANGE_INITIAL_STATE_SIZE);

	// Retrieve state at start of playback.
	CollectActiveMapChanges(pTemp);

	u32 count = g_MapChangeMgr.GetNoOfActiveMapChanges();

	ms_ActiveMapChangesUponPlayStart.Reset();

	for(u32 i=0; i<count; i++)
		ms_ActiveMapChangesUponPlayStart.Append() = pTemp[i];
}

void CPacketMapChange::OnCleanupPlaybackSession()
{
	u32 count = ms_ActiveMapChangesUponPlayStart.GetCount();
	MapChange *pTemp = Alloca(MapChange, REPLAY_MAP_CHANGE_INITIAL_STATE_SIZE);

	for(u32 i=0; i<count; i++)
		pTemp[i] = ms_ActiveMapChangesUponPlayStart[i];

	// Re-attain state at start of playback.
	AttainState(pTemp, count);

	ms_ActiveMapChangesUponPlayStart.Reset();
}

void CPacketMapChange::PrintXML(FileHandle handle) const
{
	char str[1024];
	snprintf(str, 1024, "\t\t<m_NoOfMapChanges>%u</m_NoOfMapChanges>\n", m_NoOfMapChanges);
	CFileMgr::Write(handle, str, istrlen(str));

	MapChange * pChanges = GetMapChanges();
	for(u32 i = 0; i < m_NoOfMapChanges; ++i)
	{
		snprintf(str, 1024, "\t\t<Change>%u, %u, %u, %s</Change>\n", pChanges[i].oldHash, pChanges[i].newHash, pChanges[i].changeType, pChanges[i].bExecuted ? "True" : "False");
		CFileMgr::Write(handle, str, istrlen(str));
	}
}

//====================================================================================

void CPacketMapChange::AttainState(MapChange *pToAttain, u32 toAttainSize)
{
	u32 currentNoOfMapChanges = g_MapChangeMgr.GetNoOfActiveMapChanges();
	MapChange *pCurrentMapChanges = Alloca(MapChange, currentNoOfMapChanges);

	u32 noToAdd = 0;
	MapChange *pToAdd = Alloca(MapChange, toAttainSize);
	u32 noToRemove = 0;
	MapChange *pToRemove = Alloca(MapChange, currentNoOfMapChanges);

	// Retrieve current state.
	CollectActiveMapChanges(pCurrentMapChanges);

	// Determine which map changes to apply.
	StuffInListANotInListB(pToAttain, toAttainSize, pCurrentMapChanges, currentNoOfMapChanges, pToAdd, noToAdd);
	// Determine which ones to remove.
	StuffInListANotInListB(pCurrentMapChanges, currentNoOfMapChanges, pToAttain, toAttainSize, pToRemove, noToRemove);

	// Apply the changes.
	for(int i=0; i<noToRemove; i++)
	{
		spdSphere searchVol;
		pToRemove[i].searchVol.Get(searchVol);
		g_MapChangeMgr.Remove(pToRemove[i].oldHash, pToRemove[i].newHash, searchVol, (CMapChange::eType)pToRemove[i].changeType, false);
	}

	for(int i=0; i<noToAdd; i++)
	{
		spdSphere searchVol;
		pToAdd[i].searchVol.Get(searchVol);
		g_MapChangeMgr.Add(pToAdd[i].oldHash, pToAdd[i].newHash, searchVol, pToAdd[i].bSurviveMapReload, (CMapChange::eType)pToAdd[i].changeType, !pToAdd[i].bExecuted, pToAdd[i].bAffectsScriptObjects);
	}

	// It's possible for a mapchange to be in the activelist and executed later. 
	ExecuteIfRequired(pToAttain, toAttainSize);

}


void CPacketMapChange::CollectActiveMapChanges(MapChange *pMapChanges)
{
	u32 count = g_MapChangeMgr.GetNoOfActiveMapChanges();

	for(u32 i=0; i<count; i++)
	{
		spdSphere searchVol;
		bool surviveMapReload;
		bool affectsScriptObjects;
		bool executed;
		g_MapChangeMgr.GetActiveMapChangeDetails(i, pMapChanges[i].oldHash, pMapChanges[i].newHash, searchVol, surviveMapReload, (CMapChange::eType &)pMapChanges[i].changeType, affectsScriptObjects, executed);
		pMapChanges[i].searchVol.Set(searchVol);
		pMapChanges[i].bSurviveMapReload = surviveMapReload;
		pMapChanges[i].bAffectsScriptObjects = affectsScriptObjects;
		pMapChanges[i].bExecuted = executed;
	}
}


void CPacketMapChange::StuffInListANotInListB(MapChange *pA, u32 sizeA, MapChange *pB, u32 sizeB, MapChange *pOut, u32 &outSize)
{
	for(int i=0; i<sizeA; i++)
	{
		bool isNotInListB = true;

		for(int j=0; j<sizeB; j++)
		{
			if(pA[i] == pB[j])
			{
				isNotInListB = false;
				break;
			}
		}
		if(isNotInListB)
			pOut[outSize++] = pA[i];
	}
}


void CPacketMapChange::ExecuteIfRequired(MapChange *pToAttain, u32 toAttainSize)
{
	for(u32 i=0; i<toAttainSize; i++)
	{
		if(pToAttain[i].bExecuted)
		{
			spdSphere searchVol;
			pToAttain[i].searchVol.Get(searchVol);
			CMapChange *pMapChange =  g_MapChangeMgr.GetActiveMapChange(pToAttain[i].oldHash, pToAttain[i].newHash, searchVol, (CMapChange::eType)pToAttain[i].changeType);
			
			if(pMapChange && !pMapChange->HasBeenExecuted())
			{
 				pMapChange->Execute(false);
			}
		}

		// Are we missing a reverse here?  What if bExecuted is false but HasBeenExecuted() returns true?
	}
}


void CPacketDistantCarState::Store()
{
	PACKET_EXTENSION_RESET(CPacketDistantCarState);
	CPacketBase::Store(PACKETID_DISTANTCARSTATE, sizeof(CPacketDistantCarState));
	

	m_enableDistantCars = g_distantLights.DistantCarsEnabled();
	m_RandomVehicleDensityMult = g_distantLights.DistantCarDensityMul();
}

void CPacketDistantCarState::Extract() const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketDistantCarState) == 0);
	
	g_distantLights.EnableDistantCars(m_enableDistantCars);
	g_distantLights.SetDistantCarDensityMulForReplay(m_RandomVehicleDensityMult);
}


///////////////////////////////////////////////////////////////////

atArray<u32>	CPacketDisabledThisFrame::ms_ModelCullsToStore;
atArray<u32>	CPacketDisabledThisFrame::ms_ModelShadowCullsToStore;
bool			CPacketDisabledThisFrame::ms_DisableOcclusionThisFrame = false;

void CPacketDisabledThisFrame::Store()
{
	PACKET_EXTENSION_RESET(CPacketDisabledThisFrame);
	CPacketBase::Store(PACKETID_DISABLED_THIS_FRAME, sizeof(CPacketDisabledThisFrame), CPacketDisabledThisFrame_V1);

	// Init both these to zero before we call either of the GetModelCulls/GetModelShadowCulls functions.
	m_NoOfModelCulls = 0;
	m_NoOfModelShadowCulls = 0;

	// Model Culls
	u32 *pModelCullHashes = GetModelCulls();
	for(int i=0;i<ms_ModelCullsToStore.size();i++)
	{
		pModelCullHashes[i] = ms_ModelCullsToStore[i];
	}
	m_NoOfModelCulls = ms_ModelCullsToStore.size();
	AddToPacketSize(sizeof(u32)*m_NoOfModelCulls);

	// Model shadow Culls
	u32 *pModelShadowCullHashes = GetModelShadowCulls();
	for(int i=0;i<ms_ModelShadowCullsToStore.size();i++)
	{
		pModelShadowCullHashes[i] = ms_ModelShadowCullsToStore[i];
	}
	m_NoOfModelShadowCulls = ms_ModelShadowCullsToStore.size();
	AddToPacketSize(sizeof(u32)*m_NoOfModelShadowCulls);

	// Occlusion Disable
	m_DisableOcclusionThisFrame = ms_DisableOcclusionThisFrame;

	Reset();
}

void CPacketDisabledThisFrame::Extract()	const
{
	replayAssert(GET_PACKET_EXTENSION_VERSION(CPacketDisabledThisFrame) == 0);

	// Model Culls
	u32 *pModelCullHashes = GetModelCulls();
	for(int i=0;i<m_NoOfModelCulls;i++)
	{
		u32 thisModelHash = pModelCullHashes[i];

		if(!g_PostScanCull.IsModelCulledThisFrame(thisModelHash))
		{
			g_PostScanCull.EnableModelCullThisFrame(thisModelHash);
		}
	}

	if(GetPacketVersion() >= CPacketDisabledThisFrame_V1)
	{
		// Model shadow Culls
		u32 *pModelShadowCullHashes = GetModelShadowCulls();
		for(int i=0;i<m_NoOfModelShadowCulls;i++)
		{
			u32 thisModelHash = pModelShadowCullHashes[i];

			if(!g_PostScanCull.IsModelShadowCulledThisFrame(thisModelHash))
			{
				g_PostScanCull.EnableModelShadowCullThisFrame(thisModelHash);
			}
		}
	}

	// Occlusion Disable
	if( m_DisableOcclusionThisFrame )
	{
		COcclusion::ScriptDisableOcclusionThisFrame();
	}
}

#endif // GTA_REPLAY
