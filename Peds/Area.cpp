// File header
#include "Peds/Area.h"
#include "ai/aichannel.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"

// Game headers
#include "scene/Entity.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CArea::CArea()
:m_vVec1(0.0f, 0.0f, 0.0f),
m_vVec2(0.0f, 0.0f, 0.0f),
m_fWidth(0.0f),
m_fMaxRadius(0.0f),
m_pAttachedEntity(NULL),
m_bActive(false),
m_bOrientateWithEntity(false)
{
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CArea::~CArea()
{
}

//-------------------------------------------------------------------------
// Resets the area
//-------------------------------------------------------------------------
void CArea::Reset()
{
	m_bActive = false;
	m_pAttachedEntity = NULL;
	
	OnReset();
}


//-------------------------------------------------------------------------
// Gets the information about the area
//-------------------------------------------------------------------------
bool CArea::Get( Vector3& vAreaStart, Vector3& vAreaEnd, float& fAreaWidth ) const
{
	if( !m_bActive )
	{
		return false;
	}
	if( m_pAttachedEntity )
	{
		if( m_bOrientateWithEntity )
		{	
			vAreaStart = VEC3V_TO_VECTOR3(m_pAttachedEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vVec1)));
			vAreaEnd = VEC3V_TO_VECTOR3(m_pAttachedEntity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vVec2)));
		}
		else
		{
			vAreaStart = m_vVec1;
			vAreaEnd = m_vVec2;
		}

		const Vector3 vAttachedEntityPosition = VEC3V_TO_VECTOR3(m_pAttachedEntity->GetTransform().GetPosition());
		vAreaStart += vAttachedEntityPosition;
		vAreaEnd += vAttachedEntityPosition;
	}
	else
	{
		vAreaStart	= m_vVec1;
		vAreaEnd	= m_vVec2;
	}
	fAreaWidth = m_fWidth;
	return true;
}


//-------------------------------------------------------------------------
// Gets the setup vectors, without altering them into global coordinates if attached, for debug printing
//-------------------------------------------------------------------------
bool CArea::GetSetupVectors( Vector3& vAreaStart, Vector3& vAreaEnd, float& fAreaWidth ) const
{
	if( !m_bActive )
	{
		return false;
	}

	vAreaStart	= m_vVec1;
	vAreaEnd	= m_vVec2;
	fAreaWidth = m_fWidth;
	return true;
}

