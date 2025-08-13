/////////////////////////////////////////////////////////////////////////////////
// FILE :    racingline.h
// PURPOSE : 
// AUTHOR :  
// CREATED : 29/03/10
/////////////////////////////////////////////////////////////////////////////////
#ifndef _RACINGLINE_H_
#define _RACINGLINE_H_

#include "vehicleAi/pathfind.h"
#include "vehicleAi/junctions.h"
#include "fwmaths/random.h"
#include "fwsys/timer.h"
#include "scene/physical.h"
#include "scene/RegdRefTypes.h"


template<class T>
class TDequeNode
{
public:
	TDequeNode() : m_pPrev(NULL),m_pNext(NULL){;}
	TDequeNode(const T& val) : m_pPrev(NULL),m_pNext(NULL),m_data(val){;}
	TDequeNode* m_pPrev;
	TDequeNode* m_pNext;
	T m_data;
};

template<class T>
class TDeque
{
public:
	TDeque() : m_size(0), m_pHead(NULL), m_pTail(NULL){;}
	~TDeque(){clear();}
	int size(void) const// Return the length of the sequence.
	{	
		return m_size;
	}
	void clear(void)
	{
		while(m_pTail)
		{
			TDequeNode<T>* pOldNode = m_pTail;
			m_pTail = pOldNode->m_pPrev;
			delete pOldNode;
		}
		m_pHead = NULL;
		m_size = 0;
	}
	void push_back(const T& val)// Insert an element at the end of the sequence.
	{
		TDequeNode<T>* pNewNode = rage_new TDequeNode<T>(val);
		if(m_pTail){m_pTail->m_pNext = pNewNode;}
		pNewNode->m_pPrev = m_pTail;
		m_pTail = pNewNode;
		if(!m_pHead){m_pHead = pNewNode;}
		++m_size;
	}
	void pop_back(void)// Erase an element at the end of the sequence.
	{
		Assert(m_pTail);
		TDequeNode<T>* pOldNode = m_pTail;
		m_pTail = pOldNode->m_pPrev;
		if(m_pTail){m_pTail->m_pNext = NULL;}
		else{m_pHead = NULL;}
		delete pOldNode;
		--m_size;
	}
	const T& get_back(void)// Get (but don't erase) an element at the end of the sequence.
	{
		Assert(m_pTail);
		return m_pTail->m_data;
	}
	void push_front(const T& val)// Insert an element at the beginning of the sequence. 
	{
		TDequeNode<T>* pNewNode = rage_new TDequeNode<T>(val);
		if(m_pHead){m_pHead->m_pPrev = pNewNode;}
		pNewNode->m_pNext = m_pHead;
		m_pHead = pNewNode;
		if(!m_pTail){m_pTail = pNewNode;}
		++m_size;
	}
	void pop_front(void)// Erase an element at the beginning of the sequence. 
	{
		Assert(m_pHead);
		TDequeNode<T>* pOldNode = m_pHead;
		m_pHead = pOldNode->m_pNext;
		if(m_pHead){m_pHead->m_pPrev = NULL;}
		else{m_pTail = NULL;}
		delete pOldNode;
		--m_size;
	}
	const T& get_front(void)// Get (but don't erase) an element at the beginning of the sequence.
	{
		Assert(m_pHead);
		return m_pHead->m_data;
	}
	const TDequeNode<T>* GetNode(int pos) const
	{
		Assert(pos < m_size);
		int count = 0;
		TDequeNode<T>* pNode = m_pHead;
		while(pNode && count < pos)
		{
			pNode = pNode->m_pNext;
			++count;
		}
		return pNode;
	}
	const T& operator[](int pos) const // The array subscript operator( myDeque[3], etc.).
	{
		const TDequeNode<T>* pNode = GetNode(pos);
		return pNode->m_data;
	}
	TDequeNode<T>* GetNode(int pos)
	{
		Assert(pos < m_size);
		int count = 0;
		TDequeNode<T>* pNode = m_pHead;
		while(pNode && count < pos)
		{
			pNode = pNode->m_pNext;
			++count;
		}
		return pNode;
	}
	T& operator[](int pos) // The array subscript operator( myDeque[3], etc.).
	{
		TDequeNode<T>* pNode = GetNode(pos);
		return pNode->m_data;
	}
protected:
	int	m_size;
	TDequeNode<T>* m_pHead;
	TDequeNode<T>* m_pTail;
};

template <class T0, class T1>
struct TPair {
	T0 m_first;
	T1 m_second;

