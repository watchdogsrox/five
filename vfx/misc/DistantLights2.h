///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DistantLights2.h
//	BY	: 	Thomas Randall (Adapted from original by Mark Nicholson)
//	FOR	:	Rockstar North
//	ON	:	11 Jun 2013
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////
//
//	Deals with streetlights and headlights being rendered miles into the
//	distance. The idea is to identify roads that have these lights on them
//	during the patch generation phase. These lines then get stored with the
//	world blocks and sprites are rendered along them whilst rendering
//
///////////////////////////////////////////////////////////////////////////////

#ifndef DISTANTLIGHTS2_H
#define DISTANTLIGHTS2_H

#include "DistantLightsCommon.h"

// The node generation tool needs this object in DEV builds no matter which light set we are using!
#if USE_DISTANT_LIGHTS_2 || RSG_DEV

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// Rage headers
#include "grcore/vertexbuffer.h"
#include "grmodel/shader.h"
#include "spatialdata/transposedplaneset.h"
#include "vectormath/classes.h"

// Framework headers
#include "fwmaths/Random.h"

// Game headers
#include "VehicleAI/PathFind.h"
#include "vfx/VisualEffects.h"
#include "vfx/vfx_shared.h"
#include "DistantLightsVisibility.h"
#include "entity/archetypemanager.h"

#include "security/ragesecgameinterface.h"

// This is used to store information about a generated node that is only needed for the generation step.
struct DistantLightNodeGenerationData
{
	u32 m_LinkIndex;
	u32 m_NodeFrom;
	u32 m_NodeTo;
	u32 m_IsFlipped;
};

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CBaseModelInfo;
//  CDistantLights2 ////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
using namespace rage;

#if USE_RAGESEC
class CDistantLights2 : public rageSecGamePluginBonder
#else
class CDistantLights2 
#endif
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

public: ///////////////////////////

	void					Init								(unsigned initMode);
	void					UpdateVisualDataSettings			();
	void					Shutdown							(unsigned shutdownMode);
	void					CreateBuffers						();
	void					DestroyBuffers						();

	void 					Update								();
	void					ProcessVisibility					();
	void					WaitForProcessVisibilityDependency	();
	void 					Render								(CVisualEffects::eRenderMode mode, float intensityScale);

	int						PrepareDistantCarsRenderData		();

	void					LoadData							();

	void					SetDisableVehicleLights				(bool b) { m_disableVehicleLights = b; }

	// debug functions
#if __BANK
	void 					InitWidgets							();
	void					BuildData							(CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks);
	void					SetRenderingEnabled					(bool bEnable) { m_bDisableRendering = !bEnable; }

#endif // __BANK

#if ENABLE_DISTANT_CARS
	static void				RenderDistantCars();
	void                    SetDistantCarsEnabled(bool val) { m_disableDistantCarsByScript = !val; }

	void					EnableDistantCars(bool bEnabled) { m_disableDistantCars = !bEnabled; m_disableVehicleLights = !bEnabled; }
	bool					DistantCarsEnabled()			 { return !m_disableDistantCars; }
#endif
	
#if GTA_REPLAY	
	float					DistantCarDensityMul()			{ return m_RandomVehicleDensityMult; }
	void					SetDistantCarDensityMulForReplay(float val) { m_RandomVehicleDensityMult = val; }
#endif //GTA_REPLAY

