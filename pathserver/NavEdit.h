#ifndef NAVEDIT_H
#define NAVEDIT_H

#include "fwdebug/debugdraw.h"
#include "task/System/TaskHelpers.h"

#define __NAVEDITORS	(__BANK && DEBUG_DRAW)

#if __NAVEDITORS

namespace rage
{
	// Forward declaration
	class bkSlider;

	// NAME : CNavResArea
	// PURPOSE : An area of increased resolution during navmesh construction
	// Increasing the navmesh resolution locally allows us to deal with awkward geometry
	// without having to globally
	class CNavResArea
	{
	public:
		Vector3 m_vMin;
		Vector3 m_vMax;
		int m_iResMult;
		int m_iFlags;

		CNavResArea() { Clear(); }

		void Clear()
		{
			m_vMin = Vector3(0.0f, 0.0f, 0.0f);
			m_vMax = Vector3(0.0f, 0.0f, 0.0f);
			m_iResMult = 0;
			m_iFlags = 0;
		}
		inline float GetStep() { return 1.0f / (float)( 1<<(m_iResMult+1) ); }

		enum
		{
			FLAG_DO_NOT_OPTIMISE		=	0x1,		// Do not optimise navmesh within this box
			FLAG_NO_COVER				=	0x2,		// Do not generate cover within this box
			FLAG_NO_DROPS				=	0x4,		// Do not generate dropdowns from polygons within this box
			FLAG_NO_CLIMBS				=	0x8,		// Do not generate climbs from polygons within this box
			FLAG_NO_CULL_SMALL_AREAS	=	0x10,		// Do not cull small areas of navmesh
		};
	};

	class CNavResAreaEditor
	{
	public:
		CNavResAreaEditor();
		~CNavResAreaEditor();

		static void Init();
		static void Update();
		static void Render();

		enum EditorState
		{
			STATE_NONE,
			STATE_PLACING_NEW_AREA,
			STATE_REPOSITIONING_AREA
		};

		static const Vector3 ms_vDefaultAreaSize;
		static const float ms_fDefaultResolution;
		static const int ms_iXmlVersion;

	protected:

		static void InitWidgets();
		static void LoadMetadata();

		static void ProcessInput();
		static int FindResAreaClosestToPos(const Vector3 & vPos, const float fMaxDist=FLT_MAX);

		static void OnInitEditor();
		static void OnSelectArea();
		static void OnNewArea();
		static void OnNewAreaDoorway();
		static void OnDeleteArea();
		static void OnDuplicateArea();
		static void OnWarpToArea();
		static void OnWarpToNavmeshCoords();
		static void OnSelectResolutionMultiplier();

		static void OnToggleFlag_DoNotOptimise();
		static void OnToggleFlag_NoCover();
		static void OnToggleFlag_NoDrops();
		static void OnToggleFlag_NoClimbs();
		static void OnToggleFlag_NoCullSmallAreas();

		static void OnNudgeAreaPosX();
		static void OnNudgeAreaNegX();
		static void OnNudgeAreaPosY();
		static void OnNudgeAreaNegY();
		static void OnNudgeAreaPosZ();
		static void OnNudgeAreaNegZ();

		static void OnIncreaseWidth();
		static void OnIncreaseDepth();
		static void OnIncreaseHeight();
		static void OnDecreaseWidth();
		static void OnDecreaseDepth();
		static void OnDecreaseHeight();

		static void OnSave();
		static void OnLoad();

		//------------------------------------------------

		static void RoundToNearest(float & f);
		static void RoundToNearest(Vector3 & v);

		static void DrawSamplingGrid(CNavResArea * pArea, const Color32 iCol);

		static void AddNewArea(const Vector3 & vOriginIn, const float fWidth, const float fDepth, const float fHeight);

		static void RemoveAllAreas();

		//------------------------------------------------

		static bool m_bEditorInitialised;
		static bool m_bActive;
		static EditorState m_iState;
		static int m_iCurrentArea;
		static int m_iCurrentResMultCombo;	// current area's res mult, plus one - to be zero-based
		static bool m_bCurrentAreaFlag_DoNotOptimise;
		static bool m_bCurrentAreaFlag_NoCover;
		static bool m_bCurrentAreaFlag_NoDrops;
		static bool m_bCurrentAreaFlag_NoClimbs;
		static bool m_bCurrentAreaFlag_NoCullSmallAreas;
		static CNavResArea m_CurrentArea;
		static atArray<CNavResArea*> m_ResAreas;
		static float m_fSizeStep;
		static bool m_bSizeFromCenter;
		static int m_iWarpNavMeshX;
		static int m_iWarpNavMeshY;

