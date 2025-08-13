
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    trafficlights.cpp
// PURPOSE : Everything having to do with identifying the lights and making them
//			 work.
// AUTHOR :  Obbe.
// CREATED : 04/05/00
//
/////////////////////////////////////////////////////////////////////////////////

// Framework Headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/vector.h"
#include "fwdebug/picker.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "control/gameLogic.h"
#include "vehicleAi/pathFind.h"
#include "control/replay/replay.h"
#include "control/trafficLights.h"
#include "vehicleAi/driverpersonality.h"
#include "game/cheat.h"
#include "game/clock.h"
#include "game/modelIndices.h"
#include "game/weather.h"
#include "game/zones.h"
#include "modelinfo/BaseModelInfoExtensions.h"
#include "objects/Door.h"
#include "Renderer/lights/AsyncLightOcclusionMgr.h"
#include "scene/2deffect.h"
#include "scene/building.h"
#include "scene/RefMacros.h"
#include "scene/world/gameWorld.h"
#include "system/timer.h"
#include "TimeCycle\TimeCycleConfig.h"
#include "Vfx\Misc\BrightLights.h"
#include "Vfx\VfxHelper.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/lights/lights.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/junctions.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

float CTrafficLights::TrafficLightsBrightness;	

#if __BANK
bool CTrafficLights::ms_bRenderTrafficLightsData = false;
bool CTrafficLights::ms_bRenderTrafficLightsDataForSelectedLightOnly = false;
bool CTrafficLights::ms_bRenderTrafficLightsDataActual = false;
bool CTrafficLights::ms_bAlwaysSearchForJunction = false;
bool CTrafficLights::ms_bAlwaysSearchForEntrance = false;
Color32 CTrafficLights::ms_PedWalkColor;
Color32 CTrafficLights::ms_PedDontWalkColor;
#endif

FW_INSTANTIATE_CLASS_POOL(TrafficLightInfos, CONFIGURED_FROM_FILE, atHashString("TrafficLightInfos",0x80e83c46));
AUTOID_IMPL(TrafficLightInfos);

// Light Settings 
static ConfigLightSettings		g_TrafficLightNearLightSettings;
static Vec3V					g_RedColor;
static Vec3V					g_GreenColor;
static Vec3V					g_AmberColor;
static Vec3V					g_PedWalkColor;
static Vec3V					g_PedDontWalkColor;
static float					g_farFadeStart;
static float					g_farFadeEnd;
static float					g_nearFadeStart;
static float					g_nearFadeEnd;

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SetupModelInfo
// PURPOSE :  Helper Method for light damage
/////////////////////////////////////////////////////////////////////////////////
__forceinline u8 TrafficLightCommandToLightComponent( ETrafficLightCommand command )
{
	switch(command)
	{
	case TRAFFICLIGHT_COMMAND_STOP:
		return LIGHT_RED;

	case TRAFFICLIGHT_COMMAND_AMBERLIGHT:
		return LIGHT_AMBER;

	case TRAFFICLIGHT_COMMAND_GO:
	default: 
		return LIGHT_GREEN;
	}
}

void CTrafficLights::SetConfiguration()
{
	if (g_visualSettings.GetIsLoaded())
	{
		g_TrafficLightNearLightSettings.Set( g_visualSettings, "trafficLight.near", "na" );
		g_RedColor = g_visualSettings.GetColor( "trafficLight.red.color" );
		g_GreenColor = g_visualSettings.GetColor( "trafficLight.green.color" );
		g_AmberColor = g_visualSettings.GetColor( "trafficLight.amber.color" );
		g_farFadeStart = g_visualSettings.Get("trafficLight.farFadeStart", 100.0f);
		g_farFadeEnd = g_visualSettings.Get("trafficLight.farFadeEnd", 120.0f);
		g_nearFadeStart = g_visualSettings.Get("trafficLight.nearFadeStart",30.0f);
		g_nearFadeEnd = g_visualSettings.Get("trafficLight.nearFadeEnd",35.0f);
		g_PedWalkColor = g_visualSettings.GetColor( "trafficLight.walk.color" );
		g_PedDontWalkColor = g_visualSettings.GetColor( "trafficLight.dontwalk.color" );

#if __BANK
		ms_PedWalkColor.Set((int)(g_PedWalkColor.GetXf() * 255.0f), (int)(g_PedWalkColor.GetYf() * 255.0f), (int)(g_PedWalkColor.GetZf() * 255.0f));
		ms_PedDontWalkColor.Set((int)(g_PedDontWalkColor.GetXf() * 255.0f), (int)(g_PedDontWalkColor.GetYf() * 255.0f), (int)(g_PedDontWalkColor.GetZf() * 255.0f));
#endif
	}
}



void CTrafficLights::Init()
{
	if( TrafficLightInfos::GetPool() == NULL )
		TrafficLightInfos::InitPool( MEMBUCKET_RENDER );
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitWidgets
// PURPOSE : 
/////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CTrafficLights::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank("Vehicle AI and Nodes");
	Assert(pBank);
	pBank->PushGroup("Traffic lights", false);
		pBank->AddToggle("Render traffic light/Junction data",&ms_bRenderTrafficLightsData);
		pBank->AddToggle("Render traffic light/Junction data for selected light", &ms_bRenderTrafficLightsDataForSelectedLightOnly);
		pBank->AddToggle("Always search for junction",&ms_bAlwaysSearchForJunction);
		pBank->AddToggle("Alwats search for entrance",&ms_bAlwaysSearchForEntrance);
		pBank->AddColor("Ped Boxes: Walk Color",&ms_PedWalkColor);
		pBank->AddColor("Ped Boxes: Don't Walk Color",&ms_PedDontWalkColor);
	pBank->PopGroup();
}
#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Renders some debug lines for the traffic light objects that we are editing.
/////////////////////////////////////////////////////////////////////////////////

void CTrafficLights::Update()
{
	// ---------------------------------------------------------------------
	// Calculate the brightness of the traffic light shadows on the ground

	//@@: range CTRAFFICLIGHTS_UPDATE {
	if (CClock::GetHour() > 20)
	{
		TrafficLightsBrightness = 1.0f;
	}
	else if (CClock::GetHour() > 19)
	{
		//@@: location CTRAFFICLIGHTS_UPDATE_SEVENPM
		TrafficLightsBrightness = CClock::GetMinute() / 60.0f;
	}
	else if (CClock::GetHour() > 6)
	{
		TrafficLightsBrightness = 0.0f;
	}
	else if (CClock::GetHour() > 5)
	{
		//@@: location CTRAFFICLIGHTS_UPDATE_FIVEAM
		TrafficLightsBrightness = 1.0f - CClock::GetMinute() / 60.0f;
	}
	else
	{
		TrafficLightsBrightness = 1.0f;
	}
	//@@: } CTRAFFICLIGHTS_UPDATE


	TrafficLightsBrightness = rage::Max(TrafficLightsBrightness, g_weather.GetWetness());
	TrafficLightsBrightness = rage::Max(TrafficLightsBrightness, g_weather.GetFog());
	TrafficLightsBrightness = rage::Max(TrafficLightsBrightness, g_weather.GetRain());
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ShouldCarStopForLight
// PURPOSE :  This function takes the link that the car is at and works out whether
//			  there is a light that the car should stop for.
/////////////////////////////////////////////////////////////////////////////////

s32 CTrafficLights::CalculateNodeLightCycle(const CPathNode * pTrafficLightNode)
{
	Assert(pTrafficLightNode->NumLinks());

	Vector3 vTrafficNodeCoors, vVec;
	pTrafficLightNode->GetCoors(vTrafficNodeCoors);
	
	Assertf(pTrafficLightNode->IsTrafficLight(), "Path node at %f,%f,%f is not a traffic light, but code was expecting one",vTrafficNodeCoors.x,vTrafficNodeCoors.y,vTrafficNodeCoors.z);

	for(s32 l=0; l<pTrafficLightNode->NumLinks(); l++)
	{
		const CPathNodeLink & link = ThePaths.GetNodesLink(pTrafficLightNode, l);
		if(link.IsShortCut())
			continue;
		const CPathNode * pNextNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);
		if(pNextNode && pNextNode->IsJunctionNode())
		{
			bool bUseOriginalDir = true;

			if (link.m_1.m_bDontUseForNavigation && !pTrafficLightNode->m_2.m_slipJunction)
			{

#if __DEV
				bool bFoundGoodNewNewLink = false;
#endif //__DEV
				CPathNodeLink PreEntranceLink;
				for (int i = 0; i < pTrafficLightNode->NumLinks(); i++)
				{
					PreEntranceLink = ThePaths.GetNodesLink(pTrafficLightNode, i);
					if (PreEntranceLink.m_OtherNode != pNextNode->m_address
						&& !PreEntranceLink.IsShortCut())
					{
#if __DEV
						bFoundGoodNewNewLink = true;
#endif //__DEV
						break;
					}
				}

#if __DEV
				Assert(bFoundGoodNewNewLink);
#endif //__DEV

				const CPathNode* pPreEntranceNode = ThePaths.FindNodePointerSafe(PreEntranceLink.m_OtherNode);
				if (pPreEntranceNode)
				{
					bUseOriginalDir = false;
					vVec = pTrafficLightNode->GetPos() - pPreEntranceNode->GetPos();
					vVec.z = 0.0f;
					vVec.NormalizeFast();
				}
				
			}
			
			if (bUseOriginalDir)
			{
				pNextNode->GetCoors(vVec);
				vVec = vVec - vTrafficNodeCoors;
				vVec.z = 0.0f;
				vVec.NormalizeFast();
			}

			if(Abs(vVec.x) > Abs(vVec.y))
			{
				return TRAFFIC_LIGHT_CYCLE1;
			}
			else
			{
				return TRAFFIC_LIGHT_CYCLE2;
			}
		}
	}
	return TRAFFIC_LIGHT_CYCLE1;
}

