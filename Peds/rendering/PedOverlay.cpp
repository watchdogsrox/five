//
// Filename:	PedOverlay.cpp
// Description:	Handles management of ped overlay presets (e.g.: tattoos, badges, etc.)
//				for multiplayer characters
// Written by:	LuisA
//
//	2012/01/19 - LuisA - initial;
//
//

#include "PedOverlay.h"

// rage headers
#include "atl/atfixedstring.h"
#include "atl/bitset.h"

// framework headers
#include "fwscene/stores/txdstore.h"

// game headers
#include "PedOverlay_parser.h"
#include "peds/rendering/PedVariationDebug.h"
#include "scene/DataFileMgr.h"
#include "scene/world/gameWorld.h"
#include "PedDamage.h"
#include "peds/ped.h"
#include "network/live/NetworkClan.h"
#include "network/live/LiveManager.h"
#include "network/crews/NetworkCrewEmblemMgr.h"
#include "control/replay/ReplayExtensions.h"

#define PED_OVERLAY_NEW_PRESET_STR	"[New Preset]"

//OPTIMISATIONS_OFF()

#if __BANK
static bool gUpdateCompressedDecoration = false;
static bool gDebugSerialization = false;
#endif

// we need the full assert info even on 360 beta builds here, otherwise we cannot find the offending data
#if __ASSERT
	#define PedAssertf(x,fmt,...)      (void)(Likely(x) || ::rage::diagAssertHelper(__FILE__,__LINE__,"%s: " fmt,#x,##__VA_ARGS__) || (__debugbreak(),0))
	#define PedVerifyf(x,fmt,...)      (Likely(x) || ((::rage::diagAssertHelper(__FILE__,__LINE__,"%s: " fmt,#x,##__VA_ARGS__) || (__debugbreak(),0)),false))
#else
	#define PedAssertf(x,fmt,...)
	#define PedVerifyf(x,fmt,...) (x)
#endif


////////////////////////////////////////////////////////////////////////////////
// Data file loader interface
////////////////////////////////////////////////////////////////////////////////

class CPedDecorationsDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		PedDecorationManager::GetInstance().AddCollection(file.m_filename, true);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & file ) 
	{
		PedDecorationManager::GetInstance().RemoveCollection(file.m_filename);
	}

} g_PedOverlayDataFileMounter;

//////////////////////////////////////////////////////////////////////////
// PedDecorationPreset
//////////////////////////////////////////////////////////////////////////
PedDecorationPreset::PedDecorationPreset() :
m_nameHash(0u),
m_txdHash(0u),
m_txtHash(0u),
m_zone(ZONE_INVALID),
m_type(TYPE_INVALID),
m_rotation(0.0f),
m_scale(1.0f, 1.0f),
m_uvPos(0.0f,0.0f),
m_faction(0u),
m_gender(GENDER_DONTCARE),
m_award(0u),
m_awardLevel(0u),
m_usesTintColor(false)
{
	m_garment.SetFromString("All");
	BANK_ONLY(m_bIsVisible = false;)
}

PedDecorationPreset::~PedDecorationPreset()
{
	Reset();
}


// is the preset the same except for name, gender and garment?
bool PedDecorationPreset::IsSimilar(const PedDecorationPreset & other) const
{
	return	m_txdHash == other.m_txdHash && 
			m_txtHash == other.m_txtHash && 
			m_zone == other.m_zone && 
			m_type == other.m_type &&
			m_faction == other.m_faction && 
			m_award == other.m_award && 
			m_awardLevel == other.m_awardLevel;
}

void PedDecorationPreset::Reset()
{
	m_nameHash.Clear();
	m_txdHash.Clear();
	m_txtHash.Clear();
	m_zone = ZONE_INVALID;
	m_type = TYPE_INVALID;
	m_rotation = 0.0f;
	m_faction.Clear();
	m_garment.SetFromString("All");
	m_gender  = GENDER_DONTCARE;
	m_award.Clear();
	m_awardLevel.Clear();
	m_usesTintColor = false;
}

//////////////////////////////////////////////////////////////////////////
// PedDecorationCollection
//////////////////////////////////////////////////////////////////////////
PedDecorationCollection::PedDecorationCollection()
{
	m_bRequiredForSync = false;
	m_presets.Reset();
	m_presets.Reserve(64);
	m_medalScale.Set(1.0f,1.0f);

	for (int i=0;i<m_medalLocations.GetMaxCount();i++)
		m_medalLocations[i].Set(0.0f,0.0f);
}

PedDecorationCollection::~PedDecorationCollection()
{
	m_presets.Reset();
}

bool PedDecorationCollection::Load()
{
	if (m_nameHash.IsNull())
	{
		return false;
	}

	return Load(m_nameHash.TryGetCStr());
}

bool PedDecorationCollection::Load(const char* pFilename)
{
	bool bOk = false;
	bOk = PARSER.LoadObject(pFilename, "xml", *this);

	// check mesh info data was parsed successfully
	if (bOk == false)
	{
		Displayf("PedDecorationCollection::Load: Failed to load data for \"%s\"", pFilename);
		return bOk;
	}

	return bOk;
}

#if !__FINAL
bool PedDecorationCollection::Save() const
{
	if (m_nameHash.IsNull())
	{
		return false;
	}

	return Save(m_nameHash.TryGetCStr());
}

bool PedDecorationCollection::Save(const char* pFilename) const
{
	bool bOk = false;
	char fullPath[128];
	sprintf(&fullPath[0], "%s%s", "common:/data/effects/peds/", pFilename);

	bOk = PARSER.SaveObject(&fullPath[0], "xml", this);

	// check mesh info data was parsed successfully
	if (bOk == false)
	{
		Displayf("PedDecorationCollection::Save: Failed to save data for \"%s\"", pFilename);
		return bOk;
	}

	return bOk;
}
#endif //!__FINAL

void PedDecorationCollection::Add(const PedDecorationPreset& preset)
{
	m_presets.PushAndGrow(preset);
}

int PedDecorationCollection::GetIndex(atHashString hashName) const
{
	int idx;
	Find(hashName, idx);
	return idx;
}

PedDecorationPreset* PedDecorationCollection::Get(atHashString hashName)
{
	int idx = -1;
	PedDecorationPreset* pPreset = Find(hashName, idx);
	
	return pPreset;
}

const PedDecorationPreset* PedDecorationCollection::Get(atHashString hashName) const
{
	int idx = -1;
	const PedDecorationPreset* pPreset = Find(hashName, idx);

	return pPreset;
}


PedDecorationPreset* PedDecorationCollection::Find(atHashString hashName)
{
	int idx;
	return Find(hashName, idx);
}

const PedDecorationPreset* PedDecorationCollection::Find(atHashString hashName) const 
{
	int idx;
	return Find(hashName, idx);
}

PedDecorationPreset* PedDecorationCollection::Find(atHashString hashName, int& idx)
{
	idx = -1;

	for (int i = 0; i < m_presets.GetCount(); i++) 
	{
		if (m_presets[i].GetName() == hashName)
		{
			idx = i;
			return &m_presets[i];
		}
	}

	return NULL;
}

const PedDecorationPreset* PedDecorationCollection::Find(atHashString hashName, int& idx) const
{
	idx = -1;

	for (int i = 0; i < m_presets.GetCount(); i++) 
	{
		if (m_presets[i].GetName() == hashName)
		{
			idx = i;
			return &m_presets[i];
		}
	}

	return NULL;
}

void PedDecorationCollection::Remove(atHashString hashName)
{
	int idx;
	Find(hashName, idx);

	if (idx >= 0 && idx < m_presets.GetCount())
	{
		m_presets.Delete(idx);
	}
}

#if __BANK
void PedDecorationCollection::AddWidgets(rage::bkBank& bank)
{
	m_editTool.Init(this);

	bank.PushGroup(m_nameHash.TryGetCStr());
	m_editTool.AddWidgets(bank);
	bank.PopGroup();
}

void PedDecorationCollection::AddMedalWidgets(rage::bkBank& bank)
{
	bank.PushGroup("Medals grid");
	bank.AddSlider("Medals Scale",&m_medalScale,-1.0f,1.0f,0.001f);
	bank.AddSeparator();
	for (int i=0;i<16;i++)
	{
		char label[256];
		formatf(label,256,"Medal UV[%d]",i);
		bank.AddSlider(label,&m_medalLocations[i],-1.0f,1.0f,0.001f);
	}
	bank.PopGroup();
}


void PedDecorationCollection::AddVisibleDecorations(CPed* pPed)
{
	m_editTool.AddVisibleDecorations(pPed);
}
#endif //__BANK

//////////////////////////////////////////////////////////////////////////
// PedDecorationCollection::EditTool
//////////////////////////////////////////////////////////////////////////
#if __BANK

int FilterCompare( const char * const * A,  const char * const* B)
{
	return strcmp(*A , *B);
}

atHashString PedDecorationCollection::EditTool::ms_newPresetName = PED_OVERLAY_NEW_PRESET_STR;