	TPair() {}
	TPair(const T0& a, const T1& b) : m_first(a), m_second(b) {}
	//TPair(const TPair<T0,T1>& rhs) : m_first(rhs.m_first), m_second(rhs.m_second) {}
};

#if __DEV
inline bool Vec2IsValid(const Vector2& val)
{
	return FPIsFinite(val.x) && FPIsFinite(val.y);
}
inline bool Vec3IsValid(const Vector3& val)
{
	return val.FiniteElements();
}
#endif // __DEV

typedef TPair<Vector3, u32> PointWithUpdateStamp;

class RacingLine
{
public:
	RacingLine();
	virtual ~RacingLine();

	//-----------------------------------------------------------------------
	// Main interface.
	virtual void	ClearAll										(void);
	virtual void	ClearStampedData								(u32 updateStamp);
	virtual void	RoughPathPointAdd								(u32 updateStamp, const Vector3& newRoughPathPoint);
	virtual Vector3	RoughPathPointGetLast							(void);
	virtual bool	SmoothedPathInit								(int numSmoothPathSegsPerRoughSeg);
	virtual void	SmoothedPathAdvance								(int numSmoothPathSegsPerRoughSeg);
	virtual void	SmoothedPathUpdate								(void);

	//-----------------------------------------------------------------------
	// Queries.
	virtual Vector3 SmoothedPathGetPointPos							(int pathPointIndex) const;
	virtual void	SmoothedPathGetPointAngleAndRadiusOfCurvature	(int smoothedPathPointIndex, float& angle_out, float& radius_out) const;
	virtual void	SmoothedPathGetPointAngleAndRadiusOfCurvature	(int smoothedPathPointIndex, float smoothedPathSegmentPortion, float& angle_out, float& radius_out) const;
	virtual float	SmoothedPathGetPointVelPortionMaxPropagated		(int smoothedPathPointIndex, float smoothedPathSegmentPortion, float vehVelMax, float vehSideAccMax, float ASSERT_ONLY(vehStartAccMax), float vehStopAccMax);
	virtual Vector3	SmoothedPathGetPosAlong							(int startPathPointIndex, float startPathSegmentPortion, float lookAheadDist) const;
	virtual Vector3	SmoothedPathGetClosestPos						(const Vector3& pos, int& pathPointClosest_out, float& pathSegmentPortion_out) const;

	//-----------------------------------------------------------------------
	// Drawing.
	virtual void Draw(	float DEV_ONLY(drawVertOffset),
		Color32 DEV_ONLY(roughPathPointsColour),
		Color32 DEV_ONLY(smoothPathColour),
		bool DEV_ONLY(drawPathAsHeath),
		float DEV_ONLY(vehVelMax),
		float DEV_ONLY(vehSideAccMax),
		float DEV_ONLY(vehStartAccMax),
		float DEV_ONLY(vehStopAccMax)
		) const;

protected:
#if __DEV
	void	RoughPathPointsDraw				(float drawVertOffset, Color32 roughPathPointsColour) const;
	void	SmoothedPathDraw				(float drawVertOffset, Color32 smoothPathColour) const;
	void	SmoothedPathDrawHeat			(	float drawVertOffset,
												float vehVelMax,
												float vehSideAccMax,
												float UNUSED_PARAM(vehStartAccMax),
												float vehStopAccMax
												) const;
#endif // __DEV

	Vector3	GetClosestPointOnSegmentFlat	(const Vector3& segmentAPos, const Vector3& segmentBPos, const Vector3& pos, float& segmentPortion) const;

	int								m_numSmoothPathSegsPerRoughSeg;
	atArray<PointWithUpdateStamp>	m_roughPathPolyLine;
	atArray<PointWithUpdateStamp>	m_smoothedPathPolyLine;

	atArray<float>					m_maxVelUnclampedAtNodes;
	atArray<float>					m_distToNextNodes;
};


class RacingLineCubic : public RacingLine
{
public:
	RacingLineCubic();
	virtual ~RacingLineCubic();

	//-----------------------------------------------------------------------
	// Main interface.
	virtual bool	SmoothedPathInit			(int numSmoothPathSegsPerRoughSeg);
	virtual void	SmoothedPathAdvance			(int numSmoothPathSegsPerRoughSeg);

protected:
	void			MakeSmoothedPathCubic		(int numSmoothPathSegsPerRoughSeg);
};


class RacingLineCardinal : public RacingLine
{
public:
	RacingLineCardinal();
	virtual ~RacingLineCardinal();

