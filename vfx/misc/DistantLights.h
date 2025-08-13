///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DistantLights.h
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	05 Aug 2008
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

#ifndef DISTANTLIGHTS_H
#define DISTANTLIGHTS_H

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// Rage headers
#include "grcore/vertexbuffer.h"
#include "grmodel/shader.h"
#include "spatialdata/transposedplaneset.h"
#include "vectormath/classes.h"

// Framework headers
#include "entity/archetypemanager.h"
#include "fwmaths/Random.h"

// Game headers
#include "VehicleAI/PathFind.h"
#include "vfx/VisualEffects.h"
#include "DistantLights2.h"
#include "DistantLightsVisibility.h"

// The node generation tool needs this object in DEV builds no matter which light set we are using!
#if !USE_DISTANT_LIGHTS_2 || RSG_DEV

#define DISTANTCARS_BATCH_SIZE		(PS3_SWITCH(16,DISTANTCARS_MAX))

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CDistantLights ////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CDistantLights
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void					Init								(unsigned initMode);
		void					CreateVertexBuffers					();
		void					UpdateVisualDataSettings			();
		void					Shutdown							(unsigned shutdownMode);

		void 					Update								();
		void					ProcessVisibility					();
		void					WaitForProcessVisibilityDependency	();
		void 					Render								(CVisualEffects::eRenderMode mode, float intensityScale);

		void					LoadData							();

		void					SetDisableVehicleLights				(bool b) { m_disableVehicleLights = b; }
		
		// debug functions
#if __BANK
		void 					InitWidgets							();
		void					RenderWithVisibility				(CVisualEffects::eRenderMode mode);
		void					BuildData							(CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks);
		void					SetRenderingEnabled					(bool bEnable) { m_bDisableRendering = !bEnable; }
			
		//bool					m_displayVectorMapAllGroups;
		//bool					m_displayVectorMapActiveGroups;
		//bool					m_displayVectorMapBlocks;

#endif

#if ENABLE_DISTANT_CARS
		static void				RenderDistantCars();
        void                    SetDistantCarsEnabled(bool val) { m_disableDistantCarsByScript = !val; }
#endif

	private: //////////////////////////
	
		int						PlaceLightsAlongGroup			(ScalarV_In lineLength, ScalarV_In initialOffset, ScalarV_In spacing, const Vec3V * lineVerts, int numLineVerts, const Vec4V * lineDiffs, Vec3V_In lightOffset, ScalarV_In color, Vec4V_In laneScalars, Vec3V_In perpDirection, bool lightColorChoice );

		void 					FindBlock						(float posX, float posY, s32& blockX, s32& blockY);

		// debug functions
#if __BANK
		// build data helper functions
		void 					SaveData						();
		s32						FindNextNode					(CTempLink* pTempLinks, s32 searchStart, s32 node, s32 numTempLinks);
		OutgoingLink			FindNextCompatibleLink			(s32 linkIdx, s32 nodeIdx, CTempNode* pTempNodes, CTempLink* pTempLinks, s32 numTempLinks);
		s32						RenderGroups					(s32 groupOffset, s32 numGroups, float fadeNearDist, float fadeFarDist, float alpha, float coastalAlpha , CVisualEffects::eRenderMode mode);
#endif
		
#if ENABLE_DISTANT_CARS
		void					RenderDistantCarsInternal();
#endif
	

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////
		
		s32							m_numPoints;
		DistantLightPoint_t			m_points						[DISTANTLIGHTS_MAX_POINTS];

		s32							m_numGroups;
		DistantLightGroup_t			m_groups						[DISTANTLIGHTS_MAX_GROUPS] ;

		s32							m_blockFirstGroup				[DISTANTLIGHTS_MAX_BLOCKS][DISTANTLIGHTS_MAX_BLOCKS];
		s32							m_blockNumCoastalGroups			[DISTANTLIGHTS_MAX_BLOCKS][DISTANTLIGHTS_MAX_BLOCKS];
		s32							m_blockNumInlandGroups			[DISTANTLIGHTS_MAX_BLOCKS][DISTANTLIGHTS_MAX_BLOCKS];
		
		bool						m_disableVehicleLights;
		bool						m_disableStreetLights;

#if ENABLE_DISTANT_CARS
		DistantCar_t				m_distantCarRenderData			[DISTANTCARS_MAX];
		s32							m_numDistantCars;

		grmShader *					m_DistantCarsShader;

#if __PS3
		grcVertexBuffer*			m_DistantCarsInstancesVB;
#else
		const static int vbBufferCount	= (RSG_ORBIS || __D3D11 || __XENON) ? 3 : 2;
		grcVertexBuffer*			m_DistantCarsInstancesVB[vbBufferCount];

		u32							m_DistantCarsCurVertexBuffer;
#endif

#if !(__D3D11 || RSG_ORBIS)
		grcIndexBuffer*				m_DistantCarsModelIB;
#if __XENON
		grcVertexBuffer*			m_DistantCarsModelIndexVB;
#endif
		grcVertexBuffer*			m_DistantCarsModelVB;
#endif
#endif //ENABLE_DISTANT_CARS

		static distLight sm_carLight1;
		static distLight sm_carLight2;
		static distLight sm_streetLight;

		// debug variables
#if ENABLE_DISTANT_CARS
#if !__FINAL
		int							m_softLimitDistantCars;
		bool						m_disableDistantCars;
		bool						m_disableDistantLights;
#endif
        bool                        m_disableDistantCarsByScript;
		bool						m_CreatedVertexBuffers;
		grcEffectTechnique			m_DistantCarsTechnique;

		static grcVertexDeclaration*	sm_pDistantCarsVtxDecl; 
		static DistantCarInstanceData*	sm_pCurrInstance;
		static fwModelId				sm_DistantCarModelId;
		static CBaseModelInfo*			sm_DistantCarBaseModelInfo;
#endif

#if __BANK
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

		bool						m_displayVectorMapAllGroups;
		bool						m_displayVectorMapActiveGroups;
		bool						m_displayVectorMapBlocks;

		bool						m_renderActiveGroups;

		float						m_overrideCoastalAlpha;
		float						m_overrideInlandAlpha;

		static float				ms_DisnantLightsBuildNodeRage;

		CDblBuf<spdTransposedPlaneSet8>		m_cullPlanes;
		CDblBuf<bool>						m_interiorCullAll;

public:
		bool&						GetDisableStreetLightsVisibility() { return m_disableStreetLightsVisibility; }
#endif

		friend class CDistantLightsVertexBuffer; // TODO -- this is just for access to m_disableDepthTest, should be cleaned up later ..
};

#endif // !USE_DISTANT_LIGHTS_2 || RSG_DEV

#endif // DISTANTLIGHTS_H