void PedDecorationCollection::EditTool::UpdatePresetNamesList(const atArray<PedDecorationPreset>& presets)
{
	// check for new faction names (they might have added one during editing
	m_factionFilterNames.Reset();
	m_factionFilterNames.PushAndGrow("Ignore");

	for (int i = 0; i < presets.GetCount(); i++)
	{
		if (!presets[i].GetFactionHash().IsNull()) 
		{
			int j; 
			for (j = 1; j < m_factionFilterNames.GetCount(); j++)
				if (strcmp(presets[i].GetFactionHash().TryGetCStr(), m_factionFilterNames[j])==0) break;

			if (j>=m_factionFilterNames.GetCount())
				m_factionFilterNames.PushAndGrow(presets[i].GetFactionHash().TryGetCStr());
		}
	}
	m_factionFilterNames.QSort(1, -1, FilterCompare);

	if (m_factionFilterComboIdx > m_factionFilterNames.GetCount())
		m_factionFilterComboIdx = 0;

	if (m_pFactionFilterCombo != NULL)
		m_pFactionFilterCombo->UpdateCombo("Faction Filter", &m_factionFilterComboIdx, m_factionFilterNames.GetCount(), &m_factionFilterNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnFilterChange), (datBase*)this));

	// check for garment names, they also might have changed
	m_garmentFilterNames.Reset();
	m_garmentFilterNames.PushAndGrow("Ignore");

	for (int i = 0; i < presets.GetCount(); i++)
	{
		// only add awards that are in the currently filtered list.
		if ((m_genderFilterComboIdx==0  || presets[i].GetGender() == m_genderFilterComboIdx-1) &&
			(m_zoneFilterComboIdx==0    || presets[i].GetZone() == m_zoneFilterComboIdx-1) &&
			(m_factionFilterComboIdx==0 || (!presets[i].GetFactionHash().IsNull() && strcmp( presets[i].GetFactionHash().TryGetCStr(), m_factionFilterNames[m_factionFilterComboIdx])==0)) &&
			!presets[i].GetGarmentHash().IsNull())
		{	
			int j; 
			for (j = 1; j < m_garmentFilterNames.GetCount(); j++)
				if (strcmp(presets[i].GetGarmentHash().TryGetCStr(), m_garmentFilterNames[j])==0) break;

			if (j>=m_garmentFilterNames.GetCount())
				m_garmentFilterNames.PushAndGrow(presets[i].GetGarmentHash().TryGetCStr());
		}
	}
	
	if (m_pGarmentFilterCombo != NULL)
		m_pGarmentFilterCombo->UpdateCombo("Garment Filter", &m_garmentFilterComboIdx, m_garmentFilterNames.GetCount(), &m_garmentFilterNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnFilterChange), (datBase*)this));


	// check for award names, they also might have changed
	m_awardFilterNames.Reset();
	m_awardFilterNames.PushAndGrow("Ignore");

	for (int i = 0; i < presets.GetCount(); i++)
	{
		// only add awards that are in the currently filtered list.
		if ((m_genderFilterComboIdx==0  || presets[i].GetGender() == m_genderFilterComboIdx-1) &&
			(m_zoneFilterComboIdx==0    || presets[i].GetZone() == m_zoneFilterComboIdx-1) &&
			(m_factionFilterComboIdx==0 || (!presets[i].GetFactionHash().IsNull() && strcmp( presets[i].GetFactionHash().TryGetCStr(), m_factionFilterNames[m_factionFilterComboIdx])==0)) &&
			(m_garmentFilterComboIdx==0 || (!presets[i].GetGarmentHash().IsNull() && strcmp( presets[i].GetGarmentHash().TryGetCStr(), m_garmentFilterNames[m_garmentFilterComboIdx])==0)) &&
			!presets[i].GetAwardHash().IsNull())
		{	
			int j; 
			for (j = 1; j < m_awardFilterNames.GetCount(); j++)
				if (strcmp(presets[i].GetAwardHash().TryGetCStr(), m_awardFilterNames[j])==0) break;

			if (j>=m_awardFilterNames.GetCount())
				m_awardFilterNames.PushAndGrow(presets[i].GetAwardHash().TryGetCStr());
		}
	}
	
	m_awardFilterNames.QSort(1, -1, FilterCompare);

	if (m_pAwardsFilterCombo != NULL)
		m_pAwardsFilterCombo->UpdateCombo("Award Filter", &m_awardFilterComboIdx, m_awardFilterNames.GetCount(), &m_awardFilterNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnAwardChange), (datBase*)this));


	// check for award names, they also might have changed
	m_awardLevelFilterNames.Reset();
	m_awardLevelFilterNames.PushAndGrow("Ignore");

	for (int i = 0; i < presets.GetCount(); i++)
	{
		// only add awards that are in the currently filtered list.
		if ((m_genderFilterComboIdx==0  || presets[i].GetGender() == m_genderFilterComboIdx-1) &&
			(m_zoneFilterComboIdx==0    || presets[i].GetZone() == m_zoneFilterComboIdx-1) &&
			(m_factionFilterComboIdx==0 || (!presets[i].GetFactionHash().IsNull() && strcmp( presets[i].GetFactionHash().TryGetCStr(), m_factionFilterNames[m_factionFilterComboIdx])==0)) &&
			(m_garmentFilterComboIdx==0 || (!presets[i].GetGarmentHash().IsNull() && strcmp( presets[i].GetGarmentHash().TryGetCStr(), m_garmentFilterNames[m_garmentFilterComboIdx])==0)) &&
			(m_awardFilterComboIdx==0   || (!presets[i].GetAwardHash().IsNull() && strcmp( presets[i].GetAwardHash().TryGetCStr(), m_awardFilterNames[m_awardFilterComboIdx])==0)) &&
			!presets[i].GetAwardLevelHash().IsNull())
		{	
			int j; 
			for (j = 1; j < m_awardLevelFilterNames.GetCount(); j++)
				if (strcmp(presets[i].GetAwardLevelHash().TryGetCStr(), m_awardLevelFilterNames[j])==0) break;

			if (j>=m_awardLevelFilterNames.GetCount())
				m_awardLevelFilterNames.PushAndGrow(presets[i].GetAwardLevelHash().TryGetCStr());
		}
	}

	m_awardLevelFilterNames.QSort(1, -1, FilterCompare);
	
	if (m_pAwardLevelFilterCombo != NULL)
		m_pAwardLevelFilterCombo->UpdateCombo("Award Level Filter", &m_awardLevelFilterComboIdx, m_awardLevelFilterNames.GetCount(), &m_awardLevelFilterNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnAwardChange), (datBase*)this));


	// make a list of preset that match the filters settings
	m_presetNames.Reset();
	m_presetNames.PushAndGrow(ms_newPresetName.TryGetCStr());

	for (int i = 0; i < presets.GetCount(); i++)
	{
		if ((m_genderFilterComboIdx==0  || presets[i].GetGender() == m_genderFilterComboIdx-1) &&
			(m_zoneFilterComboIdx==0    || presets[i].GetZone() == m_zoneFilterComboIdx-1) &&
			(m_factionFilterComboIdx==0 || (!presets[i].GetFactionHash().IsNull() && strcmp(presets[i].GetFactionHash().TryGetCStr(), m_factionFilterNames[m_factionFilterComboIdx])==0)) &&
			(m_garmentFilterComboIdx==0 || (!presets[i].GetGarmentHash().IsNull() && strcmp( presets[i].GetGarmentHash().TryGetCStr(), m_garmentFilterNames[m_garmentFilterComboIdx])==0)) &&
			(m_awardFilterComboIdx==0   || (!presets[i].GetAwardHash().IsNull()   && strcmp(presets[i].GetAwardHash().TryGetCStr(), m_awardFilterNames[m_awardFilterComboIdx])==0)) &&
			(m_awardLevelFilterComboIdx==0  || (!presets[i].GetAwardLevelHash().IsNull()   && strcmp(presets[i].GetAwardLevelHash().TryGetCStr(), m_awardLevelFilterNames[m_awardLevelFilterComboIdx])==0)) &&
			!presets[i].GetFactionHash().IsNull())
		{
			m_presetNames.PushAndGrow(presets[i].GetNameCStr());
		}
	}
	
	m_presetNames.QSort(1, -1, FilterCompare);
	
	m_presetNamesComboIdx = (m_presetNames.GetCount()==2)?1:0; // the list has changed, the old index is probably worthless If there is only one entry, use it. 

	if (m_pPresetNamesCombo != NULL)
		m_pPresetNamesCombo->UpdateCombo("Preset Select", &m_presetNamesComboIdx, m_presetNames.GetCount(), &m_presetNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnPresetNameSelected), (datBase*)this));
}

void PedDecorationCollection::EditTool::AddWidgets(rage::bkBank& bank)
{
	Assert(m_presetNames.GetCount() > 0);
	bank.AddSeparator();

	m_pFactionFilterCombo = bank.AddCombo("Faction Filter", &m_factionFilterComboIdx, m_factionFilterNames.GetCount(), &m_factionFilterNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnFilterChange), (datBase*)this));
	m_pGarmentFilterCombo = bank.AddCombo("Garment Filter", &m_garmentFilterComboIdx, m_garmentFilterNames.GetCount(), &m_garmentFilterNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnFilterChange), (datBase*)this));

	static const char *genderFilterList[] = {"Ignore","Male", "Female","Don't Care"};
	static const char *zoneFilterList[] = {"Ignore","ZONE_TORSO","ZONE_HEAD","ZONE_LEFT_ARM","ZONE_RIGHT_ARM","ZONE_LEFT_LEG","ZONE_RIGHT_LEG","ZONE_MEDALS","ZONE_INVALID"};
	bank.AddCombo("Gender Filter", &m_genderFilterComboIdx, 4, genderFilterList, datCallback(MFA(PedDecorationCollection::EditTool::OnFilterChange), (datBase*)this));
	bank.AddCombo("Zone Filter", &m_zoneFilterComboIdx, 8, zoneFilterList, datCallback(MFA(PedDecorationCollection::EditTool::OnFilterChange), (datBase*)this));

	m_pAwardsFilterCombo = bank.AddCombo("Award Filter", &m_awardFilterComboIdx, m_awardFilterNames.GetCount(), &m_awardFilterNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnAwardChange), (datBase*)this));
	m_pAwardLevelFilterCombo = bank.AddCombo("Award Level Filter", &m_awardLevelFilterComboIdx, m_awardLevelFilterNames.GetCount(), &m_awardLevelFilterNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnAwardChange), (datBase*)this));

	bank.AddSeparator();
	// need gender and zone combos as well
	m_pPresetNamesCombo = bank.AddCombo("Preset Select", &m_presetNamesComboIdx, m_presetNames.GetCount(), &m_presetNames[0], datCallback(MFA(PedDecorationCollection::EditTool::OnPresetNameSelected), (datBase*)this));

	bank.AddSeparator();
	bank.AddTitle("Current Preset:");
	bank.AddToggle("Visible While Editing Other Presets:", &m_bPresetVisible, datCallback(MFA(PedDecorationCollection::EditTool::OnPresetVisibleToggle), (datBase*)this));
	PARSER.AddWidgets(bank, &m_editablePreset);

	bank.AddSeparator();
	m_pParentCollection->AddMedalWidgets(bank);
	bank.AddSeparator();

	bank.AddButton("Save Current Preset", datCallback(MFA(PedDecorationCollection::EditTool::OnSaveCurrentPreset), (datBase*)this));
	bank.AddButton("Delete Current Preset", datCallback(MFA(PedDecorationCollection::EditTool::OnDeleteCurrentPreset), (datBase*)this));
	bank.AddSeparator();
	bank.AddButton("SAVE COLLECTION", datCallback(MFA(PedDecorationCollection::EditTool::OnSaveCollection), (datBase*)this));
	bank.AddButton("RELOAD COLLECTION", datCallback(MFA(PedDecorationCollection::EditTool::OnLoadCollection), (datBase*)this));
	bank.AddSeparator();
}

const PedDecorationPreset*	PedDecorationCollection::EditTool::GetDebugPreset(bool newOnly)
{
	const PedDecorationPreset* preset = (m_presetChanged || !newOnly) ? &m_editablePreset : NULL;
	m_presetChanged = false;
	return preset;  
};

