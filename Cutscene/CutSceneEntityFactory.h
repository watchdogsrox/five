/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneEntityFactory.h
// PURPOSE : A factory for creating entities for a cutscene
// AUTHOR  : Thomas French
// STARTED : 02/07/2008
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENEENTITYFACTORY_H
#define CUTSCENEENTITYFACTORY_H

#include "vector/vector3.h"

//Forward declaration 
class CCutSceneObject;

///////////////////////////
// Cut scene entity factory

//In this form its first pass to get it up and running. For a more frame work friendly function could make a static/singleton class with virtual create functions
//to have the ability to create project specific cut scene entities. See VehicleFactory.h for an idea of a template.

namespace CutsceneCreatureFactory
{
	CCutSceneObject* Create(u32 iModelIndex, const Vector3 &vPos);
}

#endif