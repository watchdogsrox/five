
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    trafficlights.h
// PURPOSE : Everything having to do with identifying the lights and making them
//			 work.
// AUTHOR :  Obbe.
// CREATED : 04/05/00
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _TRAFFICLIGHTS_H_
#define _TRAFFICLIGHTS_H_

#include "vehicles/automobile.h"
#include "modelinfo/BaseModelInfoExtensions.h"
#include "vehicleAi\junctions.h"

namespace rage { class CPathNode; }
namespace rage { class CPathNodeLink; }

class TrafficLightInfos;

#define NUM_TRAFFIC_LIGHT_MODELS	(32)

enum eTrafficLightColour
{
	LIGHT_UNKNOWN	= -1,
	LIGHT_GREEN		= 0,	// careful if you change these of their usage in TrafficLightInfos::trafficLightsDamaged
	LIGHT_AMBER		= 1,
	LIGHT_RED		= 2,
	LIGHT_OFF		= 3		// Only for vehicles
};

// eTrafficLightType
// This is the value stored in the CPathNodeLink class in pathfindtypes.h
enum eTrafficLightType
{
	TRAFFIC_LIGHT_NONE			= 0,	// no traffic light here
	TRAFFIC_LIGHT_CYCLE1		= 1,	// lights on cycle1
	TRAFFIC_LIGHT_CYCLE2		= 2,	// lights on cycle2
	TRAFFIC_LIGHT_STOPSIGN		= 3		// cars must give way to traffic here
};

enum eTrafficLightComponentId
{
	TRAFFIC_LIGHT_0				= 0,
	TRAFFIC_LIGHT_1				= 1,
	TRAFFIC_LIGHT_2				= 2,
	TRAFFIC_LIGHT_3				= 3,
	PED_WALK_BOX				= 4,
	TRAFFIC_LIGHT_COUNT
};

enum eRailwayCrossingLightComponentId
{
	RAILWAY_LIGHT_LEFT_0		= 0,
	RAILWAY_LIGHT_RIGHT_0		= 1,
	RAILWAY_LIGHT_LEFT_1		= 2,
	RAILWAY_LIGHT_RIGHT_1		= 3,
	RAILWAY_LIGHT_LEFT_2		= 4,
	RAILWAY_LIGHT_RIGHT_2		= 5,
	RAILWAY_LIGHT_COUNT
};

enum eRailwayBarrierLightComponentId
{
	RAILWAY_BARRIER_LIGHT_0		= 0,
	RAILWAY_BARRIER_LIGHT_1		= 1,
	RAILWAY_BARRIER_LIGHT_2		= 2,
	RAILWAY_BARRIER_LIGHT_3		= 3,
	RAILWAY_BARRIER_LIGHT_COUNT
};

#define TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET 0.38f
#define TRAFFIC_LIGHT_CORONA_BULLET_DAMAGE_RADIUS (TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET + TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET*0.5f)
#define TRAFFIC_LIGHT_HALF_DAMAGE_RADIUS (TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET*0.5f)
#define TRAFFIC_LIGHT_PED_BOX_DAMAGE_RADIUS (TRAFFIC_LIGHT_HALF_DAMAGE_RADIUS*1.8f)

#define LIGHTDURATION_LONGERGREEN		(10000)
#define LIGHTDURATION_AMBER				(2500)
#define LIGHTDURATION_SHORTERGREEN		(10000)
#define LIGHTDURATION_PEDS				(15000)
#define LIGHTDURATION_CYCLETIME_PED		(LIGHTDURATION_LONGERGREEN+LIGHTDURATION_SHORTERGREEN+LIGHTDURATION_PEDS+(3*LIGHTDURATION_AMBER))
#define LIGHTDURATION_CYCLETIME			(LIGHTDURATION_LONGERGREEN+LIGHTDURATION_SHORTERGREEN+(2*LIGHTDURATION_AMBER))

/////////////////////////////////////////////////////////////////////////////////
// A class to contain the functions.
/////////////////////////////////////////////////////////////////////////////////

class CTrafficLights
{
public:

	static void Init();

	static void Update();

#if __BANK 
	static void InitWidgets();
#endif

	static bool ShouldCarStopForLightNode(const CVehicle* pCar, const CNodeAddress& NodeAddr, eTrafficLightColour * pOutLightCol=NULL, const bool bAllowGoIfPastLine=true);
	
	static ETrafficLightCommand GetTrafficLightCommand(const CPathNode * pathnode, s32 timeOffset, float timeScale, bool pedPhase);
	
	static u32 LightForCars1(s32 timeOffset, float timeScale, bool pedPhase=false);

	static u32 LightForCars2(s32 timeOffset, float timeScale, bool pedPhase=false);