int HelperFindOtherValidLinkIndex(const CPathNode* pNode, const int iExceptionLink)
{
	int iRetVal = -1;
	for (int i=0; i < pNode->NumLinks(); i++)
	{
		if (i == iExceptionLink)
		{
			continue;
		}
	
		const CPathNodeLink* pLink = &ThePaths.GetNodesLink(pNode, i);
		if (pLink->IsShortCut())
		{
			continue;
		}

		iRetVal = i;
		break;
	}

	return iRetVal;
}

bool CTrafficLights::ShouldCarStopForLightNode(const CVehicle* pCar, const CNodeAddress& NodeAddr, eTrafficLightColour * pOutLightCol, const bool bAllowGoIfPastLine)
{
	const CPathNode * pNode = ThePaths.FindNodePointerSafe(NodeAddr);

	if(pNode && pNode->IsTrafficLight())
	{
		//early out if this traffic light node is an exit node for the current junction
		//we don't want to obey lights that aren't facing us
		CNodeAddress EntryNode, ExitNode;
		s32 iEntryLane, iExitLane;
		CJunction * pJunction = pCar->GetIntelligence()->GetJunction();
		if (pJunction && pJunction->FindEntryAndExitNodes(pCar, EntryNode, ExitNode, iEntryLane, iExitLane))
		{
			if (ExitNode == pNode->m_address)
			{
				return false;
			}
		}

		//if this light isn't part of our current junction, ignore it
		if (pJunction && !pJunction->ContainsEntranceNode(pNode->m_address))
		{
			return false;
		}

		const s32 TrafficLightsCycle = CalculateNodeLightCycle(pNode);

		if (TrafficLightsCycle != TRAFFIC_LIGHT_NONE)
		{
			bool bCarShouldStop = false;

			bool bJunctionHasPedPhase = false;
			s32 cycleOffset = 0;
			float cycleScale = 1.0f;
			if(pJunction)
			{
				bJunctionHasPedPhase = pJunction->GetHasPedCrossingPhase();
				cycleOffset = pJunction->GetAutoJunctionCycleOffset();
				cycleScale = pJunction->GetAutoJunctionCycleScale();
			}

			if (TrafficLightsCycle == TRAFFIC_LIGHT_CYCLE1)
			{
				const s32 Light = LightForCars1(cycleOffset, cycleScale, bJunctionHasPedPhase);

				u32 turnDir = BIT_NOT_SET;
				if (Light == LIGHT_AMBER)	//only do this if we might use the results
				{
					turnDir = pCar->GetIntelligence()->GetJunctionTurnDirection();
				}

				if (Light == LIGHT_RED || (Light == LIGHT_AMBER && !CDriverPersonality::RunsAmberLights(pCar->GetDriver(), pCar, turnDir)))
				{
					bCarShouldStop = true;
				}
				if(pOutLightCol)
					*pOutLightCol = (eTrafficLightColour)Light;
			}
			else if (TrafficLightsCycle == TRAFFIC_LIGHT_CYCLE2)
			{
				const s32 Light = LightForCars2(cycleOffset, cycleScale, bJunctionHasPedPhase);

				u32 turnDir = BIT_NOT_SET;
				if (Light == LIGHT_AMBER)	//only do this if we might use the results
				{
					turnDir = pCar->GetIntelligence()->GetJunctionTurnDirection();
				}

				if (Light == LIGHT_RED || (Light == LIGHT_AMBER && !CDriverPersonality::RunsAmberLights(pCar->GetDriver(), pCar, turnDir)))
				{
					bCarShouldStop = true;
				}
				if(pOutLightCol)
					*pOutLightCol = (eTrafficLightColour)Light;
			}
			else if (TrafficLightsCycle == TRAFFIC_LIGHT_STOPSIGN)
			{
				// TODO: put new stop-sign/give-way code here
			}

			if ( bCarShouldStop )
			{	
				// We are dealing with a red or amber light.
				// Now we have to check whether the car is in the right area. (Just before the node)
				
				//unless the caller has told us they don't care
				if (!bAllowGoIfPastLine)
				{
					return true;
				}

				//const s32 nextNode = Clamp(node+1, 0, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);
				const CPathNode * pNodeNew = ThePaths.FindNodePointerSafe(pCar->GetIntelligence()->GetJunctionNode());

				if(pNodeNew)
				{
					//carNodeCoorsNew and Old might change to point to the link ahead of the junction,
					//but trafficLightCoors will always represent the position of the light
					Vector3 carNodeCoorsNew, carNodeCoorsOld, trafficLightCoors;
					pNodeNew->GetCoors(carNodeCoorsNew);	
					pNode->GetCoors(carNodeCoorsOld);
					pNode->GetCoors(trafficLightCoors);

					s16 iEntranceLinkIndex = -1;
					if (ThePaths.FindNodesLinkIndexBetween2Nodes(pNode, pNodeNew, iEntranceLinkIndex))
					{
						const CPathNodeLink* pEntranceLink = &ThePaths.GetNodesLink(pNode, iEntranceLinkIndex);
						Assert(pEntranceLink);
						if (pEntranceLink->m_1.m_bDontUseForNavigation)
						{
#if __ASSERT
							const u32 nNumValidLinks = pNode->FindNumberNonShortcutLinks();
							Assertf(nNumValidLinks == 2, "Junction entrance has too many links! Region: %d Index: %d Coords: %.2f %.2f %.2f", NodeAddr.GetRegion(), NodeAddr.GetIndex(), pNode->GetPos().x, pNode->GetPos().y, pNode->GetPos().z);
#endif //__ASSERT
							//don't use the direction of this link. rather use the dir of the link behind us
							const Vector3 potentialCarNodeCoorsNew = carNodeCoorsOld;
							
							const int iPreEntranceLink = HelperFindOtherValidLinkIndex(pNode, iEntranceLinkIndex);
							const CPathNodeLink* pPreEntranceLink = &ThePaths.GetNodesLink(pNode, iPreEntranceLink);
							Assert(pPreEntranceLink);

							const CPathNode* pPreEntranceNode = ThePaths.FindNodePointerSafe(pPreEntranceLink->m_OtherNode);
							
							if (pPreEntranceNode)
							{
								pPreEntranceNode->GetCoors(carNodeCoorsOld);
								carNodeCoorsNew = potentialCarNodeCoorsNew;
							}
						}
					}

					// Make sure we aren't already past the node.
					Vector3	diff = carNodeCoorsNew - carNodeCoorsOld;
					diff.z = 0.0f;
					diff.Normalize();

					const float fBoundingBoxY = pCar->GetVehicleModelInfo()->GetBoundingBoxMax().y;
					const Vector3 vVehPosition = VEC3V_TO_VECTOR3(pCar->GetVehiclePosition()) + (VEC3V_TO_VECTOR3(pCar->GetVehicleForwardDirection()) * fBoundingBoxY);
					Vector3 vLightToCar = vVehPosition - trafficLightCoors;
					vLightToCar.z = 0.0f;
					const float DotPr = DotProduct( vLightToCar, diff );

					//we can be up to 1 car length over the line
					const float fVehHalfLengthMultiplier = pCar->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) ? 1.0f : 2.0f;
					if (DotPr < fBoundingBoxY * fVehHalfLengthMultiplier /*&&  DotPr > -14.0f*/)
					{
						return true;
					}
				}
			}
			else
			{
				return false;		// If we've checked one set of lights don't check another. Cars can get stuck with 2 consecutive sets of lights.
			}
		}
	}
	return false;
}

u32 FindTrafficLightTime(s32 timeOffset, float timeScale, bool pedPhase)
{
	u32 Time = NetworkInterface::GetSyncedTimeInMilliseconds() + timeOffset;
	if (timeScale != 1.0f)
	{
		Time = (int)((float)Time * timeScale);
	}
	return Time % (pedPhase ? LIGHTDURATION_CYCLETIME_PED : LIGHTDURATION_CYCLETIME);
}

/// Cycle for the lights (Red, Amber, Green):

// Time:  0   5000 6000 11000 12000 15384 // THIS IS WRONG.... WORK IT OUT
// Cars1	G		A		R		R		R		R
// Cars2	R		R		G		A		R		R
// Peds		R		R		R		R		G		A

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : LightForCars1
// PURPOSE :  Returns the state of the lights for cars (1).
//			  0 = Green, 1 = Amber, 2 = Red, 3 = off
/////////////////////////////////////////////////////////////////////////////////