void PedDecorationCollection::EditTool::Init(PedDecorationCollection* pCollection)
{
	m_pParentCollection = pCollection;
	m_pPresetNamesCombo = NULL;
	m_pAwardsFilterCombo = NULL;
	m_pAwardLevelFilterCombo = NULL;
	m_pFactionFilterCombo = NULL;
	m_pGarmentFilterCombo = NULL;

	m_presetNamesComboIdx = 0;
	m_factionFilterComboIdx = 0;
	m_garmentFilterComboIdx = 0;
	m_awardFilterComboIdx = 0;
	m_awardLevelFilterComboIdx = 0;
	m_genderFilterComboIdx = 0;
	m_zoneFilterComboIdx = 0;
	m_presetChanged = true;

	UpdatePresetNamesList(m_pParentCollection->m_presets);
}

void PedDecorationCollection::EditTool::AddVisibleDecorations(CPed* pPed)
{
	if (m_pParentCollection == NULL)
	{
		return;
	}

	for (int i = 0; i < m_pParentCollection->GetCount(); i++)
	{
		const PedDecorationPreset* pPreset = m_pParentCollection->GetAt(i);

		// only add those marked as visible and do not add the currently 
		// selected preset again
		if (pPreset != NULL && pPreset->GetName() != m_editablePreset.GetName() && pPreset->m_bIsVisible)
		{
			PEDDECORATIONMGR.AddPedDecoration( pPed, atHashString(m_pParentCollection->GetName().GetHash()), pPreset->GetName());
		}
	}
}

void PedDecorationCollection::EditTool::OnAwardChange()
{	
	UpdatePresetNamesList(m_pParentCollection->m_presets);
	OnPresetNameSelected();
}


// When Any filter except award changes we reset the award filter index
void PedDecorationCollection::EditTool::OnFilterChange()
{
	m_awardFilterComboIdx=0; // reset the award filter index when the faction changes, since the lists is drastically different
	m_awardLevelFilterComboIdx=0; // reset the award filter index when the faction changes, since the lists is drastically different
	OnAwardChange();
}

void PedDecorationCollection::EditTool::OnPresetNameSelected()
{
	m_presetChanged = true;
	int idx = m_presetNamesComboIdx;
	if (idx < 0 || idx > m_presetNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected,  reset the editable preset instance
	atHashString selectionName = atHashString(m_presetNames[idx]);
	if (selectionName == ms_newPresetName)
	{
		m_editablePreset.Reset();
	}
	else
	{
		// otherwise, try finding the preset in the collection
		PedDecorationPreset* pPreset = m_pParentCollection->Get(selectionName);
		if (pPreset != NULL) 
		{
			m_bPresetVisible = pPreset->m_bIsVisible;
			m_editablePreset = *pPreset;
		}
	}
}

void PedDecorationCollection::EditTool::OnSaveCurrentPreset()
{
	int idx = m_presetNamesComboIdx;
	if (idx < 0 || idx > m_presetNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected, add the entry to the collection
	atHashString selectionName = atHashString(m_presetNames[idx]);
	if (selectionName == ms_newPresetName)
	{
		// only add to the collection is another preset with the same
		// name doesn't already exist
		if (m_pParentCollection->Find(selectionName) == NULL)
		{
			m_pParentCollection->Add(m_editablePreset);
			m_editablePreset.Reset();
		}
	}
	else
	{
		// otherwise, try finding the preset in the collection
		// and update it
		PedDecorationPreset* pPreset = m_pParentCollection->Get(selectionName);
		if (pPreset != NULL) 
		{
			*pPreset = m_editablePreset;
			m_editablePreset.Reset();
		}
	}

	UpdatePresetNamesList(m_pParentCollection->m_presets);
}

void PedDecorationCollection::EditTool::OnDeleteCurrentPreset()
{
	int idx = m_presetNamesComboIdx;
	if (idx < 0 || idx > m_presetNames.GetCount())
	{
		return;
	}

	// if the new entry option is selected,  reset the editable preset instance
	atHashString selectionName = atHashString(m_presetNames[idx]);
	if (selectionName == ms_newPresetName)
	{
		m_editablePreset.Reset();
	}
	else
	{
		// otherwise, try finding the preset in the collection
		// and remove it
		PedDecorationPreset* pPreset = m_pParentCollection->Get(selectionName);
		if (pPreset != NULL) 
		{
			m_pParentCollection->Remove(pPreset->GetName());
			m_editablePreset.Reset();
		}
	}

	UpdatePresetNamesList(m_pParentCollection->m_presets);
}

void PedDecorationCollection::EditTool::OnSaveCollection()
{
	if (m_pParentCollection == NULL)
	{
		return;
	}
#if !__FINAL
	if (m_pParentCollection->Save())
	{
		UpdatePresetNamesList(m_pParentCollection->m_presets);
		m_editablePreset.Reset();
	}
#endif

}

void PedDecorationCollection::EditTool::OnLoadCollection()
{
	if (m_pParentCollection == NULL)
	{
		return;
	}

	if (m_pParentCollection->Load())
	{
		UpdatePresetNamesList(m_pParentCollection->m_presets);
		m_editablePreset.Reset();
	}
}


void PedDecorationCollection::EditTool::OnPresetVisibleToggle()
{
	int idx = m_presetNamesComboIdx;
	if (idx < 0 || idx > m_presetNames.GetCount())
	{
		return;
	}

	atHashString selectionName = atHashString(m_presetNames[idx]);
	PedDecorationPreset* pPreset = m_pParentCollection->Get(selectionName);
	if (pPreset != NULL) 
	{
		pPreset->m_bIsVisible = m_bPresetVisible;
	}
}

#endif //__BANK

//////////////////////////////////////////////////////////////////////////
// PedDecorationManager
//////////////////////////////////////////////////////////////////////////
PedDecorationManager::PedDecorationManager()
{
	m_collections.Reset();
	m_collections.Reserve(4);

#if __BANK
	m_collectioNamesComboIdx = -1;
	m_pDebugPed = NULL;
	m_collectionNames.Reset();
	m_collectionNames.Reserve(4);
	m_bUseDebugPed = true;
	m_bVisualDebug = false;
	m_bUpdateOnPresetSelect = false;
	m_debugFindMaxCount = m_presetIndexList.GetMaxCount();
#endif
}

PedDecorationManager::~PedDecorationManager()
{
	m_collections.Reset();

#if __BANK
	m_collectioNamesComboIdx = -1;
	m_pDebugPed = NULL;
	m_collectionNames.Reset();
	m_bUseDebugPed = false;
	m_bVisualDebug = false;
	m_bUpdateOnPresetSelect = true;
#endif
}

PedDecorationManager* PedDecorationManager::smp_Instance = NULL;

void PedDecorationManager::ClassInit() 
{
	smp_Instance = rage_new PedDecorationManager();
	Assert(smp_Instance);

	if (smp_Instance)
	{
		PEDDECORATIONMGR.Init();
	}

	CDataFileMount::RegisterMountInterface(CDataFileMgr::PED_OVERLAY_FILE, &g_PedOverlayDataFileMounter);
}

void PedDecorationManager::ClassShutdown()
{
	PEDDECORATIONMGR.Shutdown();

	delete smp_Instance;
	smp_Instance = NULL;
}

void PedDecorationManager::Init()
{
    const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PED_OVERLAY_FILE);
    while (DATAFILEMGR.IsValid(pData))
    {
		if(!pData->m_disabled)
		{
			AddCollection(pData->m_filename);
		}
        pData = DATAFILEMGR.GetNextFile(pData);
    }

	// build a list of preset IDs which represent the first preset of each unique type (not counting gender and faction)
	// also make a list of all unique factions and garments
	// DLC note: as long as new PED_OVERLAY_FILEs are added after the old ones, the new preset will get higher indices
	// and should correctly allow for new items to be added
	BuildSerializerLists(m_presetIndexList, m_garmentList);

	m_crewEmblemDecalMgr.Init();
}

void PedDecorationManager::Shutdown()
{
	m_presetIndexList.Reset();
	m_garmentList.Reset();

	m_crewEmblemDecalMgr.Shutdown();
}
int PedDecorationManager::SortFunction(int const *a, int const *b)
{
	return PEDDECORATIONMGR.m_collections[*a].GetName().GetHash() - PEDDECORATIONMGR.m_collections[*b].GetName().GetHash();
}
void PedDecorationManager::BuildSerializerLists(atFixedArray<u32,kMaxUniquePresets> &presetIndexList, atFixedArray<atHashString,kMaxUniqueGarments>	&garmentList)
{
	garmentList.Reset(); 
	presetIndexList.Reset();
	Assertf(garmentList.IsEmpty() && m_presetIndexList.IsEmpty(), "PedDecorationManager::BuildSerializerLists called more than once. Tattoos network syncing will go fubar");

	// prepend the list with this, in case we don't see one
	garmentList.Append() = ATSTRINGHASH("All",0x45835226); // this one is already registered.
	//atArray<PedDecorationCollection*> sortedCollections;
	atArray<int> sortedColIndices;
	sortedColIndices.Reserve(m_collections.GetCount());
	for (s32 col = 0; col < m_collections.GetCount(); ++col)
	{
		if(m_collections[col].GetRequiredForSync())
			sortedColIndices.PushAndGrow(col);
	}
	sortedColIndices.QSort(0,sortedColIndices.GetCount(),PedDecorationManager::SortFunction);
    BANK_ONLY(int totalPresetCount = 0;)
	for (s32 idx = 0; idx < sortedColIndices.GetCount(); ++idx)
	{
        BANK_ONLY(totalPresetCount += m_collections[sortedColIndices[idx]].GetCount();)
			AddToSerializerLists(sortedColIndices[idx], presetIndexList, garmentList);
	}

#if __BANK
	if (gDebugSerialization)
	{
		Displayf("unique preset count = %d (of %d)",presetIndexList.GetCount(),totalPresetCount);
		Displayf("unique garment count = %d",garmentList.GetCount());
		
//  		Displayf("Garments:");
//  		for (int i=0;i<garmentList.GetCount();i++)
//  			Displayf("%d: %s (0x%08x)",i,garmentList[i].TryGetCStr(), garmentList[i].GetHash());	
	}
#endif

}

void PedDecorationManager::AddToSerializerLists(int col, atFixedArray<u32,kMaxUniquePresets> &presetIndexList, atFixedArray<atHashString,kMaxUniqueGarments>	&garmentList)
{
#if __BANK
	const int prevPresetCount = presetIndexList.GetCount();
	const int prevGarmentCount = garmentList.GetCount();
#endif

	int presetCount = m_collections[col].GetCount();
	for (int i=0;i<presetCount;i++)
	{
		if ( const PedDecorationPreset * pPreset =  m_collections[col].GetAt(i) )
		{
			PedAssertf(!pPreset->GetGarmentHash().IsNull() , "Preset '%s', has a in NULL Garment name",pPreset->GetName().TryGetCStr());

			// look for it in our preset list 
			int j;
			for (j=0;j<presetIndexList.GetCount();j++)
			{
				u32 colIndex = GetCollectionIndex(presetIndexList[j]);
				u32 presetIndex = GetPresetIndex(presetIndexList[j]);
				if (pPreset->IsSimilar( *m_collections[colIndex].GetAt(presetIndex))) break;
			}

			if (j==presetIndexList.GetCount()) // did not find it so add it.
			{
				if (Verifyf(!presetIndexList.IsFull(), "there are more unique badge/tattoo decorations than can be serialized (max is %d)",presetIndexList.GetMaxCount()))
				{
					u32 presetListEntry = ((col & 0xffff) << 16) | (i & 0xffff);
					presetIndexList.Append() = presetListEntry;
				}
#if __BANK
				else
				{
					m_debugFindMaxCount++;
					Warningf("PedDecorationManager::AddToSerializerLists--presentIndexList.IsFull()--there are more unique badge/tattoo decorations than can be serialized (max is %d)--max should be %d",presetIndexList.GetMaxCount(),m_debugFindMaxCount);
				}
#endif
			}

			// check in the garment list as well
			atHashString garmentHash = pPreset->GetGarmentHash().IsNull() ? garmentList[0]: pPreset->GetGarmentHash();
			for (j=0;j<garmentList.GetCount();j++)
				if (garmentHash == garmentList[j]) break;

			if (j==garmentList.GetCount()) // did not find it so add it.
			{
				if (Verifyf(!garmentList.IsFull(), "there are more unique factions than can be serialized (max is %d)",garmentList.GetMaxCount()))
					garmentList.Append() = garmentHash;
			}
		}
	}

#if __BANK
	if (gDebugSerialization)
	{
		Displayf("AddToSerializerLists: unique preset count increased by %d (of %d)",presetIndexList.GetCount() - prevPresetCount, presetCount);
		Displayf("AddToSerializerLists: unique garment count increased from %d to %d", prevGarmentCount, garmentList.GetCount());
	}
#endif

}

#if __BANK
// make sure all the presets will serialize cleanly
void PedDecorationManager::VerifyForSerialization(const PedDecorationCollection* pCollection)
{
	for (int i = 0; i < pCollection->GetCount(); i++)
	{
		// find basic preset
		const PedDecorationPreset* pBasePreset = pCollection->GetAt(i);
		if (pBasePreset != NULL)
		{
			Gender gender = pBasePreset->GetGender();
			atHashString garmentHash = pBasePreset->GetGarmentHash();
			PedAssertf(garmentHash.IsNotNull(),"Preset '%s' does not have a value garment name", pBasePreset->GetGarmentHash().TryGetCStr());
			
			const PedDecorationPreset* pFoundPreset = NULL;

			// now look for the one with a full match (including gender and clothing)
			for (int idx=0;idx<pCollection->GetCount();idx++)
			{
				const PedDecorationPreset* pPreset = pCollection->GetAt(idx);

				if (pPreset && pBasePreset->IsSimilar(*pPreset))  // find other "similar" presets
				{
					// check for full match
					if ((pPreset->GetGarmentHash()==garmentHash || pPreset->GetGarmentHash()==ATSTRINGHASH("All",0x45835226)) && 
						(pPreset->GetGender()==gender || pPreset->GetGender()==GENDER_DONTCARE))
					{
						if (Verifyf(pFoundPreset==NULL,"Cannot distinguish between two presets for serialization (%s and %s)", pFoundPreset->GetNameCStr(),pPreset->GetNameCStr()))
						{
							pFoundPreset = pPreset;	
						}
					}
				}
			}
		}
	}
}
#endif	// __BANK


Gender GetGender(const CPed * pPed)
{
	fwModelId pedModelId = pPed->GetModelId();

	if (pedModelId.IsValid())
	{
		if (const CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId))
			return (pPedModelInfo->IsMale() ? GENDER_MALE : GENDER_FEMALE);
	}
	return GENDER_DONTCARE;  // Cannot tell
}

