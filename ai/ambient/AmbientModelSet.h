#ifndef INC_AMBIENT_MODEL_SET_H
#define INC_AMBIENT_MODEL_SET_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "fwutil/rtti.h"
#include "parser/macros.h"
#include "ai/aichannel.h"

// Game headers
#include "modelinfo/VehicleModelInfoEnums.h"	// For eVehicleModType
#include "peds/rendering/PedVariationDS.h"		// For CPedCompRestriction

class CAmbientModelVariations;

////////////////////////////////////////////////////////////////////////////////
// CAmbientModel
////////////////////////////////////////////////////////////////////////////////

class CAmbientModel
{
public:
	// Default constructor
	CAmbientModel();

	// Copy constructor
	CAmbientModel(const CAmbientModel&);

	// Destructor
	~CAmbientModel();

	// Assignment operator
	const CAmbientModel& operator=(const CAmbientModel&);

	// Get the hash of the set
	u32 GetHash() const { return m_Name.GetHash(); }

#if !__FINAL
	// Get the name of the set
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	// Get the probability
	f32 GetProbability() const { return m_Probability; }

	// Get the variations
	const CAmbientModelVariations* GetVariations() const { return m_Variations; }

	void ReleaseVariations() { m_Variations = NULL; }

private:

	// Model name
	atHashWithStringNotFinal m_Name;

	// Pointer to optional owned object specifying which variations can be used for this model.
	const CAmbientModelVariations* m_Variations;

	// Probability
	f32 m_Probability;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientModelSetFilter
////////////////////////////////////////////////////////////////////////////////

class CAmbientModelSetFilter
{
public:
	virtual ~CAmbientModelSetFilter() {}

	virtual bool Match(u32 modelHash) const = 0;

#if __ASSERT
	virtual bool FailureShouldTriggerAsserts() const { return true; }
#endif

#if __BANK
	virtual void GetFilterDebugInfoString(char* strOut, int strMaxLen) const = 0;
#endif	// __BANK
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientModelSet
////////////////////////////////////////////////////////////////////////////////

class CAmbientModelSet
{
public:

	CAmbientModelSet() {}
	virtual ~CAmbientModelSet() {}

	// Get the hash of the set
	u32 GetHash() const { return m_Name.GetHash(); }

#if !__FINAL
	// Get the name of the set
	const char* GetName() const { return m_Name.GetCStr(); }
	const char* GetModelName(s32 iModel) const { aiAssert(iModel >= 0 && iModel < m_Models.GetCount()); return m_Models[iModel].GetName(); }
#endif // !__FINAL

	// Does the set contain the specified model?
	bool GetContainsModel(u32 uModelHash, const CAmbientModelVariations** variationsOut = NULL) const;

	// Return a random model from the set.
	u32 GetRandomModelHash(const CAmbientModelVariations** variationsOut = NULL, const CAmbientModelSetFilter* filter = NULL, const char* debugContext = NULL) const;
	u32 GetRandomModelHash(float randZeroToOne, const CAmbientModelVariations** variationsOut = NULL, const CAmbientModelSetFilter* filter = NULL, const char* debugContext = NULL) const;
	strLocalIndex GetRandomModelIndex(const CAmbientModelVariations** variationsOut = NULL, const CAmbientModelSetFilter* filter = NULL, const char* debugContext = NULL) const;
	bool HasAnyValidModel(const CAmbientModelSetFilter* filter) const;

	u32 GetNumModels() const {return m_Models.GetCount(); }

	strLocalIndex GetModelIndex( u32 iModel ) const;
	const CAmbientModelVariations* GetModelVariations(u32 indexInSet) const;

	u32	GetModelHash(s32 iModel) const { aiAssert(iModel >= 0 && iModel < m_Models.GetCount()); return m_Models[iModel].GetHash(); }
	float GetModelProbability(s32 iModel) const { aiAssert(iModel >= 0 && iModel < m_Models.GetCount()); return m_Models[iModel].GetProbability(); }

	void Clear() { m_Name.SetHash(0); m_Models.Reset(); }; 
	void ReleaseVariations() { for(int i = 0 ; i < m_Models.GetCount(); i++) { m_Models[i].ReleaseVariations(); } } 

	bool operator == (const CAmbientModelSet &r) { return (GetHash() == r.GetHash()); }

private:

	// The name of the set
	atHashWithStringNotFinal m_Name;

