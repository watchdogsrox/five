
#ifndef SAVEGAME_PHOTO_UNIQUE_ID_H
#define SAVEGAME_PHOTO_UNIQUE_ID_H

// Rage headers
#include "math/random.h"

class CSavegamePhotoUniqueId
{
public:
	CSavegamePhotoUniqueId() : m_Value(0), m_bIsMaximised(false) {}

	bool operator == (const CSavegamePhotoUniqueId &other) const { return ( (m_Value == other.m_Value) && (m_bIsMaximised == other.m_bIsMaximised) ); }
	bool operator != (const CSavegamePhotoUniqueId &other) const { return ( (m_Value != other.m_Value) || (m_bIsMaximised != other.m_bIsMaximised) ); }

	void GenerateNewUniqueId();

	void Reset() { m_Value = 0; m_bIsMaximised = false; }

	bool GetIsFree() const { return (m_Value == 0); }
	int GetValue() const { return m_Value; }

	void Set(int nValue, bool bMaximised) { m_Value = nValue; m_bIsMaximised = bMaximised; }

	bool GetIsMaximised() const { return m_bIsMaximised; }
	void SetIsMaximised(bool bMaximised) { m_bIsMaximised = bMaximised; }

	static void Init(unsigned initMode);

private:
	int m_Value;
	bool m_bIsMaximised;

	static mthRandom sm_Random;
};


#endif	//	SAVEGAME_PHOTO_UNIQUE_ID_H


