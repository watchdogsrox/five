//
// Filename:	PedOverlay.h
// Description:	Handles management of ped overlay presets (e.g.: tattoos, badges, etc.)
//				for multiplayer characters
// Written by:	LuisA
//
//	2012/01/19 - LuisA - initial;
//
//

#ifndef __PED_OVERLAY_H__
#define __PED_OVERLAY_H__

// rage headers
#include "parser/macros.h"
#include "vector/Vector3.h"
#include "atl/array.h"
#include "string/string.h"
#include "paging/ref.h"
#include "atl/hashstring.h"
#include "bank/bank.h"
#include "rline/clan/rlclancommon.h"

// framework headers
#include "fwtl/LinkList.h"

// game headers
#include "peds/pedpopulation.h"
#include "peds/rendering/PedDamage.h"

#define INVALID_PED_DECORATION_IDX (255)

#define NUM_DECORATION_BITFIELDS	(56)
#define NUM_DECORATION_BITS			(NUM_DECORATION_BITFIELDS * 32)

class CPed;

namespace rage
{
	class grcTexture;
};	

//////////////////////////////////////////////////////////////////////////
// Common types
//////////////////////////////////////////////////////////////////////////
enum ePedDecorationZone
{
	ZONE_TORSO = 0,
	ZONE_HEAD,
	ZONE_LEFT_ARM,
	ZONE_RIGHT_ARM,
	ZONE_LEFT_LEG,
	ZONE_RIGHT_LEG,
	ZONE_MEDALS,
	ZONE_INVALID,
};

enum ePedDecorationType
{
	TYPE_TATTOO = 0,
	TYPE_BADGE = 1,
	TYPE_MEDAL = 2,
	TYPE_INVALID,
};

//////////////////////////////////////////////////////////////////////////
// PedDecorationPreset
//////////////////////////////////////////////////////////////////////////
class PedDecorationPreset
{

public:

	PedDecorationPreset();
	~PedDecorationPreset();

	void				SetName(atHashString name)			{ m_nameHash = name; };
	void				SetTextureName(atHashString name)	{ m_txtHash = name; };
	void				SetTXDName(atHashString name)		{ m_txdHash = name; };
	void				SetZone(ePedDecorationZone zone)	{ m_zone = zone; };
	void				SetType(ePedDecorationType type)	{ m_type = type; };

	void				SetUVPos(Vector2 uvPos)				{ m_uvPos = uvPos; };
	void				SetRotation(float rotation)			{ m_rotation = rotation; };
	void				SetScale(Vector2 scale)				{ m_scale = scale; };

	atHashString		GetName()			const			{ return m_nameHash; };
#if !__FINAL
	const char*			GetNameCStr()		const			{ return m_nameHash.TryGetCStr(); };
#endif	//	!__FINAL

	atHashString		GetAwardHash()		const			{ return m_award; };
	atHashString		GetAwardLevelHash()	const			{ return m_awardLevel; };
	atHashString		GetFactionHash()	const			{ return m_faction; };
	atHashString		GetGarmentHash()	const			{ return m_garment; };

	atHashString		GetTextureName()	const			{ return m_txtHash; };
#if !__FINAL
	const char*			GetTextureNameCStr()const			{ return m_txtHash.TryGetCStr(); };
#endif	//	!__FINAL
	atHashString		GetTXDName()		const			{ return m_txdHash; };
#if !__FINAL
	const char*			GetTXDNameCStr()	const			{ return m_txdHash.TryGetCStr(); };
#endif	//	!__FINAL
	ePedDecorationZone	GetZone()			const			{ return m_zone; };
	ePedDecorationType	GetType()			const			{ return m_type; };
	Gender				GetGender()			const			{ return m_gender; };

	Vector2				GetUVPos()			const			{ return m_uvPos; };
	Vector2				GetScale()			const			{ return m_scale; };
	float				GetRotation()		const			{ return m_rotation; };
	bool				UsesTintColor()		const			{ return m_usesTintColor; }

	void				Reset();
	bool				IsSimilar(const PedDecorationPreset & other) const;

#if __BANK
	bool				m_bIsVisible;
#endif

	PAR_SIMPLE_PARSABLE;

private:

	Vector2 			m_uvPos;
	Vector2				m_scale;
	float 				m_rotation;
	atHashString 		m_nameHash;
	atHashString 		m_txdHash;
	atHashString 		m_txtHash;
	ePedDecorationZone	m_zone;
	ePedDecorationType	m_type;

	atHashString		m_faction;	// human readable version of the faction name
	atHashString		m_garment;	// human readable version of the garment id
	Gender				m_gender;	// gender just used for UI/sorting
	atHashString		m_award;	// human readable version of the reward name
	atHashString		m_awardLevel;

	bool				m_usesTintColor;

};

//////////////////////////////////////////////////////////////////////////
// PedDecorationCollection
//////////////////////////////////////////////////////////////////////////
class PedDecorationCollection
{

public:

	PedDecorationCollection();
	~PedDecorationCollection();

	atFinalHashString			GetName()							const { return m_nameHash; };
#if !__FINAL
 	const char*					GetNameCStr()						const { return m_nameHash.TryGetCStr(); };
#endif	//	!__FINAL


#if __BANK
	void						AddWidgets(rage::bkBank& bank );
	void						AddMedalWidgets(rage::bkBank& bank );
	const PedDecorationPreset*	GetDebugPreset(bool newOnly)					{ return m_editTool.GetDebugPreset(newOnly); };
	void						AddVisibleDecorations(CPed* pPed);
#endif

	bool						Load(const char* pFilename);
	bool						Load();
#if !__FINAL
	bool						Save(const char* pFilename)			const;
	bool						Save()								const;
#endif
	void						Add(const PedDecorationPreset& preset);
	void						Remove(atHashString hashName);

	PedDecorationPreset*		Get(atHashString hashName);
	const PedDecorationPreset*	Get(atHashString hashName) const;
	int							GetIndex(atHashString hashName)		const;
	PedDecorationPreset*		GetAt(int idx)								{ return ( (idx<0 || idx>=m_presets.GetCount()) ? NULL : &m_presets[idx] ); };
	const PedDecorationPreset*	GetAt(int idx)						const	{ return ( (idx<0 || idx>=m_presets.GetCount()) ? NULL : &m_presets[idx] ); };
	
	int							GetCount()							const	{ return m_presets.GetCount(); };
	bool						GetRequiredForSync()				const	{ return m_bRequiredForSync; }

	PedDecorationPreset*		Find(atHashString hashName);
	const PedDecorationPreset*	Find(atHashString hashName)			const;

	const Vector2 &				GetMedalUV(int index)				const {return m_medalLocations[index];};
	const Vector2 &				GetMedalScale()						const {return m_medalScale;};
	PAR_SIMPLE_PARSABLE;

private:

	PedDecorationPreset*		Find(atHashString hashName, int& idx);
	const PedDecorationPreset*	Find(atHashString hashName, int& idx) const;

	atArray<PedDecorationPreset>	m_presets;
	atFinalHashString 				m_nameHash;
	atRangeArray<Vector2,16>		m_medalLocations;	
	Vector2							m_medalScale;
	bool							m_bRequiredForSync;

#if __BANK
	//////////////////////////////////////////////////////////////////////////
	// PedDecorationCollection::EditTool
	//////////////////////////////////////////////////////////////////////////
	class EditTool
	{
	public:

		EditTool() : m_pParentCollection(NULL), m_bPresetVisible(false)	{ m_presetNames.Reset(); m_presetNames.Reserve(64); };
		~EditTool() { m_presetNames.Reset(); m_pParentCollection = NULL; };

		void						Init(PedDecorationCollection* pCollection);
		void						AddWidgets(rage::bkBank& bank );
		const PedDecorationPreset*	GetDebugPreset(bool newOnly);
		void						AddVisibleDecorations(CPed* pPed);


		// widget callbacks
		void	OnPresetNameSelected();
		void	OnFilterChange();
		void	OnAwardChange();
		void	OnSaveCurrentPreset();
		void	OnDeleteCurrentPreset();
		void	OnSaveCollection();
		void	OnLoadCollection();
		void	OnPresetVisibleToggle();

	private:
		
		void	UpdatePresetNamesList(const atArray<PedDecorationPreset>& presets);