private: //////////////////////////

	int                     PlaceLightsAlongGroup			    (ScalarV_In initialOffset, ScalarV_In spacing, ScalarV_In color, ScalarV_In offsetTime, ScalarV_In speed, Vec4V_In laneScalars, Vec3V_In perpDirection, u16 groupIndex, bool isTwoWayLane, bool isTravelingAway, const Vec3V* lineVerts, const Vec4V* lineDiffs, int numLineVerts, ScalarV_In lineLength);

	void                    AppendDistantCarRenderData          (const ScalarV& offsetTime, const ScalarV& desiredDistAlongLine, const ScalarV& speed, atFixedArray<DistantCar_t, DISTANTCARS_MAX>& distantCarRenderData, const DistantLightGroup2_t &pCurrGroup, const Vec3V& worldPos, const Vec3V& direction, const bool isTravelingAway);

	void					CalcLightColour                     (const Vec3V& lightPos, const Vector3& camPos, const float lodDistance2, const ScalarV& noAlpha, const ScalarV& fullAlpha, const ScalarV& color, Color32& lightColors);

	struct CalculateLayout_Params
	{
		ScalarV oneWayColor, otherWayColor;
		Vec2V carLight1InitialOffset;
		Vec2V carLight1QuadT;
		Vec2V carLight1ScaledSpacing;
		Vec2V carLight1ScaledSpeed;
		Vec2V blockTime;
		Vec4V permutedLaneScalarsForward;
		Vec4V permutedLaneScalarsBackward;
		Vec3V perpDirection;
		Vec3V* lineVerts;
		Vec4V* lineDiffs;
		int numLineVerts;
		ScalarV lineLength;
		BoolV carDirectionChoice;
	};

	void                    CalculateLayout                 (const DistantLightGroup2_t* pCurrGroup, mthRandom &perGroupRng, const Vec2V& randomSpeedMin, const Vec2V& randomSpeedMax, const ScalarV& carLight1Speed, const Vec2V& spacingFromAi, const ScalarV& carLight1Spacing, const ScalarV& elapsedTime, const Vec4V& laneScalars, const Vec3V& vCameraPos, const DistantLightProperties& currentGroupProperties, CalculateLayout_Params& params);

	void                    PermuteLaneScalars              (const ScalarV& carLight1QuadTForward, Vec4V& permutedLaneScalarsForward, const Vec4V &laneScalars);

	void                    SetupLightProperties            (const float coastalAlpha, DistantLightProperties &coastalLightProperties, const float inlandAlpha, DistantLightProperties &inlandLightProperties);

#if RSG_BANK
	void                    VectorMapDrawActiveGroups       (const DistantLightGroup2_t* pCurrGroup);

	void                    VectorMapDisplayActiveFadingAndHiddenCars(const Vec3V& worldPos, const Vector3& camPos, float distCarEndFadeDist, const Color32& VISIBLE_COLOR, const float distCarStartFadeDist, const Vec3V& direction, const Vec3V& lightOffsets);

	void                    DebugDrawDistantLightSpheres    (Vec3V finalPos0, Vec3V finalPos1, Vec3V finalPos2, Vec3V finalPos3);

	void                    DebugDrawDistantLightsNodes     (int numLineVerts, const Vec3V* lineVerts);
#endif

	void 					FindBlock						(float posX, float posY, s32& blockX, s32& blockY);
	bool					IsInBlock						(s32 groupIndex, s32 blockX, s32 blockY) const;
	float					GetWhiteLightStartDistance		() const;
	float					GetRedLightStartDistance		() const;
	void					LoadData_Impl					(const char* path);

	static float			GetActualCarLodDistance			();

#if ENABLE_DISTANT_CARS
	void					RenderDistantCarsInternal();
#endif

	struct DistantLightsDataDependency_Params
	{
		Vector3 camPos;
		float lodDistance2;
	};

	sysDependency						m_DistantLightsDataDependency;
	DistantLightsDataDependency_Params  m_DistantLightsDataDependencyParams;


	static bool				DistantLights2RunFromDependency (const sysDependency& dependency);

	// When doing deterministic lights we need to subtract time, however, at the beginning of the game this time will be 0.
	// This is a constant time offset (as a Vec2V) that we add so that we can subtract time and have a valid value.
	const static Vec2V			sm_InitialTimeOffset;


	s32							m_numPoints;
	DistantLightPoint_t			m_points						[DISTANTLIGHTS2_MAX_POINTS];

	s32							m_numGroups;
	DistantLightGroup2_t		m_groups						[DISTANTLIGHTS2_MAX_GROUPS] ;

	s32							m_blockFirstGroup				[DISTANTLIGHTS2_MAX_BLOCKS][DISTANTLIGHTS2_MAX_BLOCKS];
	s32							m_blockNumCoastalGroups			[DISTANTLIGHTS2_MAX_BLOCKS][DISTANTLIGHTS2_MAX_BLOCKS];
	s32							m_blockNumInlandGroups			[DISTANTLIGHTS2_MAX_BLOCKS][DISTANTLIGHTS2_MAX_BLOCKS];


	bool						m_disableVehicleLights;
	bool						m_disableStreetLights;

	static distLight	sm_carLight1;
	static distLight	sm_carLight2;
	static distLight	sm_streetLight;