u32 CTrafficLights::LightForCars1(s32 timeOffset, float timeScale, bool pedPhase)
{
	u32 Time = FindTrafficLightTime(timeOffset, timeScale, pedPhase);

	if (Time < LIGHTDURATION_LONGERGREEN) return (LIGHT_GREEN);
	if (Time < LIGHTDURATION_LONGERGREEN+LIGHTDURATION_AMBER) return (LIGHT_AMBER);
	return (LIGHT_RED);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : LightForCars2
// PURPOSE :  Returns the state of the lights for cars (2).
//			  0 = Green, 1 = Amber, 2 = Red, 3 = off
/////////////////////////////////////////////////////////////////////////////////

u32 CTrafficLights::LightForCars2(s32 timeOffset, float timeScale, bool pedPhase)
{
	u32 Time = FindTrafficLightTime(timeOffset, timeScale, pedPhase);

	if (Time < LIGHTDURATION_LONGERGREEN+LIGHTDURATION_AMBER) return (LIGHT_RED);
	if (Time < LIGHTDURATION_LONGERGREEN+LIGHTDURATION_AMBER+LIGHTDURATION_SHORTERGREEN) return (LIGHT_GREEN);
	if(pedPhase)
	{
		if (Time < LIGHTDURATION_LONGERGREEN+LIGHTDURATION_AMBER+LIGHTDURATION_AMBER+LIGHTDURATION_SHORTERGREEN) return (LIGHT_AMBER);
		return (LIGHT_RED);
	}
	return (LIGHT_AMBER);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : LightForPeds
// PURPOSE :  Returns the state of the lights for peds.
//			  0 = Green, 1 = Amber, 2 = Red
/////////////////////////////////////////////////////////////////////////////////

u32 CTrafficLights::LightForPeds(s32 timeOffset, float timeScale, float dirX, float dirY, bool pedPhase, float safeTimeRatio)
{
	u32 Time = FindTrafficLightTime(timeOffset, timeScale, pedPhase);
	// If the junction has a ped phase
	if( pedPhase )
	{
		// The time must be past both direction green light times
		if( Time > LIGHTDURATION_LONGERGREEN+LIGHTDURATION_AMBER+LIGHTDURATION_SHORTERGREEN+LIGHTDURATION_AMBER )
		{
			// The time must be before the safety time remaining to the end
			// Note that safeTimeRatio may be zero.
			if( Time < (LIGHTDURATION_CYCLETIME_PED - safeTimeRatio*(LIGHTDURATION_PEDS + LIGHTDURATION_AMBER)) )
			{
				// traffic stopped for peds, return red light so peds cross
				return LIGHT_RED;
			}
		}
		// some traffic is moving, return green light so peds wait
		return LIGHT_GREEN;
	}

	// otherwise, the junction has no ped phase:

	// Compute whether this is crossing lights 1 by direction
	if( rage::Abs(dirY) > rage::Abs(dirX) )
	{
		// The time must be past lights 1 green
		if( Time > LIGHTDURATION_LONGERGREEN+LIGHTDURATION_AMBER )
		{
			// The time must be before the safety time remaining to the end
			// Note that safeTimeRatio may be zero
			if( Time < (LIGHTDURATION_CYCLETIME - safeTimeRatio*(LIGHTDURATION_SHORTERGREEN+LIGHTDURATION_AMBER)) )
			{
				// return red light so peds cross
				return LIGHT_RED;
			}
		}
	}
	else // crossing lights 2
	{
		// The time must be before the safety time remaining to the end
		// Note that safeTimeRation may be zero
		if( Time < LIGHTDURATION_LONGERGREEN+LIGHTDURATION_AMBER - safeTimeRatio*(LIGHTDURATION_LONGERGREEN+LIGHTDURATION_AMBER))
		{
			// return red light so peds cross
			return LIGHT_RED;
		}
	}

	// return green light so peds wait
	return LIGHT_GREEN;
}



//static dev_float radius = 0.12f;
static dev_float aOffset = 0.0f;
static dev_float bOffset = -0.1f;
static dev_float redOffset = TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET;
static dev_float orangeOffset = 0.0f;
static dev_float greenOffset = -TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET;

void CTrafficLights::RenderLight(ETrafficLightCommand command, const Mat34V &matrix, const CEntity *BANK_ONLY(trafficLight), float farFade, float nearFade, float brightlightAlpha, bool BANK_ONLY(turnright) )
{
	const ScalarV aOffsetV = ScalarVFromF32(aOffset);
	const ScalarV bOffsetV = ScalarVFromF32(bOffset);
	const ScalarV redOffsetV = ScalarVFromF32(redOffset);
	const ScalarV orangeOffsetV = ScalarVFromF32(orangeOffset);
	const ScalarV greenOffsetV = ScalarVFromF32(greenOffset);

	BANK_ONLY(Color32 col;)
	Vec3V lightCol;
	Vec3V lightPos;
	BrightLightType_e lightType;

	if( command == TRAFFICLIGHT_COMMAND_STOP )
	{
		BANK_ONLY(col = Color32(0x7f,0x00,0x00,0x7f);)

		lightPos = matrix.GetCol0()*aOffsetV + matrix.GetCol1()*bOffsetV + matrix.GetCol2()*redOffsetV + matrix.GetCol3();

		lightType = BRIGHTLIGHTTYPE_VEH_TRAFFIC_RED;
		lightCol = g_RedColor;
	}
	else if ( command == TRAFFICLIGHT_COMMAND_AMBERLIGHT )
	{
		BANK_ONLY(col = Color32(0x7f,0x40,0x00,0x7f);)

		lightPos = matrix.GetCol0()*aOffsetV + matrix.GetCol1()*bOffsetV + matrix.GetCol2()*orangeOffsetV + matrix.GetCol3();

		lightType = BRIGHTLIGHTTYPE_VEH_TRAFFIC_AMBER;

		lightCol = g_AmberColor;
	}
	else
	{
		BANK_ONLY(col = Color32(0x00,0x7f,0x00,0x7f);)

		lightPos = matrix.GetCol0()*aOffsetV + matrix.GetCol1()*bOffsetV + matrix.GetCol2()*greenOffsetV + matrix.GetCol3();

		lightType = BRIGHTLIGHTTYPE_VEH_TRAFFIC_GREEN;

		lightCol = g_GreenColor;
	}
	
	g_brightLights.Register(lightType, 
							lightPos, 
							matrix.GetCol2(), 
							matrix.GetCol0(), 
							matrix.GetCol1(),
							RCC_VECTOR3(lightCol),
							g_TrafficLightNearLightSettings.coronaSize,
							g_TrafficLightNearLightSettings.coronaHDR * farFade,
							true,
							brightlightAlpha);
#if __BANK && DEBUG_DRAW
	if( ms_bRenderTrafficLightsDataActual )
	{
		Vec3V bonePos = matrix.GetCol3();
		if( turnright ) col.SetAlpha(255);
		grcDebugDraw::Sphere(RCC_VECTOR3(bonePos) + VEC3V_TO_VECTOR3(trafficLight->GetTransform().GetC()) * 1.0f,0.5f,col);
	}
#endif // __DEV

	float nearLightIntensity = TrafficLightsBrightness * g_TrafficLightNearLightSettings.intensity * nearFade;
	if( nearLightIntensity > 0.02f )
	{
		// here we are going to create a spot light that is pushed away from the traffic corona, and then pointed
		// to shine back on it.  We are also going to rotate the spot light 5 degrees towards the ground because
		// it looks better that way - It just makes it easier to see from the ground and helps it pop a bit.
		Matrix34 lightMatrix = MAT34V_TO_MATRIX34( matrix );
		lightMatrix.RotateLocalX( -5  * (PI / 180.0f) );

		lightPos -= matrix.GetCol1();

		Vector3	worldDir, worldTan;
		worldDir = lightMatrix.GetVector(1);
		worldTan = lightMatrix.GetVector(0);

		const float radius = g_TrafficLightNearLightSettings.radius;
		const float falloffExp = g_TrafficLightNearLightSettings.falloffExp;
		const float innerConeAngle = g_TrafficLightNearLightSettings.innerConeAngle;
		const float outerConeAngle = g_TrafficLightNearLightSettings.outerConeAngle;

		CLightSource* pLightSource = CAsyncLightOcclusionMgr::AddLight();
		if (pLightSource)
		{
			pLightSource->Reset();
			pLightSource->SetCommon(	LIGHT_TYPE_SPOT, 
				LIGHTFLAG_NOT_IN_REFLECTION | LIGHTFLAG_DONT_LIGHT_ALPHA,
				RCC_VECTOR3(lightPos), 
				RCC_VECTOR3(lightCol), 
				nearLightIntensity, 
				LIGHT_ALWAYS_ON);
			pLightSource->SetDirTangent(worldDir, worldTan);
			pLightSource->SetRadius(radius);
			pLightSource->SetFalloffExponent(falloffExp);
			pLightSource->SetSpotlight(innerConeAngle, outerConeAngle);
		}
		// Don't call AddSceneLight - CAsyncLightOcclusionMgr will handle adding this light if it passes the
		// occlusion test


		/* this code below is commented out for performance reasons -jkinz */
		/******	

		// now we are going to create another spot light.  This one is going to be pointed down towards
		// the center of the intersection (in the direction the traffic light is facing).  With this one
		// we are hoping to get a bit of light play bouncing off cars as they pass through the intersection
		lightPos += matrix.GetCol1();

		CLightSource carLight( LIGHT_TYPE_SPOT, 
							   LIGHTFLAG_NOT_IN_REFLECTION | LIGHTFLAG_DONT_LIGHT_ALPHA,
							   RCC_VECTOR3(lightPos), 
							   RCC_VECTOR3(lightCol), 
							   nearLightIntensity,
							   LIGHT_ALWAYS_ON);

		lightMatrix.RotateLocalX( -115 * (PI / 180.0f) );
		worldDir = lightMatrix.GetVector(1);
		worldTan = lightMatrix.GetVector(0);

		carLight.SetDirTangent(worldDir, worldTan);
		carLight.SetRadius(9.0f);
		carLight.SetFalloffExponent(24);
		carLight.SetSpotlight(5.0f, 55.0f);
		carLight.SetSpecularFadeDistance(10);
		Lights::AddSceneLight(carLight);

		*******/
	}
}

void CTrafficLights::RenderPedLight(ETrafficLightCommand command, const Mat34V &matrix)
{
	BrightLightType_e lightType;
	Vector3 lightColor;

	if( command == TRAFFICLIGHT_COMMAND_PED_WALK )
	{
		lightType = BRIGHTLIGHTTYPE_PED_TRAFFIC_GREEN;
		lightColor = VEC3V_TO_VECTOR3(g_PedWalkColor);

		BANK_ONLY(lightColor.Set(ms_PedWalkColor.GetRedf(), ms_PedWalkColor.GetGreenf(), ms_PedWalkColor.GetBluef());)	
	}
	else //if ( command == TRAFFICLIGHT_COMMAND_PED_DONTWALK )
	{
		lightType = BRIGHTLIGHTTYPE_PED_TRAFFIC_RED;
		lightColor = VEC3V_TO_VECTOR3(g_PedDontWalkColor);

		BANK_ONLY(lightColor.Set(ms_PedDontWalkColor.GetRedf(), ms_PedDontWalkColor.GetGreenf(), ms_PedDontWalkColor.GetBluef());)
	}
	
	g_brightLights.Register(lightType, 
							matrix.GetCol3(),
							matrix.GetCol2(),
							-matrix.GetCol1(),
							matrix.GetCol0(),
							lightColor,
							1.25f,  //default value
							1.0f,   //default value
							false,
							1.0);	//alpha = 1.0
}

ETrafficLightCommand CTrafficLights::GetTrafficLightCommand(const CPathNode * pathnode, s32 timeOffset, float timeScale, bool pedPhase)
{
	// Probably an old school light.
	ETrafficLightCommand command = TRAFFICLIGHT_COMMAND_AMBERLIGHT;
	
	if( !pathnode->IsGiveWay() )
	{
		int lightType = CalculateNodeLightCycle(pathnode);

		int lightState = LIGHT_GREEN;
		if( lightType == TRAFFIC_LIGHT_CYCLE1 )
		{
			lightState = LightForCars1(timeOffset, timeScale, pedPhase);
		}
		else if( lightType == TRAFFIC_LIGHT_CYCLE2 )
		{
			lightState = LightForCars2(timeOffset, timeScale, pedPhase);
		}

		switch(lightState)
		{
			case LIGHT_RED:
				command = TRAFFICLIGHT_COMMAND_STOP;
				break;
			case LIGHT_AMBER:
				command = TRAFFICLIGHT_COMMAND_AMBERLIGHT;
				break;
			case LIGHT_GREEN:
				command = TRAFFICLIGHT_COMMAND_GO;
				break;
		}
	}
	
	return command;
}

s32 CTrafficLights::GetJunctionIdx(CEntity *entity, const BaseModelInfoBoneIndices* pExtension)
{
	const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
	const Vector3 vEntityDirection = VEC3V_TO_VECTOR3(entity->GetTransform().GetB());
	
	const bool isSingleLight = ( pExtension->GetBoneCount() == 1 );

	return CJunctions::GetJunctionAtPositionForTrafficLight(vEntityPosition, vEntityDirection, isSingleLight);
}

CJunction *CTrafficLights::GetJunction(s32 idx)
{
	if( idx != -1 )
	{
		return CJunctions::GetJunctionByIndex(idx);
	}
	
	return NULL;
}

bool CTrafficLights::SetupTrafficLightInfo(CEntity *entity, TrafficLightInfos *tli, const BaseModelInfoBoneIndices* pExtension, CJunction *junction)
{
	const bool isSingleLight = ( pExtension->GetBoneCount() == 1 );
	atRangeArray<int,4> entranceIds; // We deal with a maximum of 4 entrances per junction/traffic lights.
	
	Vector3 dir = VEC3V_TO_VECTOR3(entity->GetTransform().GetB());
	int entranceCount = junction->FindTrafficLightEntranceIds(dir,isSingleLight,entranceIds);

	int renderLaneId = 0;
	int entranceIdx = -1;
	int curlaneId = 0;
	
	for(int i=0;i<entranceCount;i++)
	{
		entranceIdx = entranceIds[i];
		const CJunctionEntrance &entrance = junction->GetEntrance(entranceIdx);
		const CPathNode * pEntranceNode = ThePaths.FindNodePointerSafe(entrance.m_Node);

		// We have the entrance node.
		// Now we need to find the link to one of the junction nodes.
		for(int jn=0; jn<junction->GetNumJunctionNodes(); jn++)
		{
			const CPathNode * junctionNode = ThePaths.FindNodePointerSafe( junction->GetJunctionNode(jn) );
			if(junctionNode)
			{
				s16 iLinkIndex;
				if( ThePaths.FindNodesLinkIndexBetween2Nodes( pEntranceNode->m_address, junctionNode->m_address, iLinkIndex ) )
				{
					const CPathNodeLink & link = ThePaths.GetNodesLink( pEntranceNode, iLinkIndex );

					const int iNumLanes = link.m_1.m_LanesToOtherNode;

					for(curlaneId = 0;curlaneId<iNumLanes;curlaneId++,renderLaneId++)
					{
						const int boneIdx = pExtension->GetBoneIndice((eTrafficLightComponentId)renderLaneId);
						if( boneIdx != 0xff )
						{
							tli->SetLink(renderLaneId,entranceIdx,curlaneId);
						}
					}

					break;
				}
			}
		}					
	}
	
	if( entranceIdx != -1 )
	{
		// Duplicate the last lane's info to the remaining lights
		for(;renderLaneId<TRAFFIC_LIGHT_COUNT;renderLaneId++)
		{
			tli->SetLink(renderLaneId,entranceIdx,curlaneId);
		}
		
		return true;
	}
	
	return false;
}

#if __BANK

#define MAX_TEMP_ENTRANCES 4

static void DebugDrawJunctionNodes(CEntity *entity, CJunction *junction)
{
	const float dotPMax = -0.9f;

	int entranceCount = 0;
	int entranceIds[4] = { -1,-1,-1,-1 }; // We deal with a maximum of 4 entrances per junction/traffic lights.

	if( junction->HasTrafficLightNodes() )
	{
		grcDebugDraw::Sphere(RCC_VEC3V(junction->GetJunctionCenter()), 1.0f, Color_gold);
	}
	
	// these vars are used in the case that we can't find an entrance for a light
	atRangeArray<int,MAX_TEMP_ENTRANCES> tempEntranceIds;
	float lowestDotProduct = 1.0f;
	int tempEntranceCount = 0;

	for(int i=0;i<junction->GetNumEntrances();i++)
	{
		const CJunctionEntrance &entrance = junction->GetEntrance(i);
		const Vector3 direction(entrance.m_vEntryDir,Vector2::kXY);
		float dotP = DotProduct(direction, VEC3V_TO_VECTOR3(entity->GetTransform().GetB()));
		if( dotP < dotPMax )
		{
			entranceIds[entranceCount] = i;
			entranceCount++;
		}
		else if( entranceCount == 0 )
		{
			// if there haven't been any entrances found yet, start storing up the data
			// for the "best fit" entrances if, in the end, we don't find any
			if( dotP < lowestDotProduct )
			{
				// if the dot product is lower, that means this is even more of an "opposite" entrance
				// than what we had stored beforehand, so lets whipe out all the old entrances and start again
				tempEntranceCount = 0;
				lowestDotProduct = dotP;
				memset(&tempEntranceIds[0], 0, sizeof(int) * MAX_TEMP_ENTRANCES);

				tempEntranceIds[tempEntranceCount++] = i;
			}
			else if( IsClose( dotP, lowestDotProduct, FLT_EPSILON ) )
			{
				tempEntranceIds[tempEntranceCount++] = i;
			}
		}
	}

	// if we found no entrances, lets takea look at the closest matches for consideration...
	if( entranceCount == 0 && tempEntranceCount != 0 )
	{
		entranceCount = tempEntranceCount;

		for( int i = 0; i < tempEntranceCount; i++ )
		{
			entranceIds[i] = tempEntranceIds[i];
		}
	}

	// We deal from the inside entrance to the outside one, so first is the filterleft, then the rest
	for(int i=0;i<entranceCount;i++)
	{
		const CJunctionEntrance &entranceI = junction->GetEntrance(entranceIds[i]);
		bool isFilterLeftLaneI = (entranceI.m_iLeftFilterPhase != -1);
		for(int j=i+1;j<entranceCount;j++)
		{
			const CJunctionEntrance &entranceJ = junction->GetEntrance(entranceIds[j]);
			bool isFilterLeftLaneJ = (entranceJ.m_iLeftFilterPhase != -1);
			if( false == isFilterLeftLaneI && true == isFilterLeftLaneJ )
			{
				int tmp = entranceIds[i];
				entranceIds[i] = entranceIds[j];
				entranceIds[j] = tmp;
			}
		}
	}

	int entranceIdx = -1;

	int renderLaneId = 0;
	for(int i=0;i<entranceCount;i++)
	{
		entranceIdx = entranceIds[i];
		const CJunctionEntrance &entrance = junction->GetEntrance(entranceIdx);
		const CPathNode * pEntranceNode = ThePaths.FindNodePointerSafe(entrance.m_Node);

		if( pEntranceNode )
		{
			Vector3 coords;
			pEntranceNode->GetCoors(coords);

			if( pEntranceNode->IsTrafficLight() )
			{
				grcDebugDraw::Sphere(RCC_VEC3V(coords), 0.5f, Color_yellow);
				grcDebugDraw::Text(RCC_VEC3V(coords),Color32(0xffffff),"T");
			}
			else
			{
				grcDebugDraw::Sphere(RCC_VEC3V(coords), 0.5f, Color_blue);
			}

			// We have the entrance node.
			// Now we need to find the link to one of the junction nodes.
			for(int jn=0; jn<junction->GetNumJunctionNodes(); jn++)
			{
				const CPathNode * junctionNode = ThePaths.FindNodePointerSafe( junction->GetJunctionNode(jn) );
				if(junctionNode)
				{
					junctionNode->GetCoors(coords);

					if( junctionNode->IsTrafficLight() )
					{
						grcDebugDraw::Sphere(RCC_VEC3V(coords), 0.5f, Color_white);
					}
					else
					{
						grcDebugDraw::Sphere(RCC_VEC3V(coords), 0.5f, Color_black);
					}
					
					s16 iLinkIndex;
					if( ThePaths.FindNodesLinkIndexBetween2Nodes( pEntranceNode->m_address, junctionNode->m_address, iLinkIndex ) )
					{
						const CPathNodeLink & link = ThePaths.GetNodesLink( pEntranceNode, iLinkIndex );

						const int iNumLanes = link.m_1.m_LanesToOtherNode;

						for(int curlaneId = 0;curlaneId<iNumLanes;curlaneId++,renderLaneId++)
						{
							// just bump renderLaneId...
						}

						break;
					}
				}
			}
		}		
	}				
}
#endif

void CTrafficLights::RemoveTrafficLightInfo(CEntity *entity)
{
	TrafficLightInfos::Remove(entity);
}

void CTrafficLights::TransferTrafficLightInfo(CEntity *src, CEntity *dst)
{
	TrafficLightInfos *tli = TrafficLightInfos::Get(src);
	if( tli )
	{
		ASSERT_ONLY(TrafficLightInfos *tliDst = TrafficLightInfos::Get(dst));
		Assert(tliDst == NULL);
		TrafficLightInfos::Unlink(src);
		TrafficLightInfos::Link(dst,tli);

		CJunction *junction = GetJunction(tli->GetJunctionIdx());
		if( junction )
		{
			// Link up the traffic light to the junction
			junction->SetTrafficLight(tli->GetTrafficLightIdx(),dst);
		}
	}
}

#if GTA_REPLAY
void CTrafficLights::RenderLightsReplay(CEntity* pEntity, const char* commands)
{
	if(!commands)
		return;

	CBaseModelInfo *pBaseModel = pEntity->GetBaseModelInfo();

	const float entityAlpha = ((float)pEntity->GetAlpha())/255.0f;
	const float camDist = (VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - camInterface::GetPos()).Mag();
	const float farFade = rage::Clamp( (g_farFadeEnd - camDist)/(g_farFadeEnd - g_farFadeStart),0.0f,entityAlpha);
	const float nearFade = rage::Clamp( (g_nearFadeEnd - camDist)/(g_nearFadeEnd - g_nearFadeStart),0.0f,entityAlpha);
	const float brightLightFade = entityAlpha; // simply use entity alpha

	const BaseModelInfoBoneIndices* pExtension = CTrafficLights::GetExtension(pBaseModel);
	if(pExtension == NULL )
	{
		return;
	}

	const int boneCount = pExtension->GetBoneCount();
	for(int boneId = 0;boneId<boneCount;boneId++)
	{
		int boneIdx = pExtension->GetBoneIndice((eTrafficLightComponentId)boneId);
		if( boneIdx != 0xff )
		{
			Mat34V boneMtx;
			CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtx, pEntity, boneIdx);

			ETrafficLightCommand lightCommand = (ETrafficLightCommand)commands[boneId];

			//don't set invalid commands, it will default to green if rendered
			if(lightCommand != TRAFFICLIGHT_COMMAND_INVALID)
			{
				RenderLight(lightCommand, boneMtx, pEntity,farFade,nearFade,brightLightFade,false);
			}
		}
	}

	ETrafficLightCommand pedLightCommand = (ETrafficLightCommand)commands[PED_WALK_BOX];
	if (pedLightCommand != TRAFFICLIGHT_COMMAND_INVALID)
	{
		int boneIdx = pExtension->GetBoneIndice((eTrafficLightComponentId)PED_WALK_BOX);
		if( boneIdx != 0xff )
		{
			Mat34V boneMtx;
			CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtx, pEntity, boneIdx);

			RenderPedLight(pedLightCommand, boneMtx);
		}
	}
}
#endif // GTA_REPLAY


