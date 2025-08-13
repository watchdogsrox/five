// Title	:	RelationshipEnums.h

#ifndef _RELATIONSHIP_ENUMS_H_
#define _RELATIONSHIP_ENUMS_H_

#define MAX_RELATIONSHIP_GROUPS 300
#define INVALID_RELATIONSHIP_GROUP -1

//-------------------------------------------------------------------------
// Relationship types
// This must match the enum list in script/native/commands_ped.sch
// Add new relationship types to the end of the enum list
//-------------------------------------------------------------------------
enum eRelationType
{
	ACQUAINTANCE_TYPE_INVALID = 255,	
	ACQUAINTANCE_TYPE_PED_RESPECT = 0,
	ACQUAINTANCE_TYPE_PED_LIKE,
	ACQUAINTANCE_TYPE_PED_IGNORE,
	ACQUAINTANCE_TYPE_PED_DISLIKE,
	ACQUAINTANCE_TYPE_PED_WANTED,
	ACQUAINTANCE_TYPE_PED_HATE,
	ACQUAINTANCE_TYPE_PED_DEAD,
	MAX_NUM_ACQUAINTANCE_TYPES
};

enum eGroupOwnershipType
{
	RT_mission,
	RT_random,
	RT_invalid = -1
};

#endif //_RELATIONSHIP_ENUMS_H_