		atArray<const char*>			m_presetNames;
		atArray<const char*>			m_factionFilterNames;
		atArray<const char*>			m_awardFilterNames;
		atArray<const char*>			m_awardLevelFilterNames;
		atArray<const char*>			m_garmentFilterNames;
		
		PedDecorationPreset				m_editablePreset;
		PedDecorationCollection*		m_pParentCollection;

		bkCombo*						m_pPresetNamesCombo;
		bkCombo*						m_pFactionFilterCombo;
		bkCombo*						m_pAwardsFilterCombo;
		bkCombo*						m_pAwardLevelFilterCombo;
		bkCombo*						m_pGarmentFilterCombo;

		int								m_presetNamesComboIdx;
		int								m_factionFilterComboIdx;
		int								m_awardFilterComboIdx;
		int								m_awardLevelFilterComboIdx;
		int								m_genderFilterComboIdx;
		int								m_zoneFilterComboIdx;
		int								m_garmentFilterComboIdx;

		bool							m_bPresetVisible;
		bool							m_presetChanged;

		static atHashString				ms_newPresetName;
	};

	EditTool	m_editTool;

	friend class EditTool;
#endif // __BANK
};

//////////////////////////////////////////////////////////////////////////
// PedDecalDecorationManager
//////////////////////////////////////////////////////////////////////////
struct PedDecalDecoration
{
	EmblemDescriptor	m_emblemDesc;
	strLocalIndex		m_txdId;
	s16					m_refCount;
	u8					m_framesSinceLastUse;
	bool				m_bRequested : 1;
	bool				m_bReady : 1;
	bool				m_bFailed : 1;
	bool				m_bReleasePending : 1;
	bool				m_bRefAddedThisFrame;

	PedDecalDecoration() { Reset(); }

	void Reset();
	void IncFramesSinceLastUse() { if (m_framesSinceLastUse < 255U) m_framesSinceLastUse++; }
	void DecFramesSinceLastUse() { if (m_framesSinceLastUse > 0U) m_framesSinceLastUse--; }
	void ResetFramesSinceLastUse() { m_framesSinceLastUse = 0U; }

	// The number of frames a decal can remain unused if it's in a ready state
	static const u32	MAX_FRAMES_UNUSED; 
};

//////////////////////////////////////////////////////////////////////////
// The life of a decal:
// 
// - We request the emblem decal via the crew emblem manager just once (this already adds one ref to the txd on our behalf).
// - No duplicates are allowed (ie: the same texture is returned), unless an already existing match is not in a ready state.
// - Once the txd is loaded, we cache its slot and keep a ref count (m_refCount) for the same crew emblem.
// - Every time the decal texture is requested for rendering, one ref (only one per update tick) is automatically added.
// - Every update one ref is automatically removed.
// - If its ref count remains 0 for MAX_FRAMES_UNUSED, the decal is marked for release.
// - Only when the decal is marked for release, we remove its ref with the crew emblem manager and the decal becomes free for reuse.
// - If the decal is not in a ready state and its texture is requested, we return nothing! 
//
//////////////////////////////////////////////////////////////////////////
class PedDecalDecorationManager
{

public:

	PedDecalDecorationManager() {};

	void Init();
	void Shutdown();

	void Update();
	bool GetTexture(const CPed* pPed, grcTexture*& pTexture);

private:

	bool IsInActiveState(const PedDecalDecoration* pDecal) const { return (pDecal->m_bReleasePending == false && pDecal->m_bReady == true && pDecal->m_bFailed == false); }
	bool IsRequested(const PedDecalDecoration* pDecal)  const { return (pDecal->m_bRequested == true && pDecal->m_bReleasePending == false && pDecal->m_bReady == false && pDecal->m_bFailed == false); }
	bool CanUseDecal(const PedDecalDecoration* pDecal) const { return IsInActiveState(pDecal) && (pDecal->m_refCount > 0 || pDecal->m_framesSinceLastUse < PedDecalDecoration::MAX_FRAMES_UNUSED); }

	bool AddDecal(const EmblemDescriptor& emblemDesc, PedDecalDecoration*& pDecal);

