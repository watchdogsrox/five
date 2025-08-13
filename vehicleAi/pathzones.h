#ifndef PATHZONES_H
#define PATHZONES_H

// rage includes
#include "atl/array.h"
#include "parser/macros.h"
#include "Vector/Vector2.h"
#include "Vector/vector3.h"

#define MAX_NUM_PATHZONES 4
#define MAX_NUM_PATHZONE_MAPPINGS (MAX_NUM_PATHZONES * (MAX_NUM_PATHZONES - 1))

class CPathZone
{
public:
	Vector2		vMin;
	Vector2		vMax;
	s32			ZoneKey;

	bool IsPointWithinZone(const Vector2& pos) const;

	PAR_SIMPLE_PARSABLE;
};

class CPathZoneMapping
{
public:
	Vector3	vDestination;
	s32		SrcKey;
	s32		DestKey;
	bool	LoadEntireExtents;

	PAR_SIMPLE_PARSABLE;
};

class CPathZoneArray
{
public:
	inline const CPathZone& GetZone(s32 iKey) 
	{ 
		Assert(iKey >= 0);
		if (iKey == m_Entries[iKey].ZoneKey)
		{
			return m_Entries[iKey]; 
		}
		else
		{
			for (int i = 0; i < m_Entries.GetCount(); i++)
			{
				if (m_Entries[i].ZoneKey == iKey)
				{
					return m_Entries[i];
				}
			}
		}

		Assertf(0, "Cannot find CPathZone with key %d, returning junk data", iKey);
		return m_Entries[0];
	}
	atFixedArray<CPathZone, MAX_NUM_PATHZONES> m_Entries;

	s32 GetZoneForPoint(const Vector2& pos) const;

	PAR_SIMPLE_PARSABLE;
};

class CPathZoneMappingArray
{
public:
	//inline CPathZoneMapping& Get(s32 i) { return m_Entries[i]; }
	atFixedArray<CPathZoneMapping, MAX_NUM_PATHZONE_MAPPINGS> m_Entries;

	const CPathZoneMapping& FindMappingBetweenZones(const s32 iSrcKey, const s32 iDestKey) const;

	PAR_SIMPLE_PARSABLE;
};

class CPathZoneData
{
public:
	CPathZoneArray	m_PathZones;
	CPathZoneMappingArray m_PathZoneMappings;

	PAR_SIMPLE_PARSABLE;
};

class CPathZoneManager
{
public:
	//CPathZoneManager();

	static void Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);
	static void ParsePathZoneFile();

	static s32 GetZoneForPoint(const Vector2& pos);
	static const CPathZone& GetPathZone(s32 iKey);
	static const CPathZoneMapping& GetPathZoneMapping(s32 iSrcKey, s32 iDestKey);

	static CPathZoneData m_PathZoneData;
};

#endif //PATHZONES_H