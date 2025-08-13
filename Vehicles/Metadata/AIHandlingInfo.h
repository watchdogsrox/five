
#ifndef AIHANDLINGINFO_H
#define AIHANDLINGINFO_H

#include "atl/array.h"
#include "atl/hashstring.h"
#include "math/amath.h"

// Rage includes
#include "fwtl/pool.h"

        
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class AIHandlingInfo
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

class CAICurvePoint 
{
public:
	CAICurvePoint();                 // Default constructor
	virtual ~CAICurvePoint();        // Virtual destructor

	// PURPOSE: Maximum number of infos allowed in the memory pool
	static const u32 ms_iMaxAICurvePoints = 36;

	FW_REGISTER_CLASS_POOL(CAICurvePoint);

	inline float GetAngle()			const	{ return m_Angle; }
	inline float GetAngleRadians()	const	{ return DtoR*m_Angle; }
	inline float GetSpeed()			const	{ return m_Speed; }

private:
	float                       m_Angle;
	float                       m_Speed;

	PAR_SIMPLE_PARSABLE
};
    
class CAIHandlingInfo 
{
public:
    CAIHandlingInfo();                 // Default constructor
    virtual ~CAIHandlingInfo();        // Virtual destructor

	// PURPOSE: Maximum number of infos allowed in the memory pool
	static const u32 ms_iMaxAIHandlingInfos = 4;

	// PURPOSE: Pool declaration
	FW_REGISTER_CLASS_POOL(CAIHandlingInfo);

	// PURPOSE: Get the name of this combat info
	atHashWithStringNotFinal GetName() const { return m_Name; }

	// Handling variables.
	float GetMinBrakeDistance()		const	{ return m_MinBrakeDistance; }
	float GetMaxBrakeDistance()		const	{ return m_MaxBrakeDistance; }
	float GetMaxSpeedAtBrakeDistance()	const	{ return m_MaxSpeedAtBrakeDistance; }
	float GetAbsoluteMinSpeed()		const	{ return m_AbsoluteMinSpeed; }

	float GetDistToStopAtCurrentSpeed(const float fCurrentSpeed) const
	{
		const float fBaseBrakeDist = GetMaxBrakeDistance() - GetMinBrakeDistance();
		const float fRatio = GetMaxSpeedAtBrakeDistance() > 0.0f 
			? fCurrentSpeed / GetMaxSpeedAtBrakeDistance()
			: 0.0f;

		return fRatio * fBaseBrakeDist;
	}

	const CAICurvePoint*	GetCurvePoint(s32 i)	const { return m_AICurvePoints[i]; }
	s32						GetCurvePointCount()	const { return m_AICurvePoints.GetCount(); }

private:
    
    ::rage::atHashString        m_Name;
    float                       m_MinBrakeDistance;
	float                       m_MaxBrakeDistance;
	float						m_MaxSpeedAtBrakeDistance;
    float                       m_AbsoluteMinSpeed;

	// PURPOSE: The array of AI handling infos defined in data
	atArray<CAICurvePoint*>		m_AICurvePoints;

	PAR_SIMPLE_PARSABLE
};

        
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CAIHandlingInfoMgr
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

    
class CAIHandlingInfoMgr 
{
public:
    CAIHandlingInfoMgr();                 // Default constructor
    virtual ~CAIHandlingInfoMgr();        // Virtual destructor

	// PURPOSE: Initialise AI handling info pool and load file
	static void Init(unsigned initMode);

	// PURPOSE: Clear AI handling infos and shutdown pool
	static void Shutdown(unsigned shutdownMode);

	// PURPOSE: Clear AI handling infos and shutdown pool
	static void Reset();

	// PURPOSE: Get a AI handling info by hash
	static const CAIHandlingInfo* GetAIHandlingInfoByHash(u32 uHash);

#if __BANK
	// PURPOSE: Clear AI handling infos and shutdown pool
	static void Save();

	// PURPOSE: Add in debug rag widgets
	static void AddWidgets(bkBank& bank);
#endif // __BANK
	// PURPOSE: Clear AI handling infos and shutdown pool
	static void Load();

private:

	// PURPOSE: Static instance of metadata 
	static CAIHandlingInfoMgr ms_Instance;
    
	// PURPOSE: The array of AI handling infos defined in data
	atArray<CAIHandlingInfo*> m_AIHandlingInfos;
    
	PAR_SIMPLE_PARSABLE
};

#endif //AIHANDLINGINFO_H