void CTrafficLights::RenderLights(CEntity *pEntity)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		if(pEntity->GetIsTypeDummyObject() || pEntity->GetIsTypeBuilding())
			return; // Bail on dummy's atm as they won't have a replay id
		RenderLightsReplay(pEntity, CReplayMgr::GetTrafficLightCommands(pEntity));
		return;
	}
#endif // GTA_REPLAY

	if (pEntity->m_nFlags.bRenderDamaged) return;			// Damaged. Lights no working

	CBaseModelInfo *pBaseModel = pEntity->GetBaseModelInfo();

	// bail out if the drawable isn't loaded in yet
	if (pBaseModel->GetDrawable() == NULL)
		return;

	const BaseModelInfoBoneIndices* pExtension = CTrafficLights::GetExtension(pBaseModel);
	if(pExtension == NULL )
		return;

	TrafficLightInfos *tli = TrafficLightInfos::Get(pEntity);
	if( NULL == tli )
	{
		tli = TrafficLightInfos::Add(pEntity);
	}

	if(tli && !tli->AreMatricesCached() && pBaseModel->GetFragType())
	{
		tli->CacheBoneMatrices(pEntity, pExtension);
	}

#if __BANK
	ms_bRenderTrafficLightsDataActual = ms_bRenderTrafficLightsData || (ms_bRenderTrafficLightsDataForSelectedLightOnly == true && pEntity == g_PickerManager.GetSelectedEntity());