		//------------------------------------------------

		static bkSlider * m_pCurrentAreaSlider;
	};

	// NAME : CNavDataCoverpoint
	// PURPOSE : A data specified coverpoint.
	// This enables manual placement of desired coverpoints without modifying the global algorithm just for special cases.
	// NOTE: this corresponds with the tools class CNavCustomCoverpoint (see NavGen.h)
	class CNavDataCoverpoint
	{
	public:

		enum eAIObstructionCheckStatus
		{
			AISTATUS_UNCHECKED,
			AISTATUS_PENDING,
			AISTATUS_VALID,
			AISTATUS_INVALID,
			AISTATUS_EXISTING, // a map coverpoint already exists at this data position
		};

		CNavDataCoverpoint() { Reset(); }
		~CNavDataCoverpoint() {}

		void Reset()
		{
			m_ID = 0;
			m_Type = 0;
			m_Direction = 0;
			m_CoordsX = m_CoordsY = m_CoordsZ = 0.0f;
			m_iObstructionCheckStatus = AISTATUS_UNCHECKED;
			m_bTooCloseToOtherDataPoint = false;
			m_bTooCloseToExistingMapCoverPoint = false;
		}

		u32 m_ID; // An identification number for editor sanity
		u8 m_Type; // See navmesh.h, example: NAVMESH_COVERPOINT_LOW_WALL
		u8 m_Direction; // 0-255 gets mapped to world direction vector
		float m_CoordsX, m_CoordsY, m_CoordsZ;

		// used to check for obstructions for AI usability
		int m_iObstructionCheckStatus;

		// used to check for min distance between coverpoints
		bool m_bTooCloseToOtherDataPoint;
		bool m_bTooCloseToExistingMapCoverPoint;
	};

	class CNavDataCoverpointEditor
	{
	public:
		
		CNavDataCoverpointEditor();
		~CNavDataCoverpointEditor();

		static void Init();
		static void Update();
		static void Render();

		static const int ms_iXmlVersion;

	private:

		// Initialization
		static void OnInitEditor();
		static void InitWidgets();

		// Update
		static void ProcessInput();
		static void ProcessAIObstructionChecks();

		// Events
		static void OnSelectCoverpoint();
		static void OnModifyDirection();
		static void OnModifyType();

		static void OnNewCoverpoint();
		static void OnDeleteCoverpoint();
		
		static void OnSelectNone();

		static void OnSave();
		static void OnLoad();

		// Helper methods
		static void Reset();
		
		static int FindDataCoverpointIndexClosestToPos(const Vector3 & vPos, const float fMaxDist=FLT_MAX);

		static void AddNewCoverpoint(const Vector3& vSpawnPosition);

		static void ProcessStatusHelperStart();
		
		static void ProcessStatusHelperFinished();

		static void CheckAllPointsForViolatingMinDist();

		static bool IsViolatingMinDistToOtherDataPoint(int queryDataCoverpointIndex);

		static bool IsViolatingMinDistToExistingCoverPoint(int queryDataCoverpointIndex);
	
	private:

		static bool ms_bEditorInitialized;
		static bool ms_bActive;
		static bool ms_bDisplayExistingMapPointProximity;

		static int ms_iSelectedDataCoverpointIndex;
		static atArray<CNavDataCoverpoint*> ms_aDataCoverpoints;

		static u8 ms_uCurrentCoverpointDirection; // 0 to 255 discretization of direction
		static u8 ms_uCurrentCoverpointType; // see navmesh.h, e.g. NAVMESH_COVERPOINT_LOW_WALL

		static bkSlider* ms_pCurrentCoverpointIndexSlider;
		static bkSlider* ms_pCurrentCoverpointDirectionSlider;

		// Coverpoint status checking for AI validity
		static int ms_iStatusCheckDataCoverpointIndex;
		static s32 ms_iTempCoverPointIndex;
		static CCoverPointStatusHelper ms_CoverPointStatusHelper;
	};
}

#endif // __NAVEDITORS

#endif // NAVEDIT_H

