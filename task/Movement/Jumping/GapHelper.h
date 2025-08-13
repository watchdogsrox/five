#ifndef __GAP_HELPER_H_
#define __GAP_HELPER_H_

#define JUMP_ANALYSIS_MAX_PROBES (20)

class JumpAnalysis
{
public:

	JumpAnalysis();

	class ProbeLevel
	{
	public:
		float	dMinX, dMaxX;		// The min and max distance of the level
		float	averageY;			// The average Y
		bool	bValid;				// Is level valid to land on?
		RegdPhy pPhysical;			// physical associated with level.
	};

	bool	AnalyseTerrain(const CPed* pPed, float maxHorizDistance, float minHorizDistance, int nProbes );
	bool	WasLandingFound() const { return m_LandFound; }
	bool	WasNonAssistedLandingFound() const { return m_bNonAssistedJumpLandFound; }
	bool	WasNonAssistedSlopeFound() const { return m_bNonAssistedJumpSlopeFound; }
	void	ResetLandingFound() { m_LandFound = false; }
	Vector3 GetLandingPosition(bool bAsAttachedPos = false) const;
	const Vector3 &GetLandingPositionForSync() const;
	const Vector3 &GetNonAssistedLandingPosition() const;
	CPhysical* GetLandingPhysical() const { return m_LandData.pPhysical; }
	float GetHorizontalJumpDistance() const;

	void ForceLandData(bool bLandFound, const Vector3 &vLandPosition, CPhysical *pPhysical, float fDistance);

private:

	void GetMatrix(const CPed* pPed, Matrix34& m) const;

	class ProbeStore
	{
	public:

		ProbeStore(): validHit( false ) { }

		float	dx, dy;
		bool	validHit;
		RegdPhy pPhysical;
	};

	bool	DoProbe(const CPed* pPed, Vector3 &start, Vector3 &end, WorldProbe::CShapeTestHitPoint &hitPoint);
	bool	IsProbeAdjacent(ProbeStore &thisProbe, ProbeStore &nextProbe);

	void	CalculateLandingPosition( const CPed *pPed, ProbeLevel *pLevel );

	int		FindGap(int startID, int maxID, ProbeLevel *pLevelData);
	bool	CanWeMakeIt(const CPed *pPed, int levelID, ProbeLevel *pLevelData );

	ProbeLevel	m_LandData;						// The level chosen from the analysis

	Vector3		m_LandPos;						// The position of the landing point on this level
	Vector3		m_LandPosPhysical;				// The position of the landing point (local to physical).
	Vector3		m_vNonAssistedJumpLandPos;		// An approximate land position for non-assisted (default) jump
	Vector3		m_CurDir;						// The direction of the analysis

	bool		m_LandFound;					// Whether a landing point was found
	bool		m_bNonAssistedJumpLandFound;	// If we found any valid non assist jump position.
	bool		m_bNonAssistedJumpSlopeFound;	// Is our non assisted jump position on a slope?
};

extern	JumpAnalysis g_JumpHelper;

#endif	//__GAP_HELPER_H_

