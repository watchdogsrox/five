// Title	:	Relationships.h
// Author	:	Phil Hooker
// Started	:	08/03/06
// Set of classes used to manage the relationships between all peds.
// Replacing the dependancy between pedtype and relationships.

#ifndef _RELATIONSHIPS_H_
#define _RELATIONSHIPS_H_

#include "Debug/Debug.h"
#include "Peds/RelationshipEnums.h"
#include "atl/hashstring.h"
#include "Peds/ped_channel.h"		// pedAssert
#include "Scene/RegdRefTypes.h"

#include "task/System/Tuning.h"

//-------------------------------------------------------------------------
// A class defining the acquaintance information about a relationship group
//-------------------------------------------------------------------------
class CRelationshipGroup : public fwRefAwareBase
{
public:
	FW_REGISTER_CLASS_POOL(CRelationshipGroup);

	CRelationshipGroup(atHashString name, eGroupOwnershipType type);
	~CRelationshipGroup();

	bool				IsValid() const			{ return m_eType != RT_invalid; }
	const atHashString	GetName( void ) const	{ return m_name; }
	eGroupOwnershipType	GetType() const			{ return m_eType; }
	void				SetType(eGroupOwnershipType type) { m_eType = type; }
	
	void		SetRelationship			(eRelationType AcquaintanceType, CRelationshipGroup* pOtherRelationshipGroup);
	inline bool	CheckRelationship		(eRelationType AcquaintanceType, unsigned int otherRelationshipGroupIndex) const;
	void		ClearRelationship		(CRelationshipGroup* pOtherRelationshipGroup);
	void		ClearAllRelationships();
	void		ClearAllRelationshipsOfType	(eRelationType AcquaintanceType);
	bool		HasAnyRelationShipOfType	(eRelationType AcquaintanceType) const;
	inline eRelationType GetRelationship(unsigned int otherRelationshipGroupIndex) const;

	u32			GetIndex() const	{ return m_index; }
	void		SetIndex(u16 val)	{ m_index = val; }

	bool		GetCanBeCleanedUp() const { return m_eType == RT_mission && m_bCanBeCleanedUp; }
	void		SetCanBeCleanedUp(bool bCanBeCleanedUp) { Assert(m_eType == RT_mission); m_bCanBeCleanedUp = bCanBeCleanedUp; }

	bool		GetAffectsWantedLevel() const { return m_bAffectsWantedLevel; }
	void		SetAffectsWantedLevel(bool bAffectsWantedLevel) { m_bAffectsWantedLevel = bAffectsWantedLevel; }

	bool		GetShouldBlipPeds() const { return m_bBlipPeds; }
	void		SetShouldBlipPeds(bool bShouldBlipPeds) { m_bBlipPeds = bShouldBlipPeds; }

private:

	void SetRelationshipByIndex( eRelationType AcquaintanceType, unsigned int index );
	CRelationshipGroup(const CRelationshipGroup&); //due to ref counts copy constructor is invalid
	eGroupOwnershipType		m_eType;
	atHashString			m_name;
	u8						m_relations[MAX_RELATIONSHIP_GROUPS];
	u8						m_relationTypeRef[MAX_NUM_ACQUAINTANCE_TYPES];
	u16						m_index;

	// Used for mission type relationship groups to see if they can be cleaned up post script cleanup
	bool					m_bCanBeCleanedUp:1;

	// Used to determine if a victim should be excluded from causing crimes to be reported
	bool					m_bAffectsWantedLevel:1;

	// Used to tell the mini map to blip all peds in this relationship group
	bool					m_bBlipPeds:1;
};


bool CRelationshipGroup::CheckRelationship(eRelationType AcquaintanceType, unsigned int otherRelationshipGroupIndex) const
{
	pedAssert(otherRelationshipGroupIndex < MAX_RELATIONSHIP_GROUPS);
	return m_relations[otherRelationshipGroupIndex] == (u8)AcquaintanceType;
}


eRelationType CRelationshipGroup::GetRelationship(unsigned int otherRelationshipGroupIndex) const
{
	pedAssert(otherRelationshipGroupIndex < MAX_RELATIONSHIP_GROUPS);
	return (eRelationType) m_relations[otherRelationshipGroupIndex];
}


//-------------------------------------------------------------------------
// The relationship manager responsible for managing all relationship and
// acquaintance information
//-------------------------------------------------------------------------
class CRelationshipManager
{
public:
	// Startup/shutdown system functions
	static void	Init    				( void );
	static void	Shutdown				( void );
	static void	LoadRelationshipData	( void );

	// Update functions
	static void UpdateRelationshipGroups( void );
	static void ResetRelationshipGroups( void );

	// Add/Remove relationship groups
	static CRelationshipGroup*	AddRelationshipGroup(atHashString relgroupName, eGroupOwnershipType type);
	static void					RemoveRelationshipGroup(atHashString relgroupName);
	static void					RemoveRelationshipGroup(CRelationshipGroup* pGroup);

	// Query functions
	//static CRelationshipGroup*	GetRelationshipGroupByIndex	(s16 iRelationshipGroupIndex);
	static CRelationshipGroup*	FindRelationshipGroup(atHashString relgroupName);
	static CRelationshipGroup*	FindRelationshipGroup(u32 relGroupHash);
	static CRelationshipGroup*	FindRelationshipGroupByIndex(s32 iIndex);

#if __DEV
	static void					PrintRelationshipGroups();
#endif

	static RegdRelationshipGroup s_pPlayerGroup;
	static RegdRelationshipGroup s_pCopGroup;
	static RegdRelationshipGroup s_pArmyGroup;
	static RegdRelationshipGroup s_pSecurityGuardGroup;
	static RegdRelationshipGroup s_pCivmaleGroup;
	static RegdRelationshipGroup s_pCivfemaleGroup;
	static RegdRelationshipGroup s_pNoRelationshipGroup;
	static RegdRelationshipGroup s_pAggressiveInvestigateGroup;

	static unsigned int ms_iNextUpdateTime;

	struct Tunables : CTuning
	{
		Tunables();

		bool m_DisplayRemovedGroups;

		PAR_PARSABLE;
	};

private:

	static Tunables sm_Tunables;
};

#endif //_RELATIONSHIPS_H_
