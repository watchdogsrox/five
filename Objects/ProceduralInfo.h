///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	ProceduralInfo.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	20 Oct 2006
//	WHAT:	Routines to handle loading in and storing of procedural data
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PROCEDURALINFO_H
#define	PROCEDURALINFO_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////	

// rage
#include "atl/bitset.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "math/float16.h"
#include "vector/color32.h"
#include "parser/macros.h"

// framework
#include "entity/archetype.h"
#include "vfx/vfxlist.h"

// game


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define MAX_PROCEDURAL_TAGS			(255)
#define MAX_PROCEDURAL_TAG_LENGTH	(64)
#define PROCEDURAL_TAG_PLANTS_BEGIN	(128)	// plants occupy tags [128, MAX_PROCEDURAL_TAGS]
#define	MAX_PROC_OBJ_INFOS			(272)
#define MAX_PLANT_INFOS				(112)


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  TYPEDEFS
///////////////////////////////////////////////////////////////////////////////

enum ProcObjDataFlags
{
	PROCOBJ_ALIGN_OBJ,
	PROCOBJ_USE_GRID,
	PROCOBJ_USE_SEED,
	PROCOBJ_IS_FLOATING,
	PROCOBJ_CAST_SHADOW,
	PROCOBJ_NETWORK_GAME,
	PROCOBJ_UNUSED_BIT_6,
	PROCOBJ_FARTRI			// runtime only flag
};

class ProcObjData
{
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CProcObjInfo
///////////////////////////////////////////////////////////////////////////////

struct CProcObjInfo
{
	friend class CProceduralInfo;
public:
	CProcObjInfo() : m_ModelIndex(fwModelId::MI_INVALID), m_MinDistance((u16)Float16::BINARY_0), m_MaxDistance((u16)Float16::BINARY_0){}

	float				GetMinDistSq() const	{const float minDist=m_MinDistance.GetFloat32_FromFloat16();	return minDist*minDist;			}
	float				GetMaxDistSq() const	{const float maxDist=m_MaxDistance.GetFloat32_FromFloat16();	return maxDist*maxDist;			}
	float				GetDensity() const		{const float spacing=m_Spacing.GetFloat32_FromFloat16();		return 1.0f / (spacing*spacing);}
	atHashWithStringNotFinal GetTag() const {return m_Tag;}

public:
	atHashWithStringNotFinal	m_Tag;
	atHashWithStringNotFinal	m_PlantTag;
	atHashWithStringNotFinal	m_ModelName;
	strLocalIndex				m_ModelIndex;

	Float16 					m_Spacing;

	// [SSE TODO] -- vectorize these
	Float16						m_MinXRotation;
	Float16						m_MaxXRotation;
	Float16						m_MinYRotation;
	Float16						m_MaxYRotation;

	Float16						m_MinZRotation;
	Float16						m_MaxZRotation;
	Float16						m_MinScale;
	Float16						m_MaxScale;

	Float16						m_MinScaleZ;
	Float16						m_MaxScaleZ;
	Float16						m_MinZOffset;
	Float16						m_MaxZOffset;

private:
	Float16						m_MinDistance;
	Float16						m_MaxDistance;

public:
	u8							m_MinTintPalette;
	u8							m_MaxTintPalette;

	atFixedBitSet8				m_Flags;

	vfxList						m_EntityList;

private:
	void						OnPostLoad();
	void						OnPreSave();

	static u32					GetModelIndex(const char*);
	static const char*			GetModelName(u32);

	PAR_SIMPLE_PARSABLE;
} ;

//  CPlantInfo  ///////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

enum ProcPlantFlags
{
	PROCPLANT_LOD0,
	PROCPLANT_LOD1,
	PROCPLANT_LOD2,
	PROCPLANT_FURGRASS,
	PROCPLANT_CAMERADONOTCULL,
	PROCPLANT_UNDERWATER,
	PROCPLANT_GROUNDSCALE1VERT,
	PROCPLANT_NOGROUNDSKEW_LOD0,
	PROCPLANT_NOGROUNDSKEW_LOD1,
	PROCPLANT_NOGROUNDSKEW_LOD2,
	PROCPLANT_NOSHADOW,
	PROCPLANT_UNUSED_BIT11,
	PROCPLANT_UNUSED_BIT12,
	PROCPLANT_UNUSED_BIT13,
	PROCPLANT_UNUSED_BIT14,
	PROCPLANT_LOD2FARFADE			// runtime flag only
};

class CPlantInfo
{
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CProceduralInfo;
	friend class CPlantMgr;
	friend class CPlantLocTri;

public:

	float GetCollisionRadiusSqr() const		{return m_CollisionRadius.GetFloat32_FromFloat16();}
	atHashWithStringNotFinal GetTag() const {return m_Tag;}

	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

private: //////////////////////////

	atHashWithStringNotFinal m_Tag;

	Color32				m_Color;				// R, G, B, A
	Color32				m_GroundColor;			// Ground color: R, G, B + encoded scale

