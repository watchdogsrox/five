//
// VehicleModelInfoPlates.h
// Data file for vehicle license plate settings
//
// Rockstar Games (c) 2010

#ifndef INC_VEHICLE_MODELINFO_PLATES_H_
#define INC_VEHICLE_MODELINFO_PLATES_H_

#include "atl/array.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "parser/macros.h"
#include "vector/color32.h"

namespace rage {
	class bkBank;
	class bkCombo;
	class bkGroup;
}

class CVehicleModelInfoPlateTextureSet
{
public:
	CVehicleModelInfoPlateTextureSet();

	inline const rage::atHashString &GetTextureSetName() const		{ return m_TextureSetName; }
	inline const rage::atHashString &GetDiffuseMapStrHash() const	{ return m_DiffuseMapName; }
	inline const rage::atHashString &GetNormalMapStrHash() const	{ return m_NormalMapName; }

	//Various per-background license plate settings
	inline const rage::Vector4 &GetFontExtents() const				{ return m_FontExtents; }
	inline const rage::Vector2 &GetMaxLettersOnPlate() const		{ return m_MaxLettersOnPlate; }

	//Font Settings
	inline bool IsFontOutlineEnabled() const						{ return m_IsFontOutlineEnabled; }
	inline const rage::Color32 &GetFontColor() const				{ return m_FontColor; }
	inline const rage::Color32 &GetFontOutlineColor() const			{ return m_FontOutlineColor; }
	inline const rage::Vector2 &GetFontOutlineMinMaxDepth() const	{ return m_FontOutlineMinMaxDepth; }

	void Reset(const char *diffuseMap, const char *normalMap);

	PAR_SIMPLE_PARSABLE;
private:
	rage::Vector4 m_FontExtents;
	rage::Vector2 m_MaxLettersOnPlate;
	rage::Vector2 m_FontOutlineMinMaxDepth;

	rage::atHashString m_TextureSetName;
	rage::atHashString m_DiffuseMapName;
	rage::atHashString m_NormalMapName;
	rage::Color32 m_FontColor;
	rage::Color32 m_FontOutlineColor;
	bool m_IsFontOutlineEnabled;
};

class CVehicleModelInfoPlates BANK_ONLY(: public rage::datBase)
{
public:
	typedef rage::atArray<CVehicleModelInfoPlateTextureSet> TextureArray;
	
	CVehicleModelInfoPlates();

	//Background textures
	inline int GetDefaultTextureIndex() const			{ return m_DefaultTexureIndex; }
	inline const TextureArray &GetTextureArray() const	{ return m_Textures; }

	//Font Atlas Config
	inline u8 GetFontNumericOffset() const				{ return m_NumericOffset; }
	inline u8 GetFontAlphabeticOffset() const			{ return m_AlphabeticOffset; }
	inline u8 GetFontSpaceOffset() const				{ return m_SpaceOffset; }
	inline u8 GetFontRandomCharOffset() const			{ return m_RandomCharOffset; }
	inline u8 GetFontNumRandomChar() const				{ return m_NumRandomChar; }

#if __BANK
	void AddWidgets(rage::bkBank &bank);
	void ReloadWidgets();

	bool Save(const char *filename);
#endif // __BANK
	bool Load(const char *filename, const char *ext = NULL);	//If ext is NULL, uses default XML extension

	PAR_SIMPLE_PARSABLE;
private:
	void PostLoad();

#if __BANK
	void AddTextureSet();
	void DeleteTextureSet(int index);
#endif // __BANK

private:
	//License Plate Background Texture Sets.
	TextureArray m_Textures;
	int m_DefaultTexureIndex;

	//Parameters for mapping strings to indices into the font atlas texture
	u8 m_NumericOffset;
	u8 m_AlphabeticOffset;
	u8 m_SpaceOffset;
	u8 m_RandomCharOffset;
	u8 m_NumRandomChar;
};

//Classes for Per-Vehicle plate probabilities.
class LicensePlateProbabilityNamed
{
public:
	typedef atHashString key_type;
	typedef u32 value_type;

	LicensePlateProbabilityNamed()	{}
	LicensePlateProbabilityNamed(key_type name, value_type value)
	: m_Name(name)
	, m_Value(value)
	{
	}

	inline const key_type &GetName() const	{ return m_Name; }
	inline const value_type &GetValue() const	{ return m_Value; }

	inline value_type operator +(const value_type &val) const	{ return m_Value + val; }

	PAR_SIMPLE_PARSABLE;
private:
	key_type m_Name;
	value_type m_Value;
};
inline LicensePlateProbabilityNamed::value_type operator+(const LicensePlateProbabilityNamed::value_type& a, const LicensePlateProbabilityNamed& b)	{ return b + a; }

class CVehicleModelPlateProbabilities BANK_ONLY(: public rage::datBase)
{
public:
	typedef rage::atArray<LicensePlateProbabilityNamed> array_type;
	typedef array_type::value_type::value_type value_type;
	typedef array_type::value_type::key_type name_type;
	
	CVehicleModelPlateProbabilities();

	array_type &GetProbabilityArray()					{ return m_Probabilities; }
	const array_type &GetProbabilityArray() const		{ return m_Probabilities; }
	name_type SelectPlateTextureSet() const;

	static name_type SelectPlateTextureSet(const array_type& plateProbs);

#if __BANK
	void AddWidgets(rage::bkBank &bank, const CVehicleModelInfoPlates &plateInfo);
	void ReloadWidgets();
#endif //__BANK

	PAR_SIMPLE_PARSABLE;
private:
	array_type m_Probabilities;

private:
#if __BANK
	void AddSelectedEntry();
	void DeleteEntry(int index);

	atArray<const char *> m_PlateTexSetNames;
	bkGroup *m_Group;
	bkCombo *m_Combo;
	int m_ComboSelection;
#endif //__BANK
};

#endif // INC_VEHICLE_MODELINFO_PLATES_H_