	static u32 LightForPeds(s32 timeOffset, float timeScale, float dirX = 1.0f, float dirY = 0.0f, bool pedPhase = true, float safeTimeRatio = 0.0f);

	static void SetConfiguration();

	static s32 CalculateNodeLightCycle(const CPathNode * pTrafficLightNode);

	static float TrafficLightsBrightness;

#if __BANK
	static bool    ms_bRenderTrafficLightsData;
	static bool    ms_bRenderTrafficLightsDataForSelectedLightOnly;
	static bool    ms_bRenderTrafficLightsDataActual;
	static bool    ms_bAlwaysSearchForJunction;
	static bool    ms_bAlwaysSearchForEntrance;
	static Color32 ms_PedWalkColor;
	static Color32 ms_PedDontWalkColor;
#endif

	// Component lookup
	static void SetupModelInfo(CBaseModelInfo *modelInfo);

	static inline const BaseModelInfoBoneIndices* GetExtension(const CBaseModelInfo *modelInfo)
	{
		Assertf(true == modelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT), "CTrafficLights::GetExtension called on a non traffic light model");
		return modelInfo->GetExtension<BaseModelInfoBoneIndices>();
	}
	
	// Junction utils
	static s32 GetJunctionIdx(CEntity *entity, const BaseModelInfoBoneIndices* pExtension);
	static CJunction *GetJunction(s32 idx);
	static bool SetupTrafficLightInfo(CEntity *entity, TrafficLightInfos *tli, const BaseModelInfoBoneIndices* pExtension, CJunction *junction);
	static void RemoveTrafficLightInfo(CEntity *entity);
	static void TransferTrafficLightInfo(CEntity *src, CEntity *dst);
	
	// Rendering
	static void RenderLight(ETrafficLightCommand command, const Mat34V &matrix, const CEntity *trafficLight, float farFade, float nearFade, float brightlightAlpha, bool turnright );
	static void RenderPedLight(ETrafficLightCommand command, const Mat34V &matrix);
	
	static void RenderLights(CEntity *pEntity);
	static ETrafficLightCommand RenderPedLights(CEntity* pEntity, const BaseModelInfoBoneIndices* pExtension, TrafficLightInfos* tli, CJunction* junction);
#if GTA_REPLAY
	static void	RenderLightsReplay(CEntity* pEntity, const char* commands);
#endif // GTA_REPLAY

	static bool	IsMITrafficLight(CBaseModelInfo* pBMI);
};

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsMITrafficLight
// PURPOSE :  Is this a traffic light?
/////////////////////////////////////////////////////////////////////////////////

inline bool CTrafficLights::IsMITrafficLight(CBaseModelInfo* pBMI)
{
	Assert(pBMI);
	return( pBMI->TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT));
}


/////////////////////////////////////////////////////////////////////////////////
// Traffic Light Infos
/////////////////////////////////////////////////////////////////////////////////
#define USE_TLI_COORDINATECHECK 0

class TrafficLightInfos : public fwExtension
{
public:
	EXTENSIONID_DECL(TrafficLightInfos, fwExtension);
	FW_REGISTER_CLASS_POOL(TrafficLightInfos);

	static TrafficLightInfos *Get(CEntity *entity)
	{
		return entity->GetExtension<TrafficLightInfos>();
	}
	
	static TrafficLightInfos *Add(CEntity *entity)
	{
		TrafficLightInfos *tli = rage_checked_pool_new(TrafficLightInfos);

		if( tli )
		{
			entity->GetExtensionList().Add(*tli);
		}

		Assertf(tli, "out of TrafficLightInfos");
		
		return tli;
	}

	static void Remove(CEntity *entity)
	{
		TrafficLightInfos *tli = TrafficLightInfos::Get(entity);
		Assertf(tli,"Removing a tli from an entity that doesn't have one.");
		entity->GetExtensionList().Destroy(tli);
	}

	static void Link(CEntity *entity, TrafficLightInfos *tli)
	{
		entity->GetExtensionList().Add(*tli);
	}

	static void Unlink(CEntity *entity)
	{
		TrafficLightInfos *tli = TrafficLightInfos::Get(entity);
		entity->GetExtensionList().Unlink(tli);
	}
	
	inline void SetJunctionIdx(int idx)
	{
		junctionIdx = (s8)idx;
	}
	
	inline int GetJunctionIdx()
	{
		return junctionIdx;
	}

	inline void SetTrafficLightIdx(int idx)
	{
		trafficLightIdx = (s8)idx;
	}

	inline int GetTrafficLightIdx()
	{
		return trafficLightIdx;
	}

	inline void SetIsSetup(bool s)
	{
		setup = s;
	}
	