//-------------------------------------------------------------------------
// Returns true if the point is within the area, or if there is no area.
//-------------------------------------------------------------------------
bool CArea::IsPointInArea( const Vector3& vPoint, const bool bVisualiseArea, const bool DEBUG_DRAW_ONLY(bIsDead), const bool DEBUG_DRAW_ONLY(bEditing) ) const
{
	bool bReturn = false;
	if( !m_bActive )
	{
		return false;
	}

	if( m_eType == AT_AngledArea )
	{
		Vector3 vAreaStart;
		Vector3 vAreaEnd;
		float fAreaWidth;
		Get(vAreaStart, vAreaEnd, fAreaWidth);

		if(!bVisualiseArea)
		{
			Vector3 vCenter = (vAreaStart + vAreaEnd) * 0.5f;
			float fDistSq = vCenter.Dist2(vPoint);
			if(fDistSq > square(m_fMaxRadius))
			{
				return false;
			}
		}

		const float RadiansBetweenFirstTwoPoints = fwAngle::GetRadianAngleBetweenPoints(vAreaStart.x, vAreaStart.y, vAreaEnd.x, vAreaEnd.y);
		float RadiansBetweenPoints1and4 = RadiansBetweenFirstTwoPoints + (PI / 2);

		while (RadiansBetweenPoints1and4 < 0.0f) {RadiansBetweenPoints1and4 += (2 * PI);}
		while (RadiansBetweenPoints1and4 > (2 * PI)){RadiansBetweenPoints1and4 -= (2 * PI);}

		const Vector2 vBottomLeft( ( ( fAreaWidth * 0.5f ) * rage::Sinf(RadiansBetweenPoints1and4)) + vAreaStart.x, (( fAreaWidth * 0.5f ) * -rage::Cosf(RadiansBetweenPoints1and4)) + vAreaStart.y );
		const Vector2 vBottomRight( ( ( fAreaWidth * 0.5f ) * rage::Sinf(RadiansBetweenPoints1and4)) + vAreaEnd.x, (( fAreaWidth * 0.5f ) * -rage::Cosf(RadiansBetweenPoints1and4)) + vAreaEnd.y );

		const Vector2 vTopLeft( vAreaStart.x, vAreaStart.y );
		const Vector2 vTopRight( vAreaEnd.x, vAreaEnd.y );

		Vector2 vec1To2 = vTopRight - vTopLeft;
		Vector2 vec1To4 = vTopLeft - vBottomLeft;

		const float DistanceFrom1To2 = vec1To2.Mag();
		const float DistanceFrom1To4Test = vec1To4.Mag();


		const Vector2 vec1ToPed((vPoint.x - vTopLeft.x), (vPoint.y - vTopLeft.y));
		vec1To2.Normalize();
		float TestDistance = DotProduct(vec1ToPed, vec1To2);

		//	Check that the point
		if ((TestDistance >= 0.0f)
			&& (TestDistance <= DistanceFrom1To2))
		{
			vec1To4.Normalize();
			TestDistance = DotProduct(vec1ToPed, vec1To4);

			// The width of the box is from teh center, so either infront or behind on this axis is fine
			if ( fabsf(TestDistance) <= DistanceFrom1To4Test )
			{
				if ((vPoint.z >= vAreaStart.z) && (vPoint.z <= vAreaEnd.z))
				{
					bReturn = true;
				}
			}
		}

		if( bVisualiseArea )
		{
#if DEBUG_DRAW

			Color32 col = bIsDead ? Color32( 0.5f, 0.5f, 0.5f ) : Color32( 0.0f, 0.0f, 1.0f );
			if( bEditing )
				col = Color_red;

			// might not have been normalised in an early out
			vec1To4.Normalize();
			Vector3 vDir3d(vec1To4.x, vec1To4.y, 0.0f);
			const Vector3 vVec1B = vAreaStart;
			const Vector3 vVec1T = Vector3( vAreaStart.x, vAreaStart.y, vAreaEnd.z);
			const Vector3 vVec2B = Vector3( vAreaEnd.x, vAreaEnd.y, vAreaStart.z);
			const Vector3 vVec2T = vAreaEnd;

			// bottom rectangle
			grcDebugDraw::Line( vVec1B - ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec1B + ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec2B - ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec2B + ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec1B - ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec2B - ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec1B + ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec2B + ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			// top rectangle
			grcDebugDraw::Line( vVec1T - ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec1T + ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec2T - ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec2T + ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec1T - ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec2T - ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec1T + ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec2T + ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			// vertical links
			grcDebugDraw::Line( vVec1B - ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec1T - ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec2B - ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec2T - ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec2B + ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec2T + ( vDir3d * ( fAreaWidth * 0.5f ) ), col );
			grcDebugDraw::Line( vVec1B + ( vDir3d * ( fAreaWidth * 0.5f ) ), vVec1T + ( vDir3d * ( fAreaWidth * 0.5f ) ), col );

			// Draw a line from the point to the area
			Color32 LineColour;
			LineColour = bReturn ? Color32( 0.0f, 1.0f, 0.0f ) : Color32( 1.0f, 0.0f, 0.0f );

			grcDebugDraw::Line( (vAreaStart+vAreaEnd)*0.5f, vPoint, LineColour );
#endif
		}
	}
	else if(m_eType == AT_Sphere)
	{
		Vector3 vAreaStart;
		Vector3 vAreaEnd;
		float fAreaWidth;
		Get(vAreaStart, vAreaEnd, fAreaWidth);

		// For small spheres allow points up to 2m higher, essentially changing the sphere to a 2m capsule
		// This is to fix Z height offset issues between the coverpoints and the ped.
		if( fAreaWidth < 2.0f )
		{
			Vector3 vDiff = vAreaStart - vPoint;
			if( ABS(vDiff.z) < 2.0f )
			{	
				bReturn = vDiff.XYMag2() < rage::square(fAreaWidth);
			}
		}
		else
		{
			bReturn = vAreaStart.Dist2(vPoint) < rage::square(fAreaWidth);
		}

		if( bVisualiseArea )
		{
#if DEBUG_DRAW
			Color32 col = bIsDead ? Color32( 0.5f, 0.5f, 0.5f ) : Color32( 0.0f, 0.0f, 1.0f );
			if( bEditing )
				col = Color_red;

			grcDebugDraw::Sphere(vAreaStart, m_fWidth, col, false );
#endif
		}
	}
	return bReturn;
}

//-------------------------------------------------------------------------
// Set angled area
//-------------------------------------------------------------------------
void CArea::Set( const Vector3& vAreaStart, const Vector3& vAreaEnd, float fAreaWidth, const CEntity* pAttachedEntity, bool bOrientateWithEntity )
{
	Reset();
	m_pAttachedEntity = pAttachedEntity;
	m_bActive				= true;
	m_bOrientateWithEntity	= bOrientateWithEntity;
	m_vVec1					= vAreaStart;
	m_vVec2					= vAreaEnd;
	m_fWidth				= fAreaWidth;

	// Calculate and store our max radius (non-squared)
	Vector3 vCentre = (vAreaStart + vAreaEnd) * 0.5f;
	m_fMaxRadius = rage::Sqrtf(vAreaStart.Dist2(vCentre) + rage::square(fAreaWidth * 0.5f));

	// make sure the lowest point is always first
	if( m_vVec1.z > m_vVec2.z )
	{
		Vector3 vSwap = m_vVec2;
		m_vVec2 = m_vVec1;
		m_vVec1 = vSwap;
	}
	m_eType = AT_AngledArea;
	
	OnSet();
}

//-------------------------------------------------------------------------
// Set sphere area
//-------------------------------------------------------------------------
void CArea::SetAsSphere( const Vector3& vAreaStart, float fAreaRadius, const CEntity* pAttachedEntity)
{
	Reset();
	m_pAttachedEntity = pAttachedEntity;

	m_bActive				= true;
	m_vVec1					= vAreaStart;
	m_fWidth				= fAreaRadius;
	m_fMaxRadius			= fAreaRadius;
	m_vVec2.Zero();

	m_eType = AT_Sphere;
	
	OnSetSphere();
}

//-------------------------------------------------------------------------
// Returns the areas center and estimated max radius
//-------------------------------------------------------------------------
void CArea::GetCentreAndMaxRadius( Vector3& vCentre, float& fMaxRadiusSq ) const
{
	GetCentre(vCentre);
	fMaxRadiusSq = m_fMaxRadius * m_fMaxRadius;
}

void CArea::GetCentre( Vector3& vCentre ) const
{
	Vector3 vAreaStart;
	Vector3 vAreaEnd;
	float fAreaWidth;
	Get(vAreaStart, vAreaEnd, fAreaWidth);
	// Should this really be aiAssertf?  It's the only assert in the file though so your guess is as good as mine.
	aiAssertf( m_bActive, "Attempting to get the center of a defensive area that is not active" );
	if( m_eType == AT_Sphere )
	{
		vCentre = vAreaStart;
	}
	else
	{
		vCentre = (vAreaStart + vAreaEnd) * 0.5f;
	}
}

void CArea::GetMaxRadius( float& fMaxRadius ) const
{
	fMaxRadius = m_fMaxRadius;
}