#endif // __BANK

	const float entityAlpha = ((float)pEntity->GetAlpha())/255.0f;
	const float camDist = (VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - camInterface::GetPos()).Mag();
	const float farFade = rage::Clamp( (g_farFadeEnd - camDist)/(g_farFadeEnd - g_farFadeStart),0.0f,entityAlpha);
	const float nearFade = rage::Clamp( (g_nearFadeEnd - camDist)/(g_nearFadeEnd - g_nearFadeStart),0.0f,entityAlpha);
	const float brightLightFade = entityAlpha; // simply use entity alpha

	static const float fAmberTime = ((float)LIGHTDURATION_AMBER) / 1000.0f;
	
#if __DEV
	if( ms_bRenderTrafficLightsDataActual )
	{
		Mat34V temp;
		u32 boneIdx;
		
		boneIdx = pExtension->GetBoneIndice(TRAFFIC_LIGHT_0);
		if( boneIdx != 0xff )
		{
			CVfxHelper::GetMatrixFromBoneIndex_Lights(temp, pEntity, boneIdx);
			grcDebugDraw::Axis(RCC_MATRIX34(temp),0.5f,true);
		}
				
		boneIdx = pExtension->GetBoneIndice(TRAFFIC_LIGHT_1);
		if( boneIdx != 0xff )
		{
			CVfxHelper::GetMatrixFromBoneIndex_Lights(temp, pEntity, boneIdx);
			grcDebugDraw::Axis(RCC_MATRIX34(temp),0.5f,true);
		}

		boneIdx = pExtension->GetBoneIndice(TRAFFIC_LIGHT_2);
		if( boneIdx != 0xff )
		{
			CVfxHelper::GetMatrixFromBoneIndex_Lights(temp, pEntity, boneIdx);
			grcDebugDraw::Axis(RCC_MATRIX34(temp),0.5f,true);
		}

		boneIdx = pExtension->GetBoneIndice(TRAFFIC_LIGHT_3);
		if( boneIdx != 0xff )
		{
			CVfxHelper::GetMatrixFromBoneIndex_Lights(temp, pEntity, boneIdx);
			grcDebugDraw::Axis(RCC_MATRIX34(temp),0.5f,true);
		}

		boneIdx = pExtension->GetBoneIndice(PED_WALK_BOX);
		if( boneIdx != 0xff )
		{
			CVfxHelper::GetMatrixFromBoneIndex_Lights(temp, pEntity, boneIdx);
			grcDebugDraw::Axis(RCC_MATRIX34(temp),0.5f,true);
		}

		temp = pEntity->GetTransform().GetMatrix();
		grcDebugDraw::Axis(RCC_MATRIX34(temp),1.0f,true);
	}