	// The models contained in the set
	typedef atArray<CAmbientModel> Models;
	Models m_Models;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientModelSets
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	Simple container for a number of CAmbientModelSet objects, loadable through
	the parser.
*/
class CAmbientModelSets
{
public:

	atArray<CAmbientModelSet*> m_ModelSets;

	// Mapping from model name hash to corresponding index in ambient model set array
	atBinaryMap<int, u32> m_ModelSetIndexCache;

	void Append(CAmbientModelSets &sets);
	void Remove(const CAmbientModelSets &sets);

private:

	void ParserPostLoad();

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientModelVariations
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	Base class for information about eligible variations of a specific model.
*/
class CAmbientModelVariations
{
	DECLARE_RTTI_BASE_CLASS(CAmbientModelVariations);
public:
	virtual ~CAmbientModelVariations() {}

	virtual CAmbientModelVariations* Clone() const = 0;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientPedModelVariations
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	Information about which component and prop variations can be used for a specific ped.
*/
class CAmbientPedModelVariations : public CAmbientModelVariations
{
	DECLARE_RTTI_DERIVED_CLASS(CAmbientPedModelVariations, CAmbientModelVariations);
public:
	virtual CAmbientModelVariations* Clone() const;

	atArray<CPedCompRestriction> m_CompRestrictions;
	atArray<CPedPropRestriction> m_PropRestrictions;
	
	// loadout
	atHashWithStringNotFinal m_LoadOut;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientVehicleModelVariations
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	Information about variations that may be used for a specific vehicle.
*/
class CAmbientVehicleModelVariations : public CAmbientModelVariations
{
	DECLARE_RTTI_DERIVED_CLASS(CAmbientVehicleModelVariations, CAmbientModelVariations);
public:
	// PURPOSE:	Number of extra parts that can be turned on or off, corresponding to
	//			VEH_EXTRA_1...VEH_EXTRA_10.
	static const int kNumExtras = 10;

	// PURPOSE:	Enum for controlling the presence of an extra part.
	enum UseExtra
	{
		Either,
		MustUse,
		CantUse
	};

	// PURPOSE:	Struct for controlling the presence of a mod.
	struct Mod
	{
		eVehicleModType	m_ModType;
		u8				m_ModIndex;

		PAR_SIMPLE_PARSABLE;
	};

	CAmbientVehicleModelVariations();
	virtual CAmbientModelVariations* Clone() const;

	// PURPOSE:	Get usage information about a specific extra part.
	// PARAMS:	index - part index from 0 to kNumExtras-1.
	UseExtra GetExtra(int index) const;

	// PURPOSE: Override of body colour 1, or -1.
	int				m_BodyColour1;

	// PURPOSE: Override of body colour 2, or -1.
	int				m_BodyColour2;

	// PURPOSE: Override of body colour 3, or -1.
	int				m_BodyColour3;

	// PURPOSE: Override of body colour 4, or -1.
	int				m_BodyColour4;

	// PURPOSE: Override of body colour 5, or -1.
	int				m_BodyColour5;

	// PURPOSE: Override of body colour 6, or -1.
	int				m_BodyColour6;

	// PURPOSE: Override of window tint, or -1.
	int				m_WindowTint;

	// PURPOSE: Specification of a whole colour combination to use, or -1.
	// NOTES:	Applied before m_BodyColour#.
	int				m_ColourCombination;

	// PURPOSE:	Livery to use, or -1. This controls certain textures, colours, etc, for
	//			vehicles with different major variations.
	int				m_Livery;

	// PURPOSE:	Livery to use, or -1. This controls certain textures, colours, etc, for
	//			vehicles with different major variations.
	int				m_Livery2;

	// PURPOSE:	Mod kit to use, or -1.
	int				m_ModKit;

	// PURPOSE:	Array of mods to apply.
	atArray<Mod>	m_Mods;

	// PURPOSE:	Information about the presence of extra parts.
	UseExtra		m_Extra1;
	UseExtra		m_Extra2;
	UseExtra		m_Extra3;
	UseExtra		m_Extra4;
	UseExtra		m_Extra5;
	UseExtra		m_Extra6;
	UseExtra		m_Extra7;
	UseExtra		m_Extra8;
	UseExtra		m_Extra9;
	UseExtra		m_Extra10;

	PAR_PARSABLE;
};

#endif // INC_AMBIENT_MODEL_SET_H