	inline bool IsSetup()
	{
		return setup;
	}

	inline void SetSetupDistance(float d)
	{
		distSetupAt = d;
	}

	inline float GetSetupDistance()
	{
		return distSetupAt;
	}

	inline bool AreMatricesCached()
	{
		return mtxsCached;
	}

#if USE_TLI_COORDINATECHECK
	inline void SetCoord(const Vector3 &coord)
	{
		junctionCenter = coord;
	}
	
	inline Vector3& GetCoord()
	{
		return junctionCenter;
	}
#endif // USE_TLI_COORDINATECHECK	

	struct lightLink
	{
		s8 entranceId;
		s8 laneId;
	};

	inline void SetLink(int linkId, int entranceId, int laneId)
	{
		Assert(linkId<8);
		Assert(entranceId > -1 && entranceId<128);
		Assert(laneId > -1 && laneId<128);

		links[linkId].entranceId = (s8)entranceId;
		links[linkId].laneId = (s8)laneId;
	}
	
	inline int GetEntranceID(int linkId)
	{
		return links[linkId].entranceId;
	}
	
	inline int GetLaneId(int linkId)
	{
		return links[linkId].laneId;
	}

	inline void DamageLight(u8 lightId, u8 lightColor)
	{
		trafficLightsDamaged[lightId] |= (1 << lightColor);
	}

	inline bool IsLightDamaged(u8 lightId, u8 lightColor)
	{
		return trafficLightsDamaged[lightId] & (1 << lightColor) ? true : false;
	}

	Mat34V_ConstRef GetMatrixFromBoneIndex(int boneIdx) const
	{
		Assert(mtxsCached);
		return boneMtxs[boneIdx];
	}

	void CacheBoneMatrices(CEntity *entity, const BaseModelInfoBoneIndices* pBoneIndices);
	
protected:
#if USE_TLI_COORDINATECHECK
	Vector3 junctionCenter;
#endif // USE_TLI_COORDINATECHECK

	atArray<Mat34V> boneMtxs;

	lightLink links[8];
	
	s8 junctionIdx;
	s8 trafficLightIdx;
	
	bool setup;
	float distSetupAt;		// how far away from the junction the player was when this light was set up
	bool mtxsCached;

	u8 trafficLightsDamaged[TRAFFIC_LIGHT_COUNT]; // signify if that particular light (bits 0-2, red, green, yellow light) is working or not due to damage
	
public:	
	// Functions
	TrafficLightInfos()
	{
		Reset();		
	}

	inline void Reset()
	{
		memset(links,0,sizeof(links));
		memset(trafficLightsDamaged,0,sizeof(trafficLightsDamaged));
		setup = false;
		mtxsCached = false;
		junctionIdx = -1;
		trafficLightIdx	= -1;
		distSetupAt = 0.0f;
	}
	
	~TrafficLightInfos()
	{
		// Do something ?

	}
};


/////////////////////////////////////////////////////////////////////////////////
// Railway Crossing Lights
/////////////////////////////////////////////////////////////////////////////////

class CRailwayCrossingLights
{
public:

	// Rendering
	static void RenderLights(CEntity *pEntity);
	static void RenderLight(const Mat34V &matrix, float brightlightAlpha);

	static inline bool IsRailwayCrossingLight(const CBaseModelInfo *pBaseModelInfo)
	{
		return pBaseModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_RAIL_CROSSING_LIGHT);
	}

	// Component lookup
	static void SetupModelInfo(CBaseModelInfo *modelInfo);

	static inline const BaseModelInfoBoneIndices* GetExtension(const CBaseModelInfo *modelInfo)
	{
		Assertf(true == modelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_RAIL_CROSSING_LIGHT), "CRailwayCrossingLights::GetExtension called on a non railway crossing light model");
		return modelInfo->GetExtension<BaseModelInfoBoneIndices>();
	}
};


/////////////////////////////////////////////////////////////////////////////////
// Railway Barrier Lights
/////////////////////////////////////////////////////////////////////////////////

class CRailwayBarrierLights
{
public:

	// Rendering
	static void RenderLights(CEntity *pEntity);
	static void RenderLight(const Mat34V &matrix, float brightlightAlpha);

	static bool	IsRailwayBarrierLight(CEntity *pEntity);

	// Component lookup
	static void SetupModelInfo(CBaseModelInfo *modelInfo);

	static inline const BaseModelInfoBoneIndices* GetExtension(const CBaseModelInfo *modelInfo)
	{
		Assertf(true == modelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_RAIL_CROSSING_DOOR), "CRailwayBarrierLights::GetExtension called on a non railway barrier model");
		return modelInfo->GetExtension<BaseModelInfoBoneIndices>();
	}
};
#endif