#endif // __DEV	

	u32 tlOverride = pEntity->GetTrafficLightOverride();
	if( tlOverride != 0x3)
	{
		ETrafficLightCommand command = (ETrafficLightCommand)tlOverride;
		// Traffic light state as been overriden
		const int boneCount = pExtension->GetBoneCount();

		if(tli && tli->AreMatricesCached())
		{
			for(int boneId = 0;boneId<boneCount;boneId++)
			{
				int boneIdx = pExtension->GetBoneIndice((eTrafficLightComponentId)boneId);
				if( boneIdx != 0xff )
				{
					const Mat34V& boneMtx = tli->GetMatrixFromBoneIndex(boneId);
					RenderLight(command, boneMtx, pEntity,farFade,nearFade,brightLightFade,false);
				}
			}
		}
		else
		{
			for(int boneId = 0;boneId<boneCount;boneId++)
			{
				int boneIdx = pExtension->GetBoneIndice((eTrafficLightComponentId)boneId);
				if( boneIdx != 0xff )
				{
					Mat34V boneMtx;
					CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtx, pEntity, boneIdx);
				
					RenderLight(command, boneMtx, pEntity,farFade,nearFade,brightLightFade,false);
				}
			}
		}
		
		return;
	}


	if( tli ) 
	{
		const float JUNCTION_CONNECT_DISTANCE = 100.0f;
		const bool reEvaluateJunction = (tli->IsSetup() && tli->GetSetupDistance() > JUNCTION_CONNECT_DISTANCE && camDist < JUNCTION_CONNECT_DISTANCE);

		if( tli->GetJunctionIdx() == -1 || reEvaluateJunction BANK_ONLY(|| (ms_bAlwaysSearchForJunction && pEntity == g_PickerManager.GetSelectedEntity())) )
		{
			int oldJunction = tli->GetJunctionIdx();
			int newJunction = CTrafficLights::GetJunctionIdx(pEntity, pExtension);

			// if we found a junction that is not the same as what we previously had...
			if( oldJunction != newJunction )
			{
				CJunction *junction = GetJunction(oldJunction);
		
				// if we were already hooked up to a junction, remove ourselves from that one
				if( junction != NULL )
				{
					junction->RemoveTrafficLight(pEntity);
				}

				// set the new junction index and then force a re-setup to connect to the junctions entrances properly
				tli->Reset();
				tli->SetJunctionIdx(newJunction);
				tli->SetIsSetup(false);
			}
			else
			{
				tli->SetSetupDistance(camDist);
			}
		}

		CJunction *junction = GetJunction(tli->GetJunctionIdx());
		
		if( junction )
		{
			// Link up the traffic light to the junction
			int tlIdx = tli->GetTrafficLightIdx();
			if( tlIdx == -1)
			{
				tlIdx = junction->AddTrafficLight(pEntity);
				tli->SetTrafficLightIdx(tlIdx);
			}
		
#if __BANK && DEBUG_DRAW
			if( ms_bRenderTrafficLightsDataActual )
			{
				Vector3 vUp = ZAXIS*0.5f;
				grcDebugDraw::Arrow(RCC_VEC3V(junction->GetJunctionCenter()) + RCC_VEC3V(vUp), pEntity->GetTransform().GetPosition()+RCC_VEC3V(vUp), 0.3f, Color32(0xFFFFFFFF));

				grcDebugDraw::Sphere(junction->GetJunctionCenter(),junction->GetTrafficLightSearchDistance(),Color32(255,0,0,64),false);
				
				int templateIdx = junction->GetTemplateIndex();
				
				if( templateIdx > -1 )
				{
					const CJunctionTemplate &junctionTemplate = CJunctions::GetJunctionTemplate(templateIdx);
					
					for(int i=0;i<junctionTemplate.m_iNumTrafficLightLocations;i++)
					{
						Vector3 pos;
						junctionTemplate.m_TrafficLightLocations[i].GetAsVec3(pos);
						const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
						if( (vEntityPosition - pos).Mag2() <= 6.0f * 6.0f )
						{
							grcDebugDraw::Sphere(pos,6.0f,Color32(255,128,255,64),false,1,16);
						}
						else
						{
							grcDebugDraw::Sphere(pos,6.0f,Color32(64,32,64,64),false,1,16);
						}
					}
				}
			}
#endif // __DEV

#if __BANK
			if( ms_bRenderTrafficLightsDataActual )
				DebugDrawJunctionNodes(pEntity, junction);

			if( ms_bAlwaysSearchForEntrance && pEntity == g_PickerManager.GetSelectedEntity() )
				tli->SetIsSetup(false);
#endif // __BANK

			if( tli->IsSetup() == false && junction->GetNumEntrances() != 0)
			{
				// Go through the junction and attach the right lane to the right light. 
				bool result = SetupTrafficLightInfo(pEntity, tli, pExtension, junction);
				tli->SetIsSetup(result);

				if( result )
				{
					tli->SetSetupDistance(camDist);

#if USE_TLI_COORDINATECHECK
					tli->SetCoord(junction->GetJunctionCenter());
#endif // USE_TLI_COORDINATECHECK
				}
			}
			
			
			if( tli->IsSetup() )
			{
#if USE_TLI_COORDINATECHECK				
				Assertf(tli->GetCoord() == junction->GetJunctionCenter(),"MissMatched Junction/TLI: %p %p",tli,junction);
#endif // USE_TLI_COORDINATECHECK				

				if( junction->GetTemplateIndex() == -1 )
				{
					// Old school: axis commands the light
					REPLAY_ONLY(char commands[8] = {0};)
					const int boneCount = pExtension->GetBoneCount();
					for(int boneId = 0;boneId<boneCount;boneId++)
					{
						if (boneId == PED_WALK_BOX)
							continue;

						int boneIdx = pExtension->GetBoneIndice((eTrafficLightComponentId)boneId);
						if( boneIdx != 0xff )
						{
							const CJunctionEntrance &entrance = junction->GetEntrance(tli->GetEntranceID(boneId));
							const CPathNode * pNode = ThePaths.FindNodePointerSafe(entrance.m_Node);
							if( pNode )
							{
								ETrafficLightCommand  command = GetTrafficLightCommand(pNode,
									junction->GetAutoJunctionCycleOffset(),
									junction->GetAutoJunctionCycleScale(),
									junction->GetHasPedCrossingPhase());

								// if the light is damaged, don't render it
								u8 lightId = TrafficLightCommandToLightComponent(command);
								if( tli->IsLightDamaged((u8)boneId, lightId) )
									continue;

								Mat34V boneMtx;
								if(tli->AreMatricesCached())
								{
									const Mat34V& boneMtx = tli->GetMatrixFromBoneIndex(boneId);
									RenderLight(command, boneMtx, pEntity,farFade,nearFade,brightLightFade,false);
								}
								else
								{
									Mat34V boneMtx;
									CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtx, pEntity, boneIdx);
									RenderLight(command, boneMtx, pEntity,farFade,nearFade,brightLightFade,false);
								}
							
								REPLAY_ONLY(commands[boneId] = (char)command;)
#if __BANK
								if( ms_bRenderTrafficLightsDataActual )
								{
									Color32 col = Color32(0x00,0x7f,0x00,0x7f);
									switch(command)
									{
										case TRAFFICLIGHT_COMMAND_STOP:
											col = Color32(0x7f,0x00,0x00,0x7f);
											break;
										case TRAFFICLIGHT_COMMAND_AMBERLIGHT:
											col = Color32(0x7f,0x40,0x00,0x7f);
											break;
										default:
											break;
									}

									grcDebugDraw::Line(boneMtx.d(),VECTOR3_TO_VEC3V(pNode->GetPos()),col,Color_gold);
								}
#endif // __BANK
							}
						}
					}

					REPLAY_ONLY(ETrafficLightCommand trafficLightCommand =) RenderPedLights(pEntity,pExtension,tli,junction);
#if GTA_REPLAY	
					
					if (trafficLightCommand != TRAFFICLIGHT_COMMAND_INVALID)
					{
						commands[PED_WALK_BOX] = (char)trafficLightCommand;
					}

					if(CReplayMgr::ShouldRecord())
						CReplayMgr::SetTrafficLightCommands(pEntity, commands);
#endif // GTA_REPLAY
				}
				else
				{
					REPLAY_ONLY(char commands[8] = {0};)
					const bool bStopForTrain = junction->ShouldCarsStopForTrain();
					const int boneCount = pExtension->GetBoneCount();
					for(int boneId = 0;boneId<boneCount;boneId++)
					{
						if (boneId == PED_WALK_BOX)
							continue;

						int boneIdx = pExtension->GetBoneIndice((eTrafficLightComponentId)boneId);
						if( boneIdx != 0xff )
						{
							const CJunctionEntrance &entrance = junction->GetEntrance(tli->GetEntranceID(boneId));
							ETrafficLightCommand  command = TRAFFICLIGHT_COMMAND_STOP;
							const float fTimeLeft = junction->GetLightTimeRemaining();

							if( bStopForTrain )
							{
								// remain on stop
							}
							else if( tli->GetLaneId(boneId) == 0 )
							{
								if( entrance.m_iLeftFilterPhase == -1 && junction->GetLightPhase() == entrance.m_iPhase )
								{
									command = (fTimeLeft<fAmberTime) ? TRAFFICLIGHT_COMMAND_AMBERLIGHT:TRAFFICLIGHT_COMMAND_GO;
								}
								else if ( entrance.m_iLeftFilterPhase != -1 && entrance.m_iLeftFilterPhase == junction->GetLightPhase() )
								{
									command = (fTimeLeft<fAmberTime) ? TRAFFICLIGHT_COMMAND_AMBERLIGHT:TRAFFICLIGHT_COMMAND_GO;
								}

							}
							else if( junction->GetLightPhase() == entrance.m_iPhase )
							{
								command = (fTimeLeft<fAmberTime) ? TRAFFICLIGHT_COMMAND_AMBERLIGHT:TRAFFICLIGHT_COMMAND_GO;
							}

							// if the light is damaged, don't render it
							u8 lightId = TrafficLightCommandToLightComponent(command);
							if( tli->IsLightDamaged((u8)boneId, lightId) )
								continue;
							
							Mat34V boneMtx;
							if(tli->AreMatricesCached())
							{
								boneMtx = tli->GetMatrixFromBoneIndex(boneId);
							}
							else
							{
								CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtx, pEntity, boneIdx);	
							}
							RenderLight(command, boneMtx, pEntity,farFade,nearFade,brightLightFade,entrance.m_bCanTurnRightOnRedLight);
							
							REPLAY_ONLY(commands[boneId] = (char)command;)
#if __BANK
							if( ms_bRenderTrafficLightsDataActual )
							{
								Color32 col = Color32(0x00,0x7f,0x00,0x7f);
								switch(command)
								{
									case TRAFFICLIGHT_COMMAND_STOP:
										col = Color32(0x7f,0x00,0x00,0x7f);
										break;
									case TRAFFICLIGHT_COMMAND_AMBERLIGHT:
										col = Color32(0x7f,0x40,0x00,0x7f);
										break;
									default:
										break;
								}
								
								const CPathNode * pNode = ThePaths.FindNodePointerSafe(entrance.m_Node);
								if( pNode )
									grcDebugDraw::Line(boneMtx.d(),VECTOR3_TO_VEC3V(pNode->GetPos()),col,Color_gold);
							}
#endif // __BANK						
							
						}
					}

					REPLAY_ONLY(ETrafficLightCommand trafficLightCommand =) RenderPedLights(pEntity,pExtension,tli,junction);
#if GTA_REPLAY	

					if (trafficLightCommand != TRAFFICLIGHT_COMMAND_INVALID)
					{
						commands[PED_WALK_BOX] = (char)trafficLightCommand;
					}

					if(CReplayMgr::ShouldRecord())
						CReplayMgr::SetTrafficLightCommands(pEntity, commands);
#endif // GTA_REPLAY
				}
			}
		}
	}
}