#if ENABLE_DISTANT_CARS
	static grcEffectVar	sm_DistantCarGeneralParams;
	static grcEffectVar	sm_DistantCarTexture;

	enum
	{
		MAX_COLORS = 10,
	};

	static Color32 sm_CarColors[MAX_COLORS];
	static ScalarV sm_CarLength;
#if RSG_BANK
	// We only need the width for vector map randering.
	static ScalarV sm_CarWidth;

	bool m_displayVectorMapFadingCars;
	bool m_displayVectorMapHiddenCars;
#endif // RSG_BANK

	int							m_softLimitDistantCars;
	bool						m_disableDistantCars;
	bool						m_CreatedVertexBuffers;
	bool                        m_disableDistantCarsByScript;

	static sysIpcSema				sm_RenderThreadSync;
	static bool						sm_RequiresRenderThreadSync;
	static sysIpcCurrentThreadId	sm_DistantCarRenderThreadID;

	grcEffectTechnique				m_DistantCarsTechnique;

	static grcVertexDeclaration*	sm_pDistantCarsVtxDecl; 
	static DistantCarInstanceData*	sm_pCurrInstance;
	static fwModelId				sm_DistantCarModelId;
	static CBaseModelInfo			*sm_DistantCarBaseModelInfo;
	static fwModelId				sm_DistantCarNightModelId;
	static CBaseModelInfo			*sm_DistantCarBaseModelInfoNight;

	static bank_float			ms_redLightFadeDistance;
	static bank_float			ms_whiteLightFadeDistance;
	static bank_float			ms_distantCarsFadeZone;
	static bank_bool			ms_debugDistantCars;

	atFixedArray<DistantCar_t, DISTANTCARS_MAX> m_distantCarRenderData[2];
	atFixedArray<Vec4V, DISTANTCARS_MAX>		m_distantLightsRenderData[2];
	int											m_distantDataRenderBuffer;

	grmShader *					m_DistantCarsShader;

	#define MaxBufferCount (RSG_PC ? (3 + 4) : 3)
	atFixedArray<grcVertexBuffer*, MaxBufferCount> m_DistantCarsInstancesVB;

#if !(__D3D11 || RSG_ORBIS)
	grcIndexBuffer*				m_DistantCarsModelIB;
	grcVertexBuffer*			m_DistantCarsModelVB;
#endif
	u32							m_DistantCarsCurVertexBuffer;
	u32							m_DistantCarsMaxVertexBuffers;

#endif // ENABLE_DISTANT_CARS
	bool						m_RenderingDistantLights;

	float						m_RandomVehicleDensityMult;