bool PedDecorationManager::GetDecorationsBitfieldFromLocalPed(CPed* pPed, u32* pOutBitfield, u32 wordCount, int& crewEmblemVariation)
{
	// validation
	if (wordCount == 0 || pOutBitfield == NULL)
	{
		Assertf(0, "PedDecorationManager::GetDecorationsBitfieldFromLocalPed: output array is NULL");
		return false;
	}

	if (pPed == NULL)
	{	
		Assertf(0, "PedDecorationManager::GetDecorationsBitfieldFromLocalPed: ped is NULL");
		return false;
	}

	// clear bitfield
	memset(pOutBitfield, 0, sizeof(u32)*wordCount);
	u32 numBits = wordCount*32;
 	
	atUserBitSet decorationBitfield(pOutBitfield, numBits);
	
	Gender gender = GetGender(pPed);
	Assertf(gender != GENDER_DONTCARE, "Can't tell the genre of the player");

	// first bit is the gender
	decorationBitfield.Set(0, gender==GENDER_MALE);

	crewEmblemVariation = EmblemDescriptor::INVALID_EMBLEM_ID;

	// this should be the local ped only
	const CPedDamageSet* pDamageSet = PEDDAMAGEMANAGER.GetDamageSet(pPed);
	if (pDamageSet != NULL)
	{
		const atVector<CPedDamageBlitDecoration>& decorationArray = pDamageSet->GetDecorationList();
		
		if ( decorationArray.GetCount()==0)  // no decorations to worry about
			return true;
	
		// now figure out our garment type (based on the decorations we have)
		atHashString garmentHash;
		ASSERT_ONLY(const PedDecorationPreset * pPreviouslyFoundPreset=NULL;)

		for (int i = 0; i < decorationArray.GetCount(); i++)
		{
			const u32 collection = decorationArray[i].GetCollectionNameHash();

			PedDecorationCollection* pCollection = NULL;

			// it's possible and valid that no collection is found; some decorations (e.g. dirt) are not 
			// part of one
			if (FindCollection(collection, pCollection))
			{		
				const PedDecorationPreset * pPreset = pCollection->Get(decorationArray[i].GetPresetNameHash());
				if (pPreset)
				{
					if(pPreset->GetGarmentHash()!=ATSTRINGHASH("All",0x45835226))
					{
						if (garmentHash.IsNull())
						{
							garmentHash = pPreset->GetGarmentHash();
#if !__ASSERT	
							break;
#else
							pPreviouslyFoundPreset = pPreset;
#endif
						}
						else
						{
							PedAssertf(garmentHash==pPreset->GetGarmentHash(),"Multiple Garment types found in ped (%s and %s in %s and %s)", garmentHash.TryGetCStr(),pPreset->GetGarmentHash().TryGetCStr(), pPreviouslyFoundPreset?pPreviouslyFoundPreset->GetNameCStr():"NULL",pPreset->GetNameCStr());
						}
					}
			
					// should we also check that all the decorations are either gender neutral or correct gender for this ped?
				}
			}

			if (decorationArray[i].GetEmblemDescriptor().m_type == EmblemDescriptor::EMBLEM_TOURNAMENT)
			{
				crewEmblemVariation = decorationArray[i].GetEmblemDescriptor().m_id;
			}
		}

		if(garmentHash.IsNull()) 
			garmentHash=ATSTRINGHASH("All",0x45835226);

		// find the index of garment in the unique lists		
		int garmentIndex;
		for (garmentIndex=0; garmentIndex<m_garmentList.GetCount(); garmentIndex++)
			if(m_garmentList[garmentIndex]==garmentHash) break;
		
		PedAssertf(garmentIndex<m_garmentList.GetCount(),"could not find garment '%s' in the unique garment list",garmentHash.TryGetCStr());

		// now encode the garment indexes
		decorationBitfield.Set(1, (garmentIndex&0x1)!=0x0);
		decorationBitfield.Set(2, (garmentIndex&0x2)!=0x0);
		decorationBitfield.Set(3, (garmentIndex&0x4)!=0x0);
		decorationBitfield.Set(4, (garmentIndex&0x8)!=0x0);
		decorationBitfield.Set(5, (garmentIndex&0x10)!=0x0);

#if __BANK
		if (gDebugSerialization)
		{
			Displayf("serialization Gender = %s", gender==GENDER_MALE ? "GENDER_MALE":"GENDER_FEMALE");
			Displayf("serialization Garment = %s", garmentHash.TryGetCStr());
		}
#endif

		// set the rest of the bits based on the presets unique index
		for (int i = 0; i < decorationArray.GetCount(); i++)
		{
			const u32 collection = decorationArray[i].GetCollectionNameHash();

			// it's possible and valid that no collection is found; some decorations (e.g. dirt) are not 
			// part of one
			u32 collectionIndex;
			if (FindCollectionIndex(collection, collectionIndex))
			{	
				PedDecorationCollection *pCollection = &m_collections[collectionIndex];

				const PedDecorationPreset * pPreset = pCollection->Get(decorationArray[i].GetPresetNameHash());
				if (pPreset)
				{
					int idx;
					for (idx=0;idx<m_presetIndexList.GetCount();idx++)
					{
						u32 colIndex = GetCollectionIndex(m_presetIndexList[idx]);  // only check preset if it's in the same collection
						if(colIndex == collectionIndex)
						{
							u32 presetIndex = GetPresetIndex(m_presetIndexList[idx]);
							if (pPreset->IsSimilar( *m_collections[colIndex].GetAt(presetIndex))) 
								break;
						}
					}
					
					if (idx<m_presetIndexList.GetCount())
					{
#if __ASSERT || __BANK
						u32 presetIndex = GetPresetIndex(m_presetIndexList[idx]);

						if (pCollection->GetAt(presetIndex)!=pPreset) // don't assert one bit already set, if we're the same as the one that set it.
						{
							PedAssertf(decorationBitfield.IsClear(kGarmentGenderBits+idx), "more than one preset encoded to the same bit field ('%s' and '%s')",pCollection->GetAt(presetIndex)->GetName().TryGetCStr(), pPreset->GetName().TryGetCStr());
						}
#endif

#if __BANK
						if (gDebugSerialization)
							Displayf("serializing preset %s as %s", pPreset->GetNameCStr(),pCollection->GetAt(presetIndex)->GetNameCStr());
#endif
						decorationBitfield.Set(kGarmentGenderBits+idx, true);
					}
				}
			}
		}

		return true;
	}


	// no decorations on ped
	return false;
}

void PedDecorationManager::GetDecorationListIndexAndTimeStamp(CPed* pPed, s32 &listIndex, float &timeStamp)
{
	listIndex = -1;
	timeStamp = -1.0f;

	if (const CPedDamageSet* pDamageSet = PEDDAMAGEMANAGER.GetDamageSet(pPed))
	{
		listIndex = pPed->GetDamageSetID();
		timeStamp = pDamageSet->GetLastDecorationTimeStamp();
	}
}