ETrafficLightCommand CTrafficLights::RenderPedLights(CEntity* pEntity, const BaseModelInfoBoneIndices* pExtension, TrafficLightInfos* tli, CJunction* junction)
{
	// pedestrian walk lights
	if( !tli->IsLightDamaged(PED_WALK_BOX, 0) )
	{
		int boneIdx = pExtension->GetBoneIndice(PED_WALK_BOX);
		if( boneIdx != 0xff )
		{
			Mat34V boneMtx;
			CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtx, pEntity, boneIdx);

			bool showWalk = false;
			
			if( junction->GetTemplateIndex() != -1 )
			{
				showWalk = (junction->GetLightStatusForPedCrossing(false) == LIGHT_RED);
			}
			else
			{
				// at this point we have a non-template junction:
				// Return the global default light status for the given crossing direction

				Vec3V crossingDirection = boneMtx.GetCol1();
				showWalk = (CTrafficLights::LightForPeds(junction->GetAutoJunctionCycleOffset(), junction->GetAutoJunctionCycleScale(), crossingDirection.GetXf(), crossingDirection.GetYf(), junction->GetHasPedCrossingPhase(), 0.0f) == LIGHT_RED);
			}

			ETrafficLightCommand trafficLightCommand = showWalk ? TRAFFICLIGHT_COMMAND_PED_WALK : TRAFFICLIGHT_COMMAND_PED_DONTWALK;
			RenderPedLight(trafficLightCommand, boneMtx);
			return trafficLightCommand;
		}
	}

	return TRAFFICLIGHT_COMMAND_INVALID;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SetupModelInfo