	bool FindActiveDecal(const EmblemDescriptor& emblemDesc, PedDecalDecoration*& pDecal);
	bool RequestActiveDecalTexture(PedDecalDecoration* pDecal, grcTexture*& pTexture);

	void ReleaseAll();
	void ReleaseDecal(PedDecalDecoration* pDecal);

	void ProcessRequestPendingDecal(PedDecalDecoration* pDecal);
	void ProcessFailedDecal(PedDecalDecoration* pDecal);
	void ProcessActiveDecal(PedDecalDecoration* pDecal);
	void ProcessReleasePendingDecal(PedDecalDecoration* pDecal);

	fwLinkList<PedDecalDecoration>			m_activeList;
	typedef fwLink<PedDecalDecoration>		PedDecalDecorationNode;

	static const int	MAX_DECAL_DECORATIONS = 24;

};

//////////////////////////////////////////////////////////////////////////
// PedDecorationManager
//////////////////////////////////////////////////////////////////////////
class PedDecorationManager
{

public:
	
	PedDecorationManager();
	~PedDecorationManager();

	void	Init();
	void	Shutdown();

	//////////////////////////////////////////////////////////////////////////
	// api for network synchronisation of ped decorations

	// this function will produce a bitfield where each bit that's set represents
	// a preset index from the given collection that is currently applied on
	// the *local* ped passed in pPed
	bool			GetDecorationsBitfieldFromLocalPed	(CPed* pLocalPed,				// *local* ped to query decorations from
														 u32* pOutBitfield,				// caller-supplied memory for output bitfield
														 u32 wordCount,					// number of u32 blocks for pOutBitfield
														 int& crewEmblemVariation);		// some clan decals can have a type specified in script		

	// get the damage index and time stamp of the last decoration added
	void			GetDecorationListIndexAndTimeStamp(CPed* pLocalPed, s32 &listIndex, float &timeStamp);

	// this function expects a bitfield previously built by GetDecorationsBitfieldFromLocalPed;
	// it will create compressed decorations for the *remote* ped passed in pPed
	bool			ProcessDecorationsBitfieldForRemotePed	(CPed* pRemotePed,			// *remote* ped to apply decorations to
															const u32* pInBitfield,			// bitfield representing preset indices in use
															u32 wordCount,					// number of u32 blocks in bitfield
															int crewEmblemVariation);		// some clan decals can have a type specified in script			

	// directly call AddPedDecoration/AddCompressedPedDecoration that will emplace the given decoration
	bool			PlaceDecoration(CPed* pPed, atHashString collectionName, atHashString presetName, int crewEmblemVariation=EmblemDescriptor::INVALID_EMBLEM_ID);
	bool			CheckPlaceDecoration(CPed* pPed, atHashString collectionName, atHashString presetName);
	//////////////////////////////////////////////////////////////////////////

	ePedDecorationZone GetPedDecorationZone(atHashString collectionName, atHashString presetName) const;
	ePedDecorationZone GetPedDecorationZoneByIndex(int collectionIndex, atHashString presetName) const;

	int				GetPedDecorationIndex(atHashString collectionName, atHashString presetName) const;
	int				GetPedDecorationIndexByIndex(int collectionIndex, atHashString presetName) const;
	atHashString	GetPedDecorationHashName(atHashString collectionName, int idx) const;

    int             GetNumCollections() const { return m_collections.GetCount(); }

	bool	AddPedDecoration(CPed* pPed, atHashString collectionName, atHashString presetName, int crewEmblemVariation=EmblemDescriptor::INVALID_EMBLEM_ID, float alpha = 1.0f);
	bool	AddPedDecorationByIndex(CPed* pPed, int collectionIndex, atHashString presetName);
	bool	AddCompressedPedDecoration(CPed* pPed, atHashString collectionName, atHashString presetName, int crewEmblemVariation=EmblemDescriptor::INVALID_EMBLEM_ID);

	u32		GetPedDecorationCount(CPed* pPed) const;
	bool	GetPedDecorationInfo(CPed* pPed, int decorationIdx, atHashString& collectionName, atHashString& presetName) const;

	void	ClearPedDecorations(CPed* pPed);
	void	ClearCompressedPedDecorations(CPed* pPed);
	void	ClearClanDecorations(CPed* pPed);