bool PedDecorationManager::ProcessDecorationsBitfieldForRemotePed(CPed* pPed, const u32* pInBitfield, u32 wordCount, int crewEmblemVariation)
{
	// validation
	if (wordCount == 0 || pInBitfield == NULL)
	{
		Assertf(0, "PedDecorationManager::ProcessDecorationsBitfieldForRemotePed: input array is NULL");
		return false;
	}
	if (pPed == NULL)
	{	
		Assertf(0, "PedDecorationManager::ProcessDecorationsBitfieldForRemotePed: ped is NULL");
		return false;
	}

	u32 numBits = wordCount*32;
	atUserBitSet decorationBitfield(const_cast<u32 *>(pInBitfield), numBits);
	
	// now decode the gender and garment indexes into the first 6 bits
	int garmentIndex = 0;
	Gender gender = decorationBitfield.IsSet(0)?GENDER_MALE:GENDER_FEMALE;
	garmentIndex += decorationBitfield.IsSet(1)?0x1:0x0;
	garmentIndex += decorationBitfield.IsSet(2)?0x2:0x0;
	garmentIndex += decorationBitfield.IsSet(3)?0x4:0x0;
	garmentIndex += decorationBitfield.IsSet(4)?0x8:0x0;
	garmentIndex += decorationBitfield.IsSet(5)?0x10:0x0;

	// now find our faction and garment type (based on the decorations we have)
	if (!PedVerifyf(garmentIndex<m_garmentList.GetCount(),"garmentIndex (bit 1-5 of the pInBitField are not valid. they specify garmet Index %d, but the m_garmentList.GetCount() = %d",garmentIndex,m_garmentList.GetCount()))
		return false;

	atHashString garmentHash = m_garmentList[garmentIndex];
 

#if __BANK
	if (gDebugSerialization)
	{
		Displayf("deserialization Gender = %s", gender==GENDER_FEMALE ? "GENDER_FEMALE" : "GENDER_MALE");
		Displayf("deserialization Garment = %s", garmentHash.TryGetCStr());
	}
#endif

	bool bOk = true;
		
	int presetCount = m_presetIndexList.GetCount();
		
	//before actually adding the preset, check to see if they are already set up. if so skip this to avoid recompression 
	static const int maxDecorations = 64; // should never even be close to this.

	u32 collectionHash[maxDecorations];
	atHashString presetName[maxDecorations];
	u32 actualCount = 0;
	bool perfectMatch = true;

	for (int i = 0; i < kMaxUniquePresets; i++)
	{
		if (decorationBitfield.IsSet(kGarmentGenderBits+i))
		{
			if (!PedVerifyf(i<presetCount,"pInBitfield indicates a present beyound the valid range is active ( preset bit %d is set,  m_presetIndexList.GetCount() = %d)",i,presetCount))
				continue;

			u32 colIndex = GetCollectionIndex(m_presetIndexList[i]);
			u32 presetIndex = GetPresetIndex(m_presetIndexList[i]);

			if (!PedVerifyf(colIndex < m_collections.GetCount(), "Invalid collection index in decoration bitfield! (%d / %d)", colIndex, m_collections.GetCount()))
				continue;

			PedDecorationCollection* pCollection = &m_collections[colIndex];

			// find basic preset
			const PedDecorationPreset* pBasePreset = pCollection->GetAt(presetIndex);
			const PedDecorationPreset* pFoundPreset = NULL;

			if (pBasePreset != NULL)
			{
				// now look for the one with a full match (including gender and clothing)
				for (int idx=presetIndex;idx<pCollection->GetCount();idx++)
				{
					const PedDecorationPreset* pPreset = pCollection->GetAt(idx);

					if (pPreset && pBasePreset->IsSimilar(*pPreset))  // find other "similar" presets
					{
						// check for full match
						if ((pPreset->GetGarmentHash()==garmentHash || pPreset->GetGarmentHash()==ATSTRINGHASH("All",0x45835226)) && 
							(pPreset->GetGender()==gender || pPreset->GetGender()==GENDER_DONTCARE))
						{
							if (PedVerifyf(pFoundPreset==NULL,"Found more than one match of serialize preset (%s and %s)", pFoundPreset->GetNameCStr(),pPreset->GetNameCStr()))
							{
								pFoundPreset = pPreset;	
#if !__ASSERT
								break; // if we are not going to complain about duplicates, just skip the rest.
#endif
							}
						}
					}
				}	

				if (pFoundPreset==NULL)
				{
#if __ASSERT
					const char * modelName="UnKnown";
					const char * personalityName="UnKnown";

					fwModelId pedModelId = pPed->GetModelId();
					if (pedModelId.IsValid())
					{
						if (const CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId))
						{
							modelName = pPedModelInfo->GetModelName();
							personalityName = pPedModelInfo->GetPersonalitySettings().GetPersonalityName();
						}
					}
					
					PedAssertf(pFoundPreset!=NULL,"could not find an exact match in the decoration list. Base Type = '%s', Garment = '%s', Gender= '%s', Model = '%s', personality = '%s'",pBasePreset->GetNameCStr(), garmentHash.TryGetCStr(), gender==GENDER_MALE?"GENDER_MALE":"GENDER_FEMALE", modelName, personalityName);

#if !__NO_OUTPUT
					//Invoke a failmark if we are in an network game - this will log what all the local and remote players are wearing at this time. lavalley.
					static bool s_bLogInitialFailmark = false;
					if (NetworkInterface::IsGameInProgress() && !s_bLogInitialFailmark && pPed->IsNetworkClone())
					{
						NetworkDebug::AddFailMark("FAIL MARK");
						s_bLogInitialFailmark = true;
					}
#endif
#endif
				}
				else
				{
					if (AssertVerify(actualCount<maxDecorations))
					{
						// keep track of the presets, in csae we are not a match
						collectionHash[actualCount] = pCollection->GetName().GetHash();
						presetName[actualCount] = pFoundPreset->GetName();
						actualCount++;
						perfectMatch = perfectMatch && CheckPlaceDecoration(pPed, pCollection->GetName().GetHash(), pFoundPreset->GetName());
					}
				}
			}
		}
	}

	if (PEDDAMAGEMANAGER.GetCompressedDamageSet(pPed) && perfectMatch)
	{	
		perfectMatch = (actualCount == PEDDAMAGEMANAGER.GetCompressedDamageSet(pPed)->GetNumDecorations()); // if the counts do not match it can't be perfect  :)
	}

	// if we're not a perfect match, we need to rebuild them
	if (!perfectMatch)
	{
		ClearCompressedPedDecorations(pPed); // clear out any old decorations before adding new list
		for (int i=0;i<actualCount;i++)
			bOk = bOk && PlaceDecoration(pPed, collectionHash[i], presetName[i], crewEmblemVariation);
	}

	return bOk; 
}

bool PedDecorationManager::PlaceDecoration(CPed* pPed, atHashString collectionName, atHashString presetName, int crewEmblemVariation)
{
	if (!pPed)
		return false;

	if (!PEDDECORATIONBUILDER.IsUsingCompressedMPDecorations() || pPed->IsLocalPlayer())
	{
		AddPedDecoration(pPed, collectionName, presetName);
		return true;
	}
	else
	{
		return AddCompressedPedDecoration(pPed, collectionName, presetName, crewEmblemVariation);
	}
}

bool PedDecorationManager::CheckPlaceDecoration(CPed* pPed, atHashString collectionName, atHashString presetName)
{
	if (!PEDDECORATIONBUILDER.IsUsingCompressedMPDecorations())
	{
		return false; // don't care if we're not compressed, just rebuild the list.
	} 
	else
	{
		return PEDDAMAGEMANAGER.CheckForCompressedPedDecoration(pPed, collectionName, presetName);
	}
}

ePedDecorationZone PedDecorationManager::GetPedDecorationZone(atHashString collectionName, atHashString presetName) const
{
	const PedDecorationCollection* pCollection = NULL;
	if (FindCollection(collectionName, pCollection) == false)
	{
		Displayf("PedDecorationManager::FindCollection: couldn't find collection \"%s\"", collectionName.TryGetCStr());
		return ZONE_INVALID;
	}

	const PedDecorationPreset* pPreset = pCollection->Get(presetName);

	if (pPreset==NULL)
	{
		Errorf("PedDecorationManager::FindCollection: couldn't find preset \"%s\" in collection \"%s\"", presetName.TryGetCStr(),collectionName.TryGetCStr());
		return ZONE_INVALID;
	}

	return pPreset->GetZone();
}

ePedDecorationZone PedDecorationManager::GetPedDecorationZoneByIndex(int collectionIndex, atHashString presetName) const
{
    if (collectionIndex < 0 || collectionIndex >= m_collections.GetCount())
        return ZONE_INVALID;
    
    return GetPedDecorationZone(m_collections[collectionIndex].GetName().GetHash(), presetName);
}

int PedDecorationManager::GetPedDecorationIndex(atHashString collectionName, atHashString presetName) const
{
	int index = -1;

	if (collectionName.IsNull() || presetName.IsNull())
	{
		Displayf("PedDecorationManager::GetPedDecorationIndex: invalid hash");
		return index;
	}

	const PedDecorationCollection* pCollection;
	if (FindCollection(collectionName, pCollection) == false)
	{
		Displayf("PedDecorationManager::GetPedDecorationIndex: collection \"%s\" not found", collectionName.TryGetCStr());
		return index;
	}

	Assert(pCollection);
	index = pCollection->GetIndex(presetName);

	return index;
}

int PedDecorationManager::GetPedDecorationIndexByIndex(int collectionIndex, atHashString presetName) const
{
    if (collectionIndex < 0 || collectionIndex >= m_collections.GetCount())
        return -1;

	return GetPedDecorationIndex(m_collections[collectionIndex].GetName().GetHash(), presetName);
}

atHashString PedDecorationManager::GetPedDecorationHashName(atHashString collectionName, int idx) const
{
	atHashString hashName;

	if (collectionName.IsNull())
	{
		Displayf("PedDecorationManager::GetPedDecorationHashName: invalid hash");
		return hashName;
	}

	const PedDecorationCollection* pCollection;
	if (FindCollection(collectionName, pCollection) == false)
	{
		Displayf("PedDecorationManager::GetPedDecorationHashName: collection \"%s\" not found", collectionName.TryGetCStr());
		return hashName;
	}

	Assert(pCollection);
	
	const PedDecorationPreset* pPreset;
	pPreset = pCollection->GetAt(idx);

	if (pPreset != NULL) 
	{
		hashName = pPreset->GetName();
	}

	return hashName;
}

