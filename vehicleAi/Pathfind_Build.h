#ifndef __PATHFINDBUILD_H
#define __PATHFINDBUILD_H


// Game headers
#include "debug/debug.h"
#include "pathserver/navgenparam.h"
#include "fwvehicleai/pathfindtypes.h"
#include "vehicleai/pathfind.h"

// Rage headers
#include "data/bitfield.h"
#include "file/asset.h"
#include "file/stream.h"
#include "vector/Vector3.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingmodulemgr.h"
#include "system/param.h"
#include "Vfx/Misc/distantlights.h"
#include "diag/channel.h"

#define PF_TEMPNODES		100000
#define PF_TEMPLINKS		100000
#define PF_TEMPVIRTUALJUNCTIONS 2500
#define OPEN_SPACE_SPACING	(30)		// Open space points are placed this far apart

RAGE_DECLARE_CHANNEL(pathbuild)
#define PathBuildFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(pathbuild,cond,fmt,##__VA_ARGS__)

class CPathFindBuild : public CPathFind
{
public:

	CPathFindBuild();
	virtual ~CPathFindBuild();

	void ClearTempNodes();

	void AllocateMemoryForBuildingPaths();
	void FreeMemoryForBuildingPaths();

	void SavePathFindData();
	void SortPathNodes(CTempNode *TempNodeList, CTempLink *TempLinkList, s32 NumTempNodes, s32 NumTempLinks);
	void PreparePathData(void);
	void PreparePathDataForType(CTempNode *TempNodeList, CTempLink *TempLinkList, s32 NumTempNodes, s32 NumTempLinks, float MergeDist);
	bool ThisNodeWillLeadIntoADeadEnd(const CPathNode *pArgToNode, const CPathNode *pArgFromNode) const;
	void FloodFillDistanceHashes();
	void AddOpenSpaceNodes();
	void RemoveNode(int nodeIndex);
	void RemoveLink(int link);
	bool FindNextNodeAndRemoveLinkAndNode(int nodeIndex, int &nextNodeIndex);
	void FindOpenSpacePos(int nodeX, int nodeY, float &coorsX, float &coorsY);
	void IdentifySlipLaneNodes();
	void PreprocessJunctions();
	static int QuantizeTiltValue(float angle);
	static int GetNextLargerTilt(int i);
	void SnapToGround(const bool bDoSnap, const bool bDoCamber, const bool bDoJunctions);
	void CountFloodFillGroups();
	void CheckGrid(void);
	float CalcRoadDensity(float TestX, float TestY);

	void StoreVehicleNode(const char *pFileName, s32 NodeIndex, float X, float Y, float Z,
		bool bSwitchedOff, bool bLowBridge, bool bWaterNode,
		u32 SpecialFunction, s32 Speed, float Density, u32 StreetNameHash,
		bool bHighway, bool bNoGps, bool bAdditionalTunnelFlag, bool bOpenSpace, bool usesHighResCoors, 
		bool bLeftOnly, bool bNoLeftTurns, bool bNoRightTurns, bool bOffroad, bool bNoBigVehicles,
		bool bIndicateKeepLeft, bool bIndicateKeepRight, bool bIsSlipNode);

	void StoreVehicleLink(const char *pFileName, s32 Node1, s32 Node2,
		s32 Lanes1To2, s32 Lanes2To1, float Width, bool bNarrowRoad, bool bGpsCanGoBothWays, 
		bool bShortcut, bool bIgnoreForNav, bool bBlockIfNoLanes);

	bool ReadPathInfoFromIPL(const char* pFilename);

	//	bool LoadIplFiles();
	bool QualifiesAsJunction(const CPathNode* pNode);

protected:

	static void FindTiltForLink_Simple(const Vector3 & vLinkCenter, const Vector3 & vLinkPerp, float fWidth, int & iTilt);
	static void FindTiltForLink_Complex(const Vector3 & vLinkCenter, const Vector3 & vLinkPerp, float fTotalWidth, float fCentreWidth, bool bReportIfInaccurate, bool bIsIrregularLinkType, int & iTilt, int & iFalloff);
	static float CalcSumErrorSqForLine(int iNumSamples, float * fZSamples, float fWidth, float fSlope);
	static float ZForLineWithFallof(float fX, float fWidth, float fSlope, float fFalloffWidth, float fFalloffHeight);
	static float CalcSumErrorSqForLineWithFalloff(int iNumSamples, float * fZSamples, float fWidth, float fSlope, float fFalloffWidth, float fFalloffHeight);

	// Functions for finding and reporting links with high error between the real road and the virtual road
	static void CalcMaxAndAverageErrorForLineWithFalloff(int iNumSamples, float * fZSamples, float fWidth, float fSlope, float fFalloffWidth, float fFalloffHeight, float & fErrorMaxOut, float & fErrorAveOut);
	static bool IsLinkAllIrregularMaterials(int kNumSamples, phMaterialMgr::Id * pMaterialIds);
	static bool IsMaterialIrregularSurface(phMaterialMgr::Id mtlId);
	OUTPUT_ONLY(static void PrintMaterialsOnLink(int kNumSamples, phMaterialMgr::Id * pMaterialIds);)

private:

	s32		NumTempNodes;
	s32		NumTempLinks;

	CTempNode	*TempNodes;
	CTempLink	*TempLinks;

	s32		PathNodeIndex;

	DEV_ONLY(CDistantLights  m_DistantLights);
	DEV_ONLY(CDistantLights2 m_DistantLights2);
};


#endif	//__PATHFINDBUILD_H