// PURPOSE :  Setup the boneindices extension on a modelinfo
/////////////////////////////////////////////////////////////////////////////////
void CTrafficLights::SetupModelInfo(CBaseModelInfo *modelInfo)
{
	const char *nodeNames[] = { "traffic_light_0",
								"traffic_light_1",
								"traffic_light_2",
								"traffic_light_3",
								"ped_walk_box",
								NULL };
								
	crSkeletonData *skeletonData = modelInfo->GetFragType()->GetCommonDrawable()->GetSkeletonData();
	Assertf(skeletonData,"Traffic light %s has no skeleton",modelInfo->GetModelName());

	BaseModelInfoBoneIndices *extension = modelInfo->GetExtension<BaseModelInfoBoneIndices>();
	if( NULL == extension )
	{
		BaseModelInfoBoneIndices *boneIds = rage_new BaseModelInfoBoneIndices(skeletonData, nodeNames);
		boneIds->CalculateBoneCount();
		modelInfo->GetExtensionList().Add(*boneIds);
	}
#if __ASSERT
	else
	{
		extension->VerifyBoneIndices(skeletonData, nodeNames);
	}
#endif // __ASSERT	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CacheBoneMatrices
// PURPOSE :  Cache the bone matrices on the TrafficLightInfos
/////////////////////////////////////////////////////////////////////////////////
void TrafficLightInfos::CacheBoneMatrices(CEntity *entity, const BaseModelInfoBoneIndices* pBoneIndices)
{
	Assert(!mtxsCached);
	const int boneCount = pBoneIndices->GetBoneCount();
	boneMtxs.Resize(boneCount);
	for(int boneId = 0;boneId<boneCount;boneId++)
	{
		int boneIdx = pBoneIndices->GetBoneIndice((eTrafficLightComponentId)boneId);
		if( boneIdx != 0xff )
		{
			CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtxs[boneId], entity, boneIdx);
		}
	}

	mtxsCached = true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRailwayCrossingLights::RenderLights
// PURPOSE :  Render railway crossing lights
/////////////////////////////////////////////////////////////////////////////////
void CRailwayCrossingLights::RenderLights(CEntity *pEntity)
{
	if (pEntity->m_nFlags.bRenderDamaged) return;			// Damaged. Lights no working

	CBaseModelInfo *pBaseModel = pEntity->GetBaseModelInfo();

	// bail out if the drawable isn't loaded in yet
	if (pBaseModel->GetDrawable() == NULL)
		return;

	const BaseModelInfoBoneIndices* pExtension = CRailwayCrossingLights::GetExtension(pBaseModel);
	if (pExtension == NULL)
		return;

	const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

	const s32 iJunctionIndex = CJunctions::GetJunctionAtPositionForRailwayCrossing(vEntityPosition);
	if (iJunctionIndex == -1)
	{
		return;
	}

	CJunction* pJunction = CJunctions::GetJunctionByIndex(iJunctionIndex);
	if (pJunction == NULL)
	{
		return;
	}

	ERailwayCrossingLightState railwayCrossingLightState = pJunction->GetRailwayCrossingLightState();

	const float entityAlpha = ((float)pEntity->GetAlpha())/255.0f;
	//const float camDist = (VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - camInterface::GetPos()).Mag();
	//const float farFade = rage::Clamp( (g_farFadeEnd - camDist)/(g_farFadeEnd - g_farFadeStart),0.0f,entityAlpha);
	//const float nearFade = rage::Clamp( (g_nearFadeEnd - camDist)/(g_nearFadeEnd - g_nearFadeStart),0.0f,entityAlpha);
	const float brightLightFade = entityAlpha; // simply use entity alpha

	bool bShouldRenderLight = false;
	u32 boneCount = pExtension->GetBoneCount();

	for (int boneId = 0; boneId < boneCount; boneId++)
	{
		int boneIdx = pExtension->GetBoneIndice((eRailwayCrossingLightComponentId)boneId);
		if( boneIdx != 0xff )
		{

			bShouldRenderLight =	((railwayCrossingLightState == RAILWAY_CROSSING_LIGHT_PULSE_LEFT) &&
										((boneId == RAILWAY_LIGHT_LEFT_0) || (boneId == RAILWAY_LIGHT_LEFT_1) || (boneId == RAILWAY_LIGHT_LEFT_2))) ||
									((railwayCrossingLightState == RAILWAY_CROSSING_LIGHT_PULSE_RIGHT) &&
										((boneId == RAILWAY_LIGHT_RIGHT_0) || (boneId == RAILWAY_LIGHT_RIGHT_1) || (boneId == RAILWAY_LIGHT_RIGHT_2)));

			if (bShouldRenderLight)
			{
				Mat34V boneMtx;
				CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtx, pEntity, boneIdx);

				RenderLight(boneMtx, brightLightFade);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRailwayCrossingLights::IsRailwayBarrierLight
// PURPOSE :  Render a railway crossing light
/////////////////////////////////////////////////////////////////////////////////
void CRailwayCrossingLights::RenderLight(const Mat34V &matrix, float brightlightAlpha)
{
	const ScalarV forwardOffset = ScalarVFromF32(0.0f);

	Vec3V lightPos = matrix.GetCol1() * forwardOffset + matrix.GetCol3();
	Vec3V lightCol = g_RedColor;
	BrightLightType_e lightType = BRIGHTLIGHTTYPE_RAILWAY_TRAFFIC_RED;

	g_brightLights.Register(lightType, 
		lightPos, 
		matrix.GetCol2(), 
		matrix.GetCol0(), 
		matrix.GetCol1(),
		RCC_VECTOR3(lightCol),
		0.0f,
		0.0f,
		true,
		brightlightAlpha);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRailwayCrossingLights::SetupModelInfo
// PURPOSE :  Setup the bone indices extension on a modelinfo
/////////////////////////////////////////////////////////////////////////////////
void CRailwayCrossingLights::SetupModelInfo(CBaseModelInfo *modelInfo)
{
	const char *nodeNames[] = { "railway_light_left_0",
								"railway_light_right_0",
								"railway_light_left_1",
								"railway_light_right_1",
								"railway_light_left_2",
								"railway_light_right_2",
								NULL };

	crSkeletonData *skeletonData = modelInfo->GetFragType()->GetCommonDrawable()->GetSkeletonData();
	Assertf(skeletonData,"Railway crossing light %s has no skeleton",modelInfo->GetModelName());

	BaseModelInfoBoneIndices *extension = modelInfo->GetExtension<BaseModelInfoBoneIndices>();
	if( NULL == extension )
	{
		BaseModelInfoBoneIndices *boneIds = rage_new BaseModelInfoBoneIndices(skeletonData, nodeNames);
		boneIds->CalculateBoneCount();
		modelInfo->GetExtensionList().Add(*boneIds);
	}
#if __ASSERT
	else
	{
		extension->VerifyBoneIndices(skeletonData, nodeNames);
	}
#endif // __ASSERT	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRailwayBarrierLights::RenderLights
// PURPOSE :  Render railway barrier lights
/////////////////////////////////////////////////////////////////////////////////
void CRailwayBarrierLights::RenderLights(CEntity *pEntity)
{
	if (pEntity->m_nFlags.bRenderDamaged) return;			// Damaged. Lights no working

	CBaseModelInfo *pBaseModel = pEntity->GetBaseModelInfo();

	// bail out if the drawable isn't loaded in yet
	if (pBaseModel->GetDrawable() == NULL)
		return;

	const BaseModelInfoBoneIndices* pExtension = CRailwayBarrierLights::GetExtension(pBaseModel);
	if (pExtension == NULL)
		return;

	const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

	const s32 iJunctionIndex = CJunctions::GetJunctionAtPositionForRailwayCrossing(vEntityPosition);
	if (iJunctionIndex == -1)
	{
		return;
	}

	CJunction* pJunction = CJunctions::GetJunctionByIndex(iJunctionIndex);
	if (pJunction == NULL)
	{
		return;
	}

	ERailwayCrossingLightState railwayCrossingLightState = pJunction->GetRailwayCrossingLightState();

	const float entityAlpha = ((float)pEntity->GetAlpha())/255.0f;
	//const float camDist = (VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - camInterface::GetPos()).Mag();
	//const float farFade = rage::Clamp( (g_farFadeEnd - camDist)/(g_farFadeEnd - g_farFadeStart),0.0f,entityAlpha);
	//const float nearFade = rage::Clamp( (g_nearFadeEnd - camDist)/(g_nearFadeEnd - g_nearFadeStart),0.0f,entityAlpha);
	const float brightLightFade = entityAlpha; // simply use entity alpha

	bool bShouldRenderLight = false;
	u32 boneCount = pExtension->GetBoneCount();

	if (boneCount >= 2)
	{
		for (int boneId = 0; boneId < boneCount; boneId++)
		{
			int boneIdx = pExtension->GetBoneIndice((eRailwayBarrierLightComponentId)boneId);
			if( boneIdx != 0xff )
			{
				bShouldRenderLight =	((boneId == 0 && boneCount == 2) && (railwayCrossingLightState == RAILWAY_CROSSING_LIGHT_PULSE_LEFT)) ||
										((boneId == 1 && boneCount == 2) && (railwayCrossingLightState == RAILWAY_CROSSING_LIGHT_PULSE_RIGHT)) ||
										((boneId == 0 && boneCount >= 3) && (railwayCrossingLightState != RAILWAY_CROSSING_LIGHT_OFF)) ||
										((boneId == 1 && boneCount >= 3) && (railwayCrossingLightState == RAILWAY_CROSSING_LIGHT_PULSE_LEFT)) ||
										((boneId == 2 && boneCount >= 3) && (railwayCrossingLightState == RAILWAY_CROSSING_LIGHT_PULSE_RIGHT)) ||
										((boneId >= 3) && (railwayCrossingLightState != RAILWAY_CROSSING_LIGHT_OFF));

				if (bShouldRenderLight)
				{
					Mat34V boneMtx;
					CVfxHelper::GetMatrixFromBoneIndex_Lights(boneMtx, pEntity, boneIdx);

					RenderLight(boneMtx, brightLightFade);
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRailwayBarrierLights::IsRailwayBarrierLight
// PURPOSE :  Render one of the barrier lights
/////////////////////////////////////////////////////////////////////////////////
void CRailwayBarrierLights::RenderLight(const Mat34V &matrix, float brightlightAlpha)
{
	const ScalarV forwardOffset = ScalarVFromF32(-0.001f);

	Vec3V lightPos = matrix.GetCol1() * forwardOffset + matrix.GetCol3();
	Vec3V lightCol = g_RedColor;
	BrightLightType_e lightType = BRIGHTLIGHTTYPE_RAILWAY_BARRIER_RED;

	g_brightLights.Register(lightType, 
		lightPos, 
		matrix.GetCol2(), 
		matrix.GetCol0(), 
		matrix.GetCol1(),
		RCC_VECTOR3(lightCol),
		0.0f,
		0.0f,
		true,
		brightlightAlpha);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRailwayBarrierLights::IsRailwayBarrierLight
// PURPOSE :  Is this a railway barrier light?
/////////////////////////////////////////////////////////////////////////////////
bool CRailwayBarrierLights::IsRailwayBarrierLight(CEntity *pEntity)
{
	if(pEntity && pEntity->GetIsTypeObject())
	{
		CObject* pObj = static_cast<CDoor*>(pEntity);
		if (pObj->IsADoor())
		{
			CDoor* pDoor = static_cast<CDoor*>(pObj);
			if (pDoor->GetDoorType() == CDoor::DOOR_TYPE_RAIL_CROSSING_BARRIER)
			{
				return true;
			}
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CRailwayBarrierLights::SetupModelInfo
// PURPOSE :  Setup the bone indices extension on a modelinfo
/////////////////////////////////////////////////////////////////////////////////
void CRailwayBarrierLights::SetupModelInfo(CBaseModelInfo *modelInfo)
{
	const char *nodeNames[] = { "railway_light_0",
								"railway_light_1",
								"railway_light_2",
								"railway_light_3",
								NULL };

	crSkeletonData *skeletonData = modelInfo->GetFragType()->GetCommonDrawable()->GetSkeletonData();
	Assertf(skeletonData,"Railway barrier %s has no skeleton",modelInfo->GetModelName());

	BaseModelInfoBoneIndices *extension = modelInfo->GetExtension<BaseModelInfoBoneIndices>();
	if( NULL == extension )
	{
		BaseModelInfoBoneIndices *boneIds = rage_new BaseModelInfoBoneIndices(skeletonData, nodeNames);
		boneIds->CalculateBoneCount();
		modelInfo->GetExtensionList().Add(*boneIds);
	}
#if __ASSERT
	else
	{
		extension->VerifyBoneIndices(skeletonData, nodeNames);
	}
#endif // __ASSERT	
}