PedDecorationPreset* PedDecorationManager::AddPedDecorationCommon(CPed* pPed, atHashString collectionName, atHashString presetName, 
																  ePedDamageZones &zone, Vector2 &uvs, float &rotation, Vector2 &scale,
																  const char* OUTPUT_ONLY(funcName))
{
	if (pPed == NULL || collectionName.IsNull() || presetName.IsNull())
	{
		Displayf("PedDecorationManager::%s: failed to add decoration",funcName);
		return NULL;
	}

	PedDecorationCollection* pCollection;

	if (FindCollection(collectionName, pCollection) == false)
	{
		Displayf("PedDecorationManager::%s: collection \"%s\" not found", funcName, collectionName.TryGetCStr());
		return NULL;
	}

	Assert(pCollection);   
	PedDecorationPreset* pPreset = pCollection->Find(presetName);

	if (pPreset == NULL)
	{
		Displayf("PedDecorationManager::%s: preset \"%s\" not found in collection \"%s\"", funcName, presetName.TryGetCStr(), collectionName.TryGetCStr());
		return NULL;
	}

	if (pPreset->GetType() == TYPE_INVALID)
	{
		Displayf("PedDecorationManager::%s: preset \"%s\" in collection \"%s\" has invalid type (INVALID_TYPE in data file?)", funcName, presetName.TryGetCStr(), collectionName.TryGetCStr());
		return NULL;
	}

	ComputePlacement(pPed, pCollection, pPreset, zone, uvs, rotation, scale);

	return pPreset;
}

void PedDecorationManager::ComputePlacement(CPed* pPed, const PedDecorationCollection* pCollection, const PedDecorationPreset* pPreset, ePedDamageZones &zone, Vector2 &uvs, float &rotation, Vector2 &scale)
{
	// if it's a police medal we will overwrite the uv and scale info.
	if (pPreset->GetZone()==ZONE_MEDALS)
	{
		int exisitingDecorationIndex;
		int medalIndex = GetPedMedalIndex(pPed, pCollection, pPreset, exisitingDecorationIndex);
		if (medalIndex<0) 
			return;

		zone = kDamageZoneMedals;
		uvs = pCollection->GetMedalUV(medalIndex);
		rotation = 0;
		scale = pCollection->GetMedalScale();

		if (exisitingDecorationIndex>=0)
			ClearPedDecoration(pPed, exisitingDecorationIndex) ;
	}
	else
	{
		zone = static_cast<ePedDamageZones>(pPreset->GetZone());
		uvs = pPreset->GetUVPos();
		rotation = pPreset->GetRotation();
		scale = pPreset->GetScale();
	}
}

bool PedDecorationManager::AddPedDecoration(CPed* pPed, atHashString collectionName, atHashString presetName, int crewEmblemVariation, float alpha)
{
	ePedDamageZones zone;
	Vector2 uvs,scale;
	float rotation;

	PedDecorationPreset* pPreset = AddPedDecorationCommon(pPed, collectionName, presetName, zone, uvs, rotation, scale, "AddPedDecoration");
	
	if (pPreset) 
	{
		PEDDAMAGEMANAGER.AddPedDecoration(pPed, collectionName, presetName, zone, uvs, rotation, scale, pPreset->UsesTintColor(), pPreset->GetTXDName(), pPreset->GetTextureName(), false, alpha, pPreset->GetType() !=  TYPE_TATTOO, crewEmblemVariation);

#if GTA_REPLAY
		if(pPed && (CReplayMgr::ShouldRecord() || CReplayMgr::IsReplayInControlOfWorld()))
		{
			//if we don't have the extension then we need to get one
			if(!ReplayPedExtension::HasExtension(pPed))
			{
				if(!ReplayPedExtension::Add(pPed))
					replayAssertf(false, "Failed to add a ped extension...ran out?");
			}
			ReplayPedExtension::SetPedDecorationData(pPed, collectionName, presetName, crewEmblemVariation, false, alpha);
		}
#endif //GTA_REPLAY

		return true;
	}

	return false;
}

bool PedDecorationManager::AddPedDecorationByIndex(CPed* pPed, int collectionIndex, atHashString presetName)
{
    if (collectionIndex < 0 || collectionIndex >= m_collections.GetCount())
        return false;

	return AddPedDecoration(pPed, m_collections[collectionIndex].GetName().GetHash(), presetName);
}

bool PedDecorationManager::AddCompressedPedDecoration(CPed* pPed, atHashString collectionName, atHashString presetName, int crewEmblemVariation)
{
	ePedDamageZones zone;
	Vector2 uvs,scale;
	float rotation;

	PedDecorationPreset* pPreset = AddPedDecorationCommon(pPed, collectionName, presetName, zone, uvs, rotation, scale, "AddCompressedPedDecoration");
	
	if (pPreset) 
	{
		bool result = PEDDAMAGEMANAGER.AddCompressedPedDecoration(pPed, collectionName, presetName, zone, uvs, rotation, scale, pPreset->UsesTintColor(), pPreset->GetTXDName(), pPreset->GetTextureName(), pPreset->GetType() !=  TYPE_TATTOO, crewEmblemVariation);

#if GTA_REPLAY
		if(pPed && (CReplayMgr::ShouldRecord() || CReplayMgr::IsReplayInControlOfWorld()))
		{
			//if we don't have the extension then we need to get one
			if(!ReplayPedExtension::HasExtension(pPed))
			{
				if(!ReplayPedExtension::Add(pPed))
					replayAssertf(false, "Failed to add a ped extension...ran out?");
			}
			ReplayPedExtension::SetPedDecorationData(pPed, collectionName, presetName, crewEmblemVariation, true, 1.0f);  // do we need to add alpha support here?
		}
#endif //GTA_REPLAY

		return result;
	}

	return false;
}

void PedDecorationManager::ClearPedDecorations(CPed* pPed)
{
	if (pPed && pPed->GetDamageSetID() != kInvalidPedDamageSet)
	{
		PEDDAMAGEMANAGER.ClearPedDecorations(pPed->GetDamageSetID());

#if GTA_REPLAY
		if(pPed && ReplayPedExtension::HasExtension(pPed))
		{
			ReplayPedExtension::ClearPedDecorationData(pPed);
		}
#endif //GTA_REPLAY
	}
}

void PedDecorationManager::ClearCompressedPedDecorations(CPed* pPed)
{
	pedDebugf3("PedDecorationManager::ClearCompressedPedDecorations pPed[%p]",pPed);

	if (pPed && pPed->GetCompressedDamageSetID() != kInvalidPedDamageSet)
	{
		PEDDAMAGEMANAGER.ClearCompressedPedDecorations(pPed->GetCompressedDamageSetID());

#if GTA_REPLAY
		if(pPed && ReplayPedExtension::HasExtension(pPed))
		{
			ReplayPedExtension::ClearPedDecorationData(pPed);
		}
#endif //GTA_REPLAY
	}

	// hack: if we are not using compressed textures the decorations are in the normal decoration list, so if they are trying to clear tattoos, we should clear those too.
	if (!PEDDECORATIONBUILDER.IsUsingCompressedMPDecorations() && pPed && pPed->GetDamageSetID() != kInvalidPedDamageSet)
		PEDDAMAGEMANAGER.ClearPedDecorations(pPed->GetDamageSetID() , true);
}

void PedDecorationManager::ClearClanDecorations(CPed* pPed)
{
	if (pPed && pPed->GetDamageSetID() != kInvalidPedDamageSet)
	{
		PEDDAMAGEMANAGER.ClearClanDecorations(pPed->GetDamageSetID());
	}
}

// clears a specific blit so we can reuses it for medals.
void PedDecorationManager::ClearPedDecoration(CPed* pPed, int decorationIdx) 
{
	if (pPed)
	{
		u8 dmgIdx = pPed->GetCompressedDamageSetID();
		if (dmgIdx != kInvalidPedDamageSet)
		{
			PEDDAMAGEMANAGER.ClearCompressedPedDecoration(dmgIdx, decorationIdx);
		}
		else
		{
			dmgIdx = pPed->GetDamageSetID();
			if (dmgIdx != kInvalidPedDamageSet)
				PEDDAMAGEMANAGER.ClearPedDecoration(dmgIdx, decorationIdx);
		}
	}
}

int PedDecorationManager::GetPedMedalIndex(CPed* pPed, const PedDecorationCollection * pCollection, const PedDecorationPreset* pPreset, int &existingDecorationIndex)
{
	int medalCount = 0;

	existingDecorationIndex = -1;
	atHashString collectionName(pCollection->GetName().GetHash());
	
	u32 count = GetPedDecorationCount(pPed);
	int i;

	for (i=0; i<count; i++)
	{
		atHashString activeCollectionName;
		atHashString activePresetName;

		if (!GetPedDecorationInfo(pPed, i, activeCollectionName, activePresetName) )
			continue;

		if (activeCollectionName != collectionName)
			continue;

		const PedDecorationPreset * activePreset = pCollection->Get(activePresetName);
		
		if (activePreset==NULL)
			continue;

		if (activePreset->GetZone()==ZONE_MEDALS)
		{
			if (activePreset->GetAwardHash()==pPreset->GetAwardHash() && 
				activePreset->GetFactionHash()==pPreset->GetFactionHash())
			{
				// should we also check that out new medals higher rank?
				existingDecorationIndex = i; // we've got the same award type, we'll need to clear this one.
				break;
			}

			medalCount++;
		}
	}

	if (Verifyf(medalCount<=16,"trying to add more than 16 medals to the medal grid"))
		return medalCount;
	else
		return -1;
}

u32	PedDecorationManager::GetPedDecorationCount(CPed* pPed) const
{
	// try first for a compressed damage set
	const CCompressedPedDamageSet* pCompressedDamageSet = PEDDAMAGEMANAGER.GetCompressedDamageSet(pPed);
	if (pCompressedDamageSet != NULL)
	{
		return pCompressedDamageSet->GetNumDecorations();
	}

	// this should be the player ped only
	const CPedDamageSet* pDamageSet = PEDDAMAGEMANAGER.GetDamageSet(pPed);
	if (pDamageSet != NULL)
	{
		return pDamageSet->GetNumDecorations();
	}

	return 0;
}

bool PedDecorationManager::GetPedDecorationInfo(CPed* pPed, int decorationIdx, atHashString& collectionName, atHashString& presetName) const
{
	// try first for a compressed damage set
	const CCompressedPedDamageSet* pCompressedDamageSet = PEDDAMAGEMANAGER.GetCompressedDamageSet(pPed);
	if (pCompressedDamageSet != NULL)
	{
		return pCompressedDamageSet->GetDecorationInfo(decorationIdx, collectionName, presetName);
	}

	// this should be the player ped only
	const CPedDamageSet* pDamageSet = PEDDAMAGEMANAGER.GetDamageSet(pPed);
	if (pDamageSet != NULL)
	{
		return pDamageSet->GetDecorationInfo(decorationIdx, collectionName, presetName);
	}

	return false;
}

