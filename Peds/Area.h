#ifndef AREA_H
#define AREA_H

// Framework headers
#include "data/base.h"
#include "fwmaths/Vector.h"

// Game includes
#include "scene/Entity.h" // TEMPORARILY for the asserts used in the regdptr macros
#include "scene/RegdRefTypes.h"


class CEntity;

//-------------------------------------------------------------------------
// Defines a general area
//-------------------------------------------------------------------------
class CArea: public datBase	// Declaring a virtual in a base class causes bizarre code generation issues on x64?
{

public:

	CArea();
	virtual ~CArea();

	enum AreaType
	{
		AT_AngledArea = 0,
		AT_Sphere
	};

	//Returns true if the area is active
	bool IsActive( void ) const { return m_bActive; }
	// Gets the information about the area
	bool Get( Vector3& vAreaStart, Vector3& vAreaEnd, float& fAreaWidth ) const;
	// Gets the setup vectors, without altering them into global coordinates if attached, for debug printing
	bool GetSetupVectors( Vector3& vAreaStart, Vector3& vAreaEnd, float& fAreaWidth ) const;
	// Returns the attached entity, NULL if not attached
	const CEntity* GetAttachedEntity() {return m_pAttachedEntity;}
	// Returns true if the point is within the area, or if there is no area.
	bool IsPointInArea( const Vector3& vPoint, const bool bVisualiseArea = false, const bool bIsDead = false, const bool bEditing = false ) const;
	// Removes the area
	void Reset( void );
	// Set angled area
	void Set( const Vector3& vAreaStart, const Vector3& vAreaEnd, float fAreaWidth, const CEntity* pAttachedEntity = NULL, bool bOrientateWithEntity = false );
	// Set sphere area
	void SetAsSphere( const Vector3& vAreaStart, float fAreaRadius, const CEntity* pAttachedEntity = NULL );
	// Gets the maximum radius and center of the area
	void GetCentreAndMaxRadius( Vector3& vCentre, float& fMaxRadiusSq ) const;
	void GetCentre( Vector3& vCentre ) const;
	void GetMaxRadius( float& fMaxRadius ) const;
	// Returns true if this area will rotate with the entity
	bool WillOrientateWithEntity( void ) const { return m_bOrientateWithEntity; }
	// Gets/Sets the width
	float GetWidth() const { return m_fWidth; }
	void SetWidth(float val) { m_fWidth = val; }

	u32 GetType() {return m_eType;}

	Vector3 GetVec1() { return m_vVec1; }
	Vector3 GetVec2() { return m_vVec2; }

protected:

	virtual void OnReset() {}
	virtual void OnSet() {}
	virtual void OnSetSphere() {}
	
private:

	Vector3			m_vVec1;
	Vector3			m_vVec2;
	float			m_fWidth;
	float			m_fMaxRadius;
	RegdConstEnt	m_pAttachedEntity;
	unsigned int	m_bActive : 1;
	unsigned int	m_bOrientateWithEntity :1;
	unsigned int	m_eType : 1;
	
};

#endif //AREA_H