#if __BANK	// build data helper functions
	enum BuildStatus
	{
		STATUS_FINISHED,
		STATUS_INIT,
		STATUS_CULLING,
		STATUS_BUILDING,
		STATUS_SPLITTING,
		STATUS_SORTING,
		STATUS_SAVING,

		// Status Stopped is only used for debugging and will not do anything. Setting the state to stopped will pause the process.
		STATUS_STOPPED,
	};


	typedef atFixedArray<OutgoingLink, 32> DistantLightTempLinks;
	typedef atUserArray<bool, 0, u32> DistantLightAddedLinks;

	void					RenderDebugData					();
	void					UpdateNodeGeneration			();
	void					GenerateData					();
	void					LoadTempCacheData				();
	void					LoadTempData					();
	void 					SaveData						();
	void					BuildDataStep					(CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks);
	void					BuildDataSplit					(CTempNode* pTempNodes, CTempLink* pTempLinks);
	bool					SplitGroup						(CTempNode* pTempNodes, CTempLink* pTempLinks, DistantLightGroup2_t* currentGroup, s16 newLength);
	void					UpdateGroupDirection			(DistantLightGroup2_t* group);
	void					BuildDataSort					();
	void					BuildDataBuildNetworkStep		(CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks );
	void					BuildDebugDataBuildNetworkStep	();
	void					UpdateGroupPositionAndSize		( DistantLightGroup2_t* group );
	void					BuildDataCull					(CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks);
	static bool				IsValidLink						( const CTempLink& link, const CTempNode& node1, const CTempNode& node2 );
	void					BuildDataInit					();
	s32						FindNextNode					(CTempLink* pTempLinks, s32 searchStart, s32 node, s32 numTempLinks);
	void					ClearBuildData					();
	void					ChangeState						(BuildStatus status);
	void					FindAttachedLinks				(DistantLightTempLinks& links, DistantLightAddedLinks& addedLinks, s32 linkIdx, s32 nodeIdx, CTempLink* pTempLinks, s32 numTempLinks);
	OutgoingLink			FindNextCompatibleLink			(s32 linkIdx, s32 nodeIdx, CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks);

	int	m_NumRenderedWhiteLights;
	int m_NumRenderedRedLights;

	// Build debug data, to perform tasks over several frames.
	CTempNode*	m_pTempNodes;
	CTempLink*	m_pTempLinks;
	s32			m_numTempLinks;
	float		m_msToGeneratePerFrame;
	bool		m_renderTempCacheLinks;

	enum
	{
		STATUS_TEXT_MAX_LENGTH = 21
	};

	BuildStatus					m_status;

	// This is used as an iterator in loops over different frames!!!!
	s32							m_currentLinkIndex;

	char						m_statusText[STATUS_TEXT_MAX_LENGTH];

	atFixedArray<DistantLightNodeGenerationData, DISTANTLIGHTS2_MAX_POINTS> m_nodeGenerationData;
	bool						m_drawDistantLightsLocations;
	bool						m_drawDistantLightsNodes;
	bool						m_drawGroupLocations;
	bool						m_drawBlockLocations;

	bool						m_disableStreetLightsVisibility;
	bool						m_disableDepthTest;
	bool						m_bDisableRendering;
	s32							m_numBlocksRendered;
	s32							m_numBlocksWaterReflectionRendered;
	s32							m_numBlocksMirrorReflectionRendered;
	s32							m_numGroupsRendered;
	s32							m_numGroupsWaterReflectionRendered;
	s32							m_numGroupsMirrorReflectionRendered;
	s32							m_numLightsRendered;

	s32							m_maxGroupGeneration;

	// Debug road generation options.
	bool						m_generateDebugGroup;
	s32							m_debugGroupXStart;
	s32							m_debugGroupYStart;
	s32							m_debugGroupZStart;
	s32							m_debugGroupLength;
	s32							m_debugGroupLinkLength;


	bool						m_displayVectorMapAllGroups;
	bool						m_displayVectorMapActiveGroups;
	bool						m_displayVectorMapBlocks;
	bool						m_displayVectorMapActiveCars;

	bool						m_renderActiveGroups;

	float						m_overrideCoastalAlpha;
	float						m_overrideInlandAlpha;

	static float				ms_NodeGenBuildNodeRage;
	static float				ms_NodeGenMaxAngle;
	static float				ms_NodeGenMinGroupLength;

	CDblBuf<spdTransposedPlaneSet8>		m_cullPlanes;
	CDblBuf<bool>						m_interiorCullAll;

public:
	bool&						GetDisableStreetLightsVisibility() { return m_disableStreetLightsVisibility; }
#endif // __BANK

	friend class CDistantLightsVertexBuffer; // TODO -- this is just for access to m_disableDepthTest, should be cleaned up later ..
};

#endif // USE_DISTANT_LIGHTS_2 || RSG_DEV

#endif // DISTANTLIGHTS2_H