void PedDecorationManager::AddCollection(const char* pFilename, bool bUpdateSerializerList /*= false*/)
{
	PedDecorationCollection* pCol;
	char baseName[80];
	const char* pJustFileName = ASSET.FileName(pFilename);
	ASSET.BaseName(baseName, (int)strlen(pJustFileName), pJustFileName);

	if (FindCollection(baseName, pCol) == false)
	{
		PedDecorationCollection& curCollection = m_collections.Grow();
		if ( curCollection.Load(pFilename) )
		{
			PedAssertf((curCollection.GetName() == atStringHash(baseName)), "name of collection incorrect in %s", pFilename);
#if  __BANK
			VerifyForSerialization(&curCollection);
#endif
			if(bUpdateSerializerList)
			{
				BuildSerializerLists(m_presetIndexList,m_garmentList);
			}
		}
		else
		{
			m_collections.Pop();
			Displayf("PedDecorationManager::AddCollection: failed to add collection \"%s\"", pFilename);
		}
	}
}

void PedDecorationManager::RemoveCollection(const char* pFilename)
{
	PedDecorationCollection* col = rage_new PedDecorationCollection;
	if(PARSER.LoadObject(pFilename, NULL, *col))
	{
		for(int i=0;i<m_collections.GetCount();i++)
		{
			if(m_collections[i].GetName()==col->GetName())
			{
				m_collections.Delete(i);
				i--;
				BuildSerializerLists(m_presetIndexList,m_garmentList);
#if __BANK
				UpdateDebugTool();
#endif
			}
		}
	}
	delete col;
}

void PedDecorationManager::Update()
{
	m_crewEmblemDecalMgr.Update();

#if __BANK
	UpdateDebugTool();
#endif
}

bool PedDecorationManager::FindCollection(atHashString collectionName, PedDecorationCollection*& pCollectionOut)
{
	for (int i=0; i<m_collections.GetCount(); i++)
	{
		if (m_collections[i].GetName() == collectionName)
		{
			pCollectionOut = &m_collections[i];
			return true;
		}
	}

	return false;
}

bool PedDecorationManager::FindCollection(const char* pFilename, PedDecorationCollection*& pCollectionOut)
{
	return FindCollection(atHashString(pFilename), pCollectionOut);
}

bool PedDecorationManager::FindCollection(atHashString collectionName, const PedDecorationCollection*& pCollectionOut) const
{
	for (int i=0; i<m_collections.GetCount(); i++)
	{
		if (m_collections[i].GetName() == collectionName)
		{
			pCollectionOut = &m_collections[i];
			return true;
		}
	}

	return false;
}

bool PedDecorationManager::FindCollection(const char* pFilename, const PedDecorationCollection*& pCollectionOut) const
{
	return FindCollection(atHashString(pFilename), pCollectionOut);
}

bool PedDecorationManager::FindCollectionIndex(atHashString collectionName, u32& outCollectionIndex) const
{
	for (int i=0; i<m_collections.GetCount(); i++)
	{
		if (m_collections[i].GetName() == collectionName)
		{
            outCollectionIndex = i;
			return true;
		}
	}

	return false;
}

bool PedDecorationManager::GetCrewEmblemDecal(const CPed* pPed, grcTexture*& pTexture)
{
	m_crewEmblemDecalMgr.GetTexture(pPed, pTexture);
	return (pTexture != NULL);
}

#if __BANK
void PedDecorationManager::TestCompressedDecoration()
{
	if (CPed * pPed = GetDebugPed())
	{
		u32 idx = 1;
		atHashString presetHash("FM_Tshirt_Award_000");
		ePedDamageZones zone;
		Vector2 uvs,scale;
		float rotation;

		PedDecorationPreset* pPreset = m_collections[idx].Get(presetHash);

		u8 damageIndex = pPed->GetDamageSetID();
		if (damageIndex!=kInvalidPedDamageSet)
		{
			PEDDAMAGEMANAGER.ClearPedDecorations(damageIndex); // clear previous decoration (or they stack up)
		}

		ComputePlacement(pPed, &m_collections[idx], pPreset, zone, uvs, rotation, scale);
		PEDDAMAGEMANAGER.AddCompressedPedDecoration(pPed, atHashString(m_collections[idx].GetName().GetHash()), pPreset->GetName(), zone, uvs, rotation, scale, pPreset->UsesTintColor(), pPreset->GetTXDNameCStr(), pPreset->GetTextureNameCStr(), pPreset->GetType() !=  TYPE_TATTOO);
	}

}

void PedDecorationManager::UpdateDebugTool()
{
	int idx = m_collectioNamesComboIdx;

	if(m_bTestCompressedDecoration)
	{
		TestCompressedDecoration();
		m_bTestCompressedDecoration = false;
	}

	if ((m_bVisualDebug == false) || (idx<0 || idx>=m_collections.GetCount()))
	{
		return;
	}

	const PedDecorationPreset* pPreset = m_collections[idx].GetDebugPreset(m_bUpdateOnPresetSelect);

	if (ValidatePreset(pPreset) == false)
	{
		return;
	}

	CPed* pPed = GetDebugPed();
	if (pPed == NULL)
	{
		return;
	}

	ePedDamageZones zone;
	Vector2 uvs,scale;
	float rotation;

	if (PEDDECORATIONBUILDER.IsUsingCompressedMPDecorations() == false)
	{
		u8 dmgIdx = pPed->GetDamageSetID();
		if (dmgIdx!=kInvalidPedDamageSet && !m_bUpdateOnPresetSelect)
		{
			PEDDAMAGEMANAGER.ClearPedDecorations(dmgIdx);
		}

		// call the PEDDAMAGEMANAGER directly, otherwise we won't be updating the decoration with the fresh data from
		// the debug tool...
		ComputePlacement(pPed, &m_collections[idx], pPreset, zone, uvs, rotation, scale);
		PEDDAMAGEMANAGER.AddPedDecoration(pPed, atHashString(m_collections[idx].GetName().GetHash()), pPreset->GetName(), zone, uvs, rotation, scale, pPreset->UsesTintColor(), pPreset->GetTXDNameCStr(), pPreset->GetTextureNameCStr(), pPreset->GetType() !=  TYPE_TATTOO);

		m_collections[idx].AddVisibleDecorations(pPed);
	}
	else if (gUpdateCompressedDecoration)
	{
		if (!m_bUpdateOnPresetSelect)
			ClearCompressedPedDecorations(pPed);
		
		// call the PEDDAMAGEMANAGER directly, otherwise we won't be updating the decoration with the fresh data from
		// the debug tool...
		ComputePlacement(pPed, &m_collections[idx], pPreset, zone, uvs, rotation, scale);
		PEDDAMAGEMANAGER.AddCompressedPedDecoration(pPed, atHashString(m_collections[idx].GetName().GetHash()), pPreset->GetName(), zone, uvs, rotation, scale, pPreset->UsesTintColor(), pPreset->GetTXDNameCStr(), pPreset->GetTextureNameCStr(), pPreset->GetType() !=  TYPE_TATTOO);
		gUpdateCompressedDecoration = false;

#if 0
		// debug many blits at once
		for (int i = 0; i < 16; i++) 
		{
			pPreset = m_collections[idx].GetAt(i);
			ComputePlacement(pPed, &m_collections[idx], pPreset, zone, uvs, rotation, scale);
			PEDDAMAGEMANAGER.AddCompressedPedDecoration(pPed, m_collections[idx].GetName(), pPreset->GetName(), zone, uvs, rotation, scale, pPreset->UsesTintColor(), pPreset->GetTXDNameCStr(), pPreset->GetTextureNameCStr(), pPreset->GetType() !=  TYPE_TATTOO);
		}
#endif
	}

}

bool PedDecorationManager::ValidatePreset(const PedDecorationPreset* pPreset) const
{
	if ((pPreset == NULL) || (pPreset->GetTXDName().IsNull()) || (pPreset->GetTextureName().IsNull()))
	{
		return false;
	}

	return true;
}

void PedDecorationManager::PopulateCollectionNamesList()
{
	m_collectioNamesComboIdx = 0;
	for (int i=0; i<m_collections.GetCount(); i++)
	{
		m_collectionNames.PushAndGrow(m_collections[i].GetNameCStr());
	}
}

void PedDecorationManager::CheckDebugPedChange()
{
	if (m_bUseDebugPed && m_pDebugPed != CPedVariationDebug::GetDebugPed())
	{

	}
}

CPed* PedDecorationManager::GetDebugPed()
{
	if (m_bUseDebugPed)
	{
		m_pDebugPed = CPedVariationDebug::GetDebugPed();
		return m_pDebugPed;
	}

	return CGameWorld::FindLocalPlayer();
}

void PedDecorationManager::AddWidgets(rage::bkBank& bank )
{

	PopulateCollectionNamesList();

	if (m_collections.GetCount() == 0)
	{
		m_pGroup = bank.PushGroup("Ped Decoration Presets", false);	
			bank.AddTitle("No collections found");
		bank.PopGroup();
		return;
	}

	m_pButtonAddWidgets = NULL;
	m_pGroup = NULL;

	m_pGroup = bank.PushGroup("Ped Decoration Presets", false);
	m_pButtonAddWidgets = bank.AddButton("Create Ped Decoration Presets Widgets", datCallback(MFA(PedDecorationManager::AddWidgetsOnDemand), (datBase*)this));
	bank.PopGroup();

}

void PedDecorationManager::TestSerialization()
{
	const bool bPreviousSer = gDebugSerialization;
	gDebugSerialization = true;
	if ( CPed* pPed = PEDDECORATIONMGR.GetDebugPed() )
	{
		const int idx = PEDDECORATIONMGR.m_collectioNamesComboIdx;
		if (idx>=0 && idx<PEDDECORATIONMGR.m_collections.GetCount())
		{
			u32 bitField[NUM_DECORATION_BITFIELDS];
			int crewEmblemVariation;
			PEDDECORATIONMGR.GetDecorationsBitfieldFromLocalPed(pPed, bitField, NUM_DECORATION_BITFIELDS, crewEmblemVariation);
			PEDDECORATIONMGR.ProcessDecorationsBitfieldForRemotePed(pPed, bitField, NUM_DECORATION_BITFIELDS, crewEmblemVariation);
		}
	}

	gDebugSerialization = bPreviousSer;
}

