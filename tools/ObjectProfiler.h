#ifndef __OBJECT_PROFILER_H
#define __OBJECT_PROFILER_H

#if __BANK

#include "atl/string.h"
#include "atl/array.h"
#include "parser/manager.h"
#include "parser/macros.h"

class ProfiledObjectLODData
{
public:
	int	lod;
	float min;
	float max;
	float average;
	float std;
	PAR_SIMPLE_PARSABLE;
};

class ProfiledObjectData
{
public:
	atString	objectName;
	atArray<ProfiledObjectLODData>	objectLODData;
	PAR_SIMPLE_PARSABLE;
};

class ObjectProfilerResults
{
public:

	void Reset()
	{
		objectData.Reset();
	}

	atString	buildVersion;
	atArray<ProfiledObjectData>	objectData;
	PAR_SIMPLE_PARSABLE;
};

class CObjectProfiler
{
public:

	enum
	{
		STATE_SET_TO_DISPLAY = 0,
		STATE_SET_TO_WAIT_FOR_DRAWABLE,
		STATE_SET_TO_PLACE_FOR_LOD,
		STATE_SET_TO_ROTATE,
		STATE_SET_TO_CLEANUP,
		STATE_SET_TO_COMPLETE
	};

	static void		Init();
	static bool		Process();

	static bool		IsActive() { return m_ObjProfileActive; }

	static float	m_CurrentRotAngle;
	static int		m_CurrentObjectIDX;
	static int		m_CurrentObjectLodIDX;
	static int		m_ObjProfileState;
	static bool		m_ObjProfileActive;

	static ObjectProfilerResults m_Results;
};

#endif	//__BANK

#endif	//__OBJECT_PROFILER_H