	//-----------------------------------------------------------------------
	// Main interface.
	virtual bool	SmoothedPathInit				(int numSmoothPathSegsPerRoughSeg);
	virtual void	SmoothedPathAdvance				(int numSmoothPathSegsPerRoughSeg);

protected:
	void			MakeSmoothedPathCardinal		(int numSmoothPathSegsPerRoughSeg);
	void			MakeSimpleSmoothedPathCardinal	(int numSmoothPathSegsPerRoughSeg);
};


// The racing line is then approximated by a series of points (masses)
// connected by vectors (springs) which are then simulated to create a more
// optimal path.
class CRacingLineMass
{
public:
	CRacingLineMass();
	void Set(	const u32 updateStamp,
				const float mass,
				const Vector3& pos);

	u32	m_updateStamp;

	Vector3 m_pos;
	Vector2 m_vel;
	Vector2 m_force;
	float	m_mass;
};


// The racing line is then approximated by a series of points (masses)
// connected by vectors (springs) which are then simulated to create a more
// optimal path.
class CRacingLineSpring
{
public:
	CRacingLineSpring();
	void Set(	const u32 updateStamp,
		const float restLength,
		const float stiffness,
		const int massIndex0,
		const int massIndex1);

	u32	m_updateStamp;

	// The masses at the spring's ends.
	int		m_massIndex0;
	int		m_massIndex1;

	// The spring's behavior parameters.
	float	m_restLength;
	float	m_stiffness;
	float	m_dampingFactor;
};


class RacingLineOptimizing : public RacingLine
{
public:
	RacingLineOptimizing();
	virtual ~RacingLineOptimizing();

	//-----------------------------------------------------------------------
	// Main interface.
	virtual void	ClearAll				(void);
	virtual void	ClearStampedData		(u32 updateStamp);
	void			BoundPolyLinesAddPoint	(u32 boundPolyLineId, u32 updateStamp, const Vector3& boundPolyLineNewPoint);
	virtual bool	SmoothedPathInit		(int numSmoothPathSegsPerRoughSeg);
	virtual void	SmoothedPathAdvance		(int numSmoothPathSegsPerRoughSeg);
	virtual void	SmoothedPathUpdate		(void);

	//-----------------------------------------------------------------------
	// Drawing.
	virtual void Draw(	float DEV_ONLY(drawVertOffset),
						Color32 DEV_ONLY(roughPathPointsColour),
						Color32 DEV_ONLY(smoothPathColour),
						bool DEV_ONLY(drawPathAsHeath),
						float DEV_ONLY(vehVelMax),
						float DEV_ONLY(vehSideAccMax),
						float DEV_ONLY(vehStartAccMax),
						float DEV_ONLY(vehStopAccMax)
						) const;

protected:
	void			SmoothedPathMakeFromRacingLine			(void);

	void			RaceLineReset							(void);
	void			RaceLineInit							(int numRaceLineMassesPerRoughSeg);
	virtual void	RaceLineAdvance							(int numRaceLineMassesPerRoughSeg);
	Vector3			RaceLineMassGetInitialPosFromRoughPath	(int massIndex) const;

	void			RaceLineUpdate							(void) ;
	void			UpdateForces							(void);
	void			UpdateVelocities						(void);
	void			UpdatePositions							(void);

	bool			CheckIfSegmentCollidesWithBounds		(const Vector3& oldPos, const Vector3& newPos, Vector3& collisionPosEarliest_out);
	enum IntersectResult { PARALLEL, COINCIDENT, NOT_INTERESECTING, INTERESECTING };
	IntersectResult	Test2DSegmentSegment					(const Vector3& seg0A, const Vector3& seg0B, const Vector3& seg1A, const Vector3& seg1B, float& seg0Portion_out, Vector3& collisionPos_out);

	// The representation of the initial valid path is used to set up
	// the racing line.
	// The racing line is then approximated by a series of points (masses)
	// connected by vectors (springs) which are then simulated to create a more
	// optimal path.
	int								m_numRaceLineMassesPerRoughSeg;
	TDeque<CRacingLineMass>			m_masses;
	TDeque<CRacingLineSpring>		m_springs;

	// The representation of the bounds to smooth within.
	typedef TDeque<PointWithUpdateStamp> BoundPolyLine;
	typedef TPair<BoundPolyLine, u32> BoundPolyLineWithId;
	atArray<BoundPolyLineWithId>	m_boundPolyLines;
};

#endif // _RACINGLINE_H_