void PedDecorationManager::AddWidgetsOnDemand()
{
	bkBank& bank = *BANKMGR.FindBank("Peds"); 

	// remove old button:
	if(m_pButtonAddWidgets)
	{
		bank.Remove(*((bkWidget*)m_pButtonAddWidgets));
		m_pButtonAddWidgets = NULL;
	}

	bkGroup* prevGroup = static_cast<bkGroup*>(bank.GetCurrentGroup());
	if (prevGroup != NULL)
	{
		bank.UnSetCurrentGroup(*prevGroup);
	}

	bank.SetCurrentGroup(*m_pGroup);

	bank.PushGroup("Programmers' Debug Tools");
	PEDDECORATIONBUILDER.AddToggleWidget(bank);
	bank.AddToggle("Test Compressed Decoration", &m_bTestCompressedDecoration);
	bank.AddToggle("Update Compressed Decoration", &gUpdateCompressedDecoration);
	bank.AddButton("TestSerialization",datCallback(CFA(PedDecorationManager::TestSerialization))); 
	bank.PopGroup();

	bank.AddCombo("Collection to Debug:", &m_collectioNamesComboIdx, m_collectionNames.GetCount(), &m_collectionNames[0], NullCB);
	bank.AddToggle("Visualise Preset on Ped", &m_bVisualDebug);
	bank.AddToggle("Use Presets on Debug Ped", &m_bUseDebugPed);
 	bank.AddToggle("Medals test (add decoration when preset is selected, no auto clearing)", &m_bUpdateOnPresetSelect);
	bank.AddSeparator();

	bank.PushGroup("Edit Collections:");
	for (int i = 0; i < m_collections.GetCount(); i++)
	{
		m_collections[i].AddWidgets(bank);
	}
	bank.PopGroup();

	bank.UnSetCurrentGroup(*m_pGroup);

	if (prevGroup != NULL)
	{
		bank.SetCurrentGroup(*prevGroup);
	}

}

#endif // __BANK

//////////////////////////////////////////////////////////////////////////
//
const u32 PedDecalDecoration::MAX_FRAMES_UNUSED = 128U;

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecoration::Reset()
{
	m_emblemDesc.Invalidate();
	m_txdId = -1;
	m_refCount = 0;
	m_bRequested = false;
	m_bReady = false;
	m_bFailed = false;
	m_bReleasePending = false;
	m_bRefAddedThisFrame = false;
	m_framesSinceLastUse = 0U;
}

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::Init()
{
	// Initialise the managed txd slots list
	m_activeList.Init(MAX_DECAL_DECORATIONS);
}

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::Shutdown()
{
	ReleaseAll();
	m_activeList.Shutdown();
}

//////////////////////////////////////////////////////////////////////////
//
bool PedDecalDecorationManager::AddDecal(const EmblemDescriptor& emblemDesc, PedDecalDecoration*& pDecal)
{
	// Really?
	if (!AssertVerify(emblemDesc.IsValid()))
	{
		return false;
	}

	// If we're full, try later
	if (m_activeList.GetNumFree() <= 0)
	{
		return false;
	}

	// Insert a new decal
	PedDecalDecorationNode* pNode = m_activeList.Insert();
	pDecal = &(pNode->item);
	pDecal->Reset();

	// Request the emblem
	bool bOk = NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestEmblem(emblemDesc  ASSERT_ONLY(, "PedDecalDecorationManager"));

	// Failed? Remove it
	if (bOk == false)
	{
		pDecal = NULL;
		m_activeList.Remove(pNode);
		return false;
	}

	// All good - set it up
	pDecal->m_emblemDesc = emblemDesc;
	pDecal->m_bRequested = true;


	return bOk;
}

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::ReleaseAll()
{

	// Find matching txd slot
	PedDecalDecorationNode* pNode = m_activeList.GetFirst()->GetNext();
	while(pNode != m_activeList.GetLast())
	{
		PedDecalDecoration* pCurEntry = &(pNode->item);
		PedDecalDecorationNode* pLastNode = pNode;
		pNode = pNode->GetNext();
		
		// Release decal
		ReleaseDecal(pCurEntry);

		// Forget about it
		m_activeList.Remove(pLastNode);
	}


}

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::ReleaseDecal(PedDecalDecoration* pDecal)
{
	// We did request it
	if (pDecal->m_bRequested)
	{
		// Remove all pending refs
		while (pDecal->m_refCount > 0)
		{
			if (pDecal->m_txdId == -1 || g_TxdStore.GetNumRefs(pDecal->m_txdId) <= 1)
			{
				pDecal->m_refCount = 0;
				break;
			}

			g_TxdStore.RemoveRef(pDecal->m_txdId, REF_OTHER);
			pDecal->m_refCount--;
		}

		// Tell the crew emblem manager we're not interested in the emblem anymore
		NETWORK_CLAN_INST.GetCrewEmblemMgr().ReleaseEmblem(pDecal->m_emblemDesc  ASSERT_ONLY(, "PedDecalDecorationManager"));

		// Forget about the decal
		pDecal->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::Update()
{
	// Nothing to do
	if (m_activeList.GetNumUsed() == 0)
	{
		return;
	}

	PedDecalDecorationNode* pNode = m_activeList.GetFirst()->GetNext();
	while(pNode != m_activeList.GetLast())
	{
		PedDecalDecoration* pCurEntry = &(pNode->item);
		PedDecalDecorationNode* pLastNode = pNode;
		pNode = pNode->GetNext();

		// Regardless of its state, update the number of frames since this decal was last used
		// Whenever the texture is accessed, this counter gets reset
		pCurEntry->IncFramesSinceLastUse();

		// Process failed requests first
		if (pCurEntry->m_bFailed)
		{			
			ProcessFailedDecal(pCurEntry);
			// Remove it now
			m_activeList.Remove(pLastNode);
		}
		// Then requests ready for release
		else if (pCurEntry->m_bReleasePending)
		{
			ProcessReleasePendingDecal(pCurEntry);
			// Remove it now
			m_activeList.Remove(pLastNode);
		}
		// Request is still pending
		else if (pCurEntry->m_bReady == false)
		{
			ProcessRequestPendingDecal(pCurEntry);
		}
		// Update already active decals
		else if (pCurEntry->m_bReady)
		{
			ProcessActiveDecal(pCurEntry);
		}
		#if __ASSERT
		// this is not a known state...
		else
		{
			Assertf(0, "PedDecalDecorationManager::Update: found decal in unknown state");
		}
		#endif
	}
}

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::ProcessActiveDecal(PedDecalDecoration* pDecal)
{
	// If the decal has no refs and its texture hasn't been used for a while, mark it for release
	if (pDecal->m_refCount == 0 && pDecal->m_framesSinceLastUse >= PedDecalDecoration::MAX_FRAMES_UNUSED)
	{
		pDecal->m_bReady = false;
		pDecal->m_bReleasePending = true;
	}
	// The decal is active, decrease its ref count
	else if (pDecal->m_bRefAddedThisFrame && pDecal->m_refCount > 0)
	{
		if (Verifyf(pDecal->m_txdId != -1, "Found invalid txd slot in active ped decal"))
		{
			g_TxdStore.RemoveRef(pDecal->m_txdId, REF_OTHER);
		}
		pDecal->m_bRefAddedThisFrame = false;
		pDecal->m_refCount--;
	}

}
//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::ProcessRequestPendingDecal(PedDecalDecoration* pDecal)
{
	// Bail if request failed
	if (NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestFailed(pDecal->m_emblemDesc))
	{
		pDecal->m_bFailed = true;
		return;
	}

	// Mark as ready if the txd is around
	s32 txdSlot = (s32)NETWORK_CLAN_INST.GetCrewEmblemMgr().GetEmblemTXDSlot(pDecal->m_emblemDesc);
	if (txdSlot != -1)
	{
		Assert(pDecal->m_refCount == 0);
		pDecal->m_txdId = txdSlot;
		pDecal->m_bReady = true;
	}

}

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::ProcessFailedDecal(PedDecalDecoration* pDecal)
{
	ReleaseDecal(pDecal);
}

//////////////////////////////////////////////////////////////////////////
//
void PedDecalDecorationManager::ProcessReleasePendingDecal(PedDecalDecoration* pDecal)
{
	ReleaseDecal(pDecal);
}

//////////////////////////////////////////////////////////////////////////
//
bool PedDecalDecorationManager::GetTexture(const CPed* pPed, grcTexture*& pTexture)
{
	pTexture = NULL;

	if (pPed == NULL)
	{
		return false;
	}

	rlClanId clanId = -1;

	// Do we have a valid clan id?
	bool bValidClanId = CPedDamageManager::GetClanIdForPed(pPed, clanId);

	// Carry on if we got one
	if  (bValidClanId && clanId != -1)
	{
		// We do, try to find a match and if so update its ref count, etc.
		PedDecalDecoration* pDecal;
		EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)clanId);
		if (FindActiveDecal(emblemDesc, pDecal))
		{
			//  Try getting a texture back (it might not be ready yet).
			bool bOk = RequestActiveDecalTexture(pDecal, pTexture);
			return bOk;
		}

		// We don't have a match, register one.
		return AddDecal(emblemDesc, pDecal);
	}


	return false;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedDecalDecorationManager::RequestActiveDecalTexture(PedDecalDecoration* pDecal, grcTexture*& pTexture)
{
	// Don't let this texture get used...
	if (CanUseDecal(pDecal) == false || g_TxdStore.GetNumRefs(pDecal->m_txdId) <= 1)
	{
		return false;
	}

	// Add a ref if we haven't this frame
	if (pDecal->m_bRefAddedThisFrame == false)
	{
		// Safe to use, add a ref to the TXD
		pDecal->m_refCount++;
		g_TxdStore.AddRef(pDecal->m_txdId, REF_OTHER);
		pDecal->m_bRefAddedThisFrame = true;
	}

	// Get the texture
	fwTxd* pTxd = g_TxdStore.Get(pDecal->m_txdId);
	pTexture = pTxd->GetEntry(0);

	// Acknowledge somebody's still using this decal
	pDecal->ResetFramesSinceLastUse();

	return true;
}

//////////////////////////////////////////////////////////////////////////
//
bool PedDecalDecorationManager::FindActiveDecal(const EmblemDescriptor& emblemDesc, PedDecalDecoration*& pDecal)
{
	// Find matching txd slot
	PedDecalDecorationNode* pNode = m_activeList.GetFirst()->GetNext();
	while(pNode != m_activeList.GetLast())
	{
		PedDecalDecoration* pCurEntry = &(pNode->item);
		pNode = pNode->GetNext();

		if (pCurEntry->m_emblemDesc == emblemDesc && (IsInActiveState(pCurEntry) || IsRequested(pCurEntry)))
		{
			pDecal = pCurEntry;
			return true;
		}
	}

	pDecal = NULL;
	return false;

}