	void	AddCollection(const char* pFilename, bool bUpdateSerializerList = false);
	void	RemoveCollection(const char* pFilename);
	void	Update();
#if __BANK
	void	AddWidgets(rage::bkBank& bank );
	void	AddWidgetsOnDemand();
#endif
	// public only because the edit tools need to use this.
	void	ComputePlacement(CPed* pPed, const PedDecorationCollection* pCollection, const PedDecorationPreset* pPreset, ePedDamageZones &zone, Vector2 &uvs, float &rotation, Vector2 &scale);

	bool	GetCrewEmblemDecal(const CPed* pPed, grcTexture*& pTexture);

	static void								ClassInit();
	static void								ClassShutdown();
	static PedDecorationManager&			GetInstance() { return *smp_Instance; }


private:
	PedDecorationPreset * AddPedDecorationCommon(CPed* pPed, atHashString collectionName, atHashString presetName,
												 ePedDamageZones &zone, Vector2 &uvs, float &rotation, Vector2 &scale,
												 const char* OUTPUT_ONLY(funcName));
	

	int		GetPedMedalIndex(CPed* pPed, const PedDecorationCollection * pCollection, const PedDecorationPreset* pPreset, int &existingDecorationIndex);
	void	ClearPedDecoration(CPed* pPed, int decorationIdx); 

	bool	FindCollection(const char* pFilename, PedDecorationCollection*& pCollectionOut);
	bool	FindCollection(atHashString collectionName, PedDecorationCollection*& pCollectionOut);

	bool	FindCollection(const char* pFilename, const PedDecorationCollection*& pCollectionOut) const;
	bool	FindCollection(atHashString collectionName, const PedDecorationCollection*& pCollectionOut) const;

    bool    FindCollectionIndex(atHashString collectionName, u32& outCollectionIndex) const;

	enum { kGarmentGenderBits=6, kMaxUniquePresets = NUM_DECORATION_BITS-kGarmentGenderBits, kMaxUniqueGarments = 32 /*5 bits worth */}; // for serialization 
	void	BuildSerializerLists(atFixedArray<u32,kMaxUniquePresets> &presetIndexList, atFixedArray<atHashString,kMaxUniqueGarments>	&garmentList);
	void AddToSerializerLists(int col, atFixedArray<u32,kMaxUniquePresets> &presetIndexList, atFixedArray<atHashString,kMaxUniqueGarments>	&garmentList);
	BANK_ONLY(void	VerifyForSerialization(const PedDecorationCollection* pCollection));

	u32		GetPresetIndex(u32 presetIndexListEntry) const { return presetIndexListEntry & 0xffff; }
	u32		GetCollectionIndex(u32 presetIndexListEntry) const { return (presetIndexListEntry >> 16) & 0xffff; }
	static int		SortFunction(int const*a, int const*b);
	atFixedArray<u32,kMaxUniquePresets>			  m_presetIndexList;		// list of presets that are equal, except for garment and gender
	atFixedArray<atHashString,kMaxUniqueGarments> m_garmentList;			// need a list of unique garments for serialization

#if __BANK
	void	CheckDebugPedChange();
	CPed*	GetDebugPed();
	void	PopulateCollectionNamesList();
	void	TestCompressedDecoration();
	void	UpdateDebugTool();
	bool	ValidatePreset(const PedDecorationPreset* pPreset) const;
	static void	TestSerialization();

	atArray<const char*>	m_collectionNames;
	bkButton*				m_pButtonAddWidgets;
	bkGroup*				m_pGroup;

	int						m_collectioNamesComboIdx;
	bool					m_bTestCompressedDecoration;
	bool					m_bUseDebugPed;
	bool					m_bVisualDebug;
	bool					m_bUpdateOnPresetSelect;
	CPed*					m_pDebugPed;
	u32						m_debugFindMaxCount;
#endif

	PedDecalDecorationManager			m_crewEmblemDecalMgr;
	atArray<PedDecorationCollection>	m_collections;
	static PedDecorationManager*		smp_Instance;
};

#define PEDDECORATIONMGR PedDecorationManager::GetInstance()


#endif // __PED_OVERLAY_H__