	// [SSE TODO] -- vectorize these
	Float16				m_ScaleXY;				// scale XY
	Float16				m_ScaleZ;				// scale Z
	Float16				m_ScaleVariationXY;		// scale variation XY
	Float16				m_ScaleVariationZ;		// scale variation Z

	Float16				m_ScaleRangeXYZ;		// scale range XYZ
	Float16				m_ScaleRangeZ;			// scale range Z

	Float16				m_MicroMovementsScaleH;	// micro-movements: global horizontal scale
	Float16				m_MicroMovementsScaleV;	// micro-movements: global vertical scale
	Float16				m_MicroMovementsFreqH;	// micro-movements: global horizontal frequency
	Float16				m_MicroMovementsFreqV;	// micro-movements: global vertical frequency

	Float16				m_WindBendScale;		// wind bend scale
	Float16				m_WindBendVariation;	// wind bend variation

	Float16				m_CollisionRadius;		// collision radius sqr

	Float16				m_Density;				// num of plants per sqm
	Float16				m_DensityRange;			// density range

	u8					m_ModelId;				// model_id of the plant
	u8					m_TextureId;			// texture number to use for the model (was UV offset for PS2)
	atFixedBitSet16		m_Flags;				// ProcPlantFlags: bitmask for enabled LODs and misc flags

	u8					m_Intensity;			// Intensity
	u8					m_IntensityVar;			// Intensity Variation

private:
	void				OnPostLoad();

	PAR_SIMPLE_PARSABLE;
} ; // CPlantInfo


enum ProcTagLookupFlags
{
	PROCTAGLOOKUP_NEXTGENONLY,
	PROCTAGLOOKUP_UNUSED1,
	PROCTAGLOOKUP_UNUSED2,
	PROCTAGLOOKUP_UNUSED3,
	PROCTAGLOOKUP_UNUSED4,
	PROCTAGLOOKUP_UNUSED5,
	PROCTAGLOOKUP_UNUSED6,
	PROCTAGLOOKUP_UNUSED7
};

class ProcTagLookup  
{
public:
	s16				procObjIndex;
	s16				plantIndex;
	atFixedBitSet8	m_Flags;
	u8				m_unused8;

public:
	ProcTagLookup() :
			procObjIndex(-1),
			plantIndex(-1) { m_Flags.Reset(); }

public:
	void	PreLoad(parTreeNode* pTreeNode);
	void	PostSave(parTreeNode* pTreeNode);

#if !__FINAL
	atString	m_name;
	const char* GetName() const					{ return m_name.c_str(); };
#endif

	PAR_SIMPLE_PARSABLE;
}; 


///////////////////////////////////////////////////////////////////////////////
//  CProceduralInfo
///////////////////////////////////////////////////////////////////////////////

class CProceduralInfo
{
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CProcObjectMan;
	friend class CPlantMgr;
	friend class CPlantMgrDataEditor;
	friend class CPlantLocTri;

	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// 
		bool Init ();
		void Shutdown ();

		bool CreatesProcObjects (s32 procTagId);
		bool CreatesPlants (s32 procTagId);

		bool RejectsGrass(s32 procTagId);

		u32 GetProcObjInfoIdx (CProcObjInfo *data);
		u32 GetProcObjCount() const { return m_procObjInfos.GetCount(); }

		int	FindProcObjInfo(const char* pName);
		int	FindPlantInfo(const char* pName);
		const CProcObjInfo& GetProcObjInfo(int i) const {return m_procObjInfos[i];}
		const CPlantInfo& GetPlantInfo(int i) const {return m_plantInfos[i];}
		const ProcTagLookup& GetProcTagLookup(int i) const {return m_procTagTable[i];}

#if __BANK
		s32 GetNumProcTags () const {return m_numProcTags;}	
#endif

#if !__SPU
private: //////////////////////////
#endif

		s32 ProcessProcTag (char procTagNames[MAX_PROCEDURAL_TAGS][MAX_PROCEDURAL_TAG_LENGTH], char procPlantNames[MAX_PROCEDURAL_TAGS][MAX_PROCEDURAL_TAG_LENGTH], char* currProcTag, s32 type);
		void ConstructTagTable();

///////////////////////////////////
// VARIABLES 
///////////////////////////////////		

		float m_version;

		atArray<CProcObjInfo> m_procObjInfos; 
		atArray<CPlantInfo> m_plantInfos;
		
		s32 m_numProcTags;
		ProcTagLookup m_procTagTable [MAX_PROCEDURAL_TAGS];

		PAR_SIMPLE_PARSABLE;

} ; // CProceduralInfo

// wrapper class needed to interface with game skeleton code
class CProceduralInfoWrapper
{
public:

    static void Init(unsigned initMode);
    static void Shutdown(unsigned shutdownMode);
};

///////////////////////////////////////////////////////////////////////////////
//  INLINED
///////////////////////////////////////////////////////////////////////////////

inline bool CProceduralInfo::RejectsGrass(s32 procTagId)
{
	Assert(procTagId <= m_numProcTags);
	return m_procTagTable[procTagId].plantIndex == 0; //0 plant index is special & set to reject grass.
}

///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CProceduralInfo	g_procInfo;



#endif // PROCEDURALINFO_H


