#ifndef ROUTE_H
#define ROUTE_H

//*********************************************************************************
//	Route.h
//	Pool classes for routes which peds may follow.
//	It might be a good idea to change these to use a template class, but it would
//	cause some complications with the pool allocation code.
//
//*********************************************************************************

// Rage headers
#include "diag/tracker.h"		// RAGE_TRACK()
#include "vector/vector3.h"

// Framework Headers
//#include "fwmaths/Maths.h"

// Game headers
#include "vehicleAi/pathFind.h"
#include "debug/debug.h"
#include "Task\Animation/TaskAnims.h"

// Used to store dictionary name and anim name from script commands.
class CAnimNameDescriptor
{
public:

	CAnimNameDescriptor()
	{
		Clear();
	}
	CAnimNameDescriptor(const strStreamingObjectName pAnimDictName, const char* pAnimName)
	{
		m_animDictName = pAnimDictName;	
		strcpy(m_animName,pAnimName);	
	}
	CAnimNameDescriptor(const CAnimNameDescriptor& src)
	{
		From(src);
	}
	~CAnimNameDescriptor()
	{
	}

	CAnimNameDescriptor& operator=(const CAnimNameDescriptor& src)
	{
		From(src);
		return *this;
	}

	bool operator==(const CAnimNameDescriptor& src) const
	{
		return ((0==strcmp(m_animName,src.m_animName)) && 
			(m_animDictName == src.m_animDictName));
	}

	bool operator!=(const CAnimNameDescriptor& src) const
	{
		return (!(*this==src));
	}

	void Clear()
	{
		strcpy(m_animName,"");	
		m_animDictName.Clear();
	}

	bool IsEmpty() const
	{
		return (0==strcmp(m_animName,""));
	}

	const char* GetName() const {return m_animName;} 
	const strStreamingObjectName GetDictName() const {return m_animDictName;} 

private:

	char m_animName[ANIM_NAMELEN];
	strStreamingObjectName m_animDictName;

	void From(const CAnimNameDescriptor& src)
	{
		strcpy(m_animName,src.m_animName);	
		m_animDictName = src.m_animDictName;	
	}
};

//*********************************************************************************
//	CPointRoute
//	A list of points for a ped to follow

class CPointRoute
{
public:
	
    inline CPointRoute()
    : m_iRouteSize(0)
    {
    }
    inline CPointRoute(const CPointRoute& src)
    {   
        From(src);
    }
    inline ~CPointRoute(){}
    
	FW_REGISTER_CLASS_POOL(CPointRoute);	// was 64


    inline CPointRoute& operator=(const CPointRoute& src)
    {
        From(src);
        return *this;
    }
 
	enum 
	{ 
		MAX_NUM_ROUTE_ELEMENTS=8
 	};
   
    inline bool operator==(const CPointRoute& src) const
    {
        if(m_iRouteSize!=src.m_iRouteSize)
        {
            return false;
        }
        
        int i;
        for(i=0;i<m_iRouteSize;i++)
        {
            if(m_routePoints[i]!=src.m_routePoints[i])
            {
                return false;
            }
        }
        return true;
    }
    
    inline bool operator!=(const CPointRoute& src) const
    {
        return (!(*this==src));
    }
    
    inline void From(const CPointRoute& src)
    {
        m_iRouteSize=src.m_iRouteSize;
        int i;
        for(i=0;i<m_iRouteSize;i++)
        {
            m_routePoints[i]=src.m_routePoints[i];
        }
       	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
    }
   
    inline void Clear() 
    {
    	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
        m_iRouteSize=0;
    }
    
    inline bool Add(const Vector3& routeElement) 
    {
       	Assert(m_iRouteSize>=0);
        if(m_iRouteSize<MAX_NUM_ROUTE_ELEMENTS)
        {
            m_routePoints[m_iRouteSize]=routeElement;
            m_iRouteSize++;
            return true;
        }   
        return false;
    }
    
    inline bool Insert(int index, const Vector3& routeElement) 
    {
    	Assert(index >= 0 && index <= m_iRouteSize);
    	
       	if(m_iRouteSize+1>=MAX_NUM_ROUTE_ELEMENTS)
       	{
	       	Assert(m_iRouteSize+1<MAX_NUM_ROUTE_ELEMENTS);
       		return false;
     	}
     	
       	for(int i=m_iRouteSize; i>index; i--)
       	{
       		m_routePoints[i] = m_routePoints[i-1];
      	}
      	m_routePoints[index] = routeElement;
		m_iRouteSize++;

        return true;
    }
        
    inline const Vector3& Get(const int i) const
    {
    	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
    	Assert(i<MAX_NUM_ROUTE_ELEMENTS);
    	Assert(i>=0);
        return m_routePoints[i];
    }
     
	inline void Set(const int i, const Vector3 & vec)
	{
		Assert(m_iRouteSize>=0 && m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(i>=0 && i<m_iRouteSize);
		m_routePoints[i] = vec;
	}

    inline int GetSize() const 
    {
    	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
    	return m_iRouteSize;
    }
    
    inline void SetSize(const int iSize)
    {
    	Assert(iSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(iSize>=0);
        if(iSize>MAX_NUM_ROUTE_ELEMENTS)
        {
            return;
        }
        m_iRouteSize=iSize;
    }
    
    inline void Reverse()
    {
    	int i0=0;
    	int i1=m_iRouteSize-1;
    	while(i0<i1)
    	{
    		Vector3 a=m_routePoints[i0];
    		m_routePoints[i0]=m_routePoints[i1];
    		m_routePoints[i1]=a;
    		i0++;
    		i1--;
    	}
    }
    
    inline void Remove(const int index)
    {
    	Assert(index>=0);
    	Assert(index<MAX_NUM_ROUTE_ELEMENTS);   	
   		   		
   		int i;
   		for(i=index;i<(m_iRouteSize-1);i++)
   		{
	   		m_routePoints[i]=m_routePoints[i+1];
   		}    	
   		m_iRouteSize--;
    }

	inline float GetLength() const
	{
		float fLength = 0.0f;
		int i;
		for(i=1; i<m_iRouteSize; i++)
		{
			fLength += (m_routePoints[i] - m_routePoints[i-1]).Mag();
		}
		return fLength;
	}

	inline void GetMinMax(Vector3 & vMin, Vector3 & vMax)
	{
		Assert(m_iRouteSize > 0);
		vMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
		vMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		int i;
		for(i=0; i<m_iRouteSize; i++)
		{
			if(m_routePoints[i].x < vMin.x) vMin.x = m_routePoints[i].x;
			if(m_routePoints[i].y < vMin.y) vMin.y = m_routePoints[i].y;
			if(m_routePoints[i].z < vMin.z) vMin.z = m_routePoints[i].z;
			if(m_routePoints[i].x > vMax.x) vMax.x = m_routePoints[i].x;
			if(m_routePoints[i].y > vMax.y) vMax.y = m_routePoints[i].y;
			if(m_routePoints[i].z > vMax.z) vMax.z = m_routePoints[i].z;
		}
	}

private:

    int m_iRouteSize;
    Vector3 m_routePoints[MAX_NUM_ROUTE_ELEMENTS];
};


//*********************************************************************************
//	CPatrolRoute
//	Routes which peds will patrol along, including details of what anims they
//	will play as they progress along the route.

class CPatrolRoute
{
public:

#if !__FINAL
	static int ms_iNumRoutes;
#endif
    inline CPatrolRoute()
    : m_iRouteSize(0)
    {
    	int i;
    	for(i=0;i<MAX_NUM_ROUTE_ELEMENTS;i++)
    	{
    		m_anims[i].Clear();
    	}
    }
    inline CPatrolRoute(const CPatrolRoute& src)
    {   
        From(src);
    }
    ~CPatrolRoute(){}
   
	FW_REGISTER_CLASS_POOL(CPatrolRoute);

    inline CPatrolRoute& operator=(const CPatrolRoute& src)
    {
        From(src);
        return *this;
    }
	enum 
	{ 
		MAX_NUM_ROUTE_ELEMENTS=8
 	};
   
    inline bool operator==(const CPatrolRoute& src) const
    {
        if(m_iRouteSize!=src.m_iRouteSize)
        {
            return false;
        }
        
        int i;
        for(i=0;i<m_iRouteSize;i++)
        {
            if(m_routePoints[i]!=src.m_routePoints[i])
            {
                return false;
            }
            if(m_anims[i]!=src.m_anims[i])
            {
            	return false;
            }
        }
        return true;
    }
    
    inline bool operator!=(const CPatrolRoute& src) const
    {
        return (!(*this==src));
    }
    
    inline void From(const CPatrolRoute& src)
    {
        m_iRouteSize=src.m_iRouteSize;
        int i;
        for(i=0;i<m_iRouteSize;i++)
        {
            m_routePoints[i]=src.m_routePoints[i];
            m_anims[i]=src.m_anims[i];
        }
       	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
    }
    
    inline void Clear() 
    {
    	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
        m_iRouteSize=0;
    }
    
    inline bool Add(const Vector3& routeElement, const CAnimNameDescriptor& animNameDescriptor) 
    {
       	Assert(m_iRouteSize>=0);
        if(m_iRouteSize<MAX_NUM_ROUTE_ELEMENTS)
        {
            m_routePoints[m_iRouteSize]=routeElement;
    		m_anims[m_iRouteSize]=animNameDescriptor;
            m_iRouteSize++;
            return true;
        }   
        return false;
    }
    
    inline const Vector3& Get(const int i) const
    {
    	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
    	Assert(i<MAX_NUM_ROUTE_ELEMENTS);
    	Assert(i>=0);
        return m_routePoints[i];
    }

    inline const CAnimNameDescriptor& GetAnim(const int i) const
    {
    	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
    	Assert(i<MAX_NUM_ROUTE_ELEMENTS);
    	Assert(i>=0);
        return m_anims[i];
    }
    
    inline int GetSize() const 
    {
    	Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(m_iRouteSize>=0);
    	return m_iRouteSize;
    }
    
    inline void SetSize(const int iSize)
    {
    	Assert(iSize<=MAX_NUM_ROUTE_ELEMENTS);
    	Assert(iSize>=0);
        if(iSize>MAX_NUM_ROUTE_ELEMENTS)
        {
            return;
        }
        m_iRouteSize=iSize;
    }
    
    inline void Reverse()
    {
    	int i0=0;
    	int i1=m_iRouteSize-1;
    	while(i0<i1)
    	{
    		Vector3 a=m_routePoints[i0];
    		m_routePoints[i0]=m_routePoints[i1];
    		m_routePoints[i1]=a;

			CAnimNameDescriptor b=m_anims[i0];    		
    		m_anims[i0]=m_anims[i1];
    		m_anims[i1]=b;
    		
    		i0++;
    		i1--;
    	}
    }
    
    inline void Remove(const int index)
    {
    	Assert(index>=0);
    	Assert(index<MAX_NUM_ROUTE_ELEMENTS);   	
   		   		
   		int i;
   		for(i=index;i<(m_iRouteSize-1);i++)
   		{
	   		m_routePoints[i]=m_routePoints[i+1];
	   		m_anims[i]=m_anims[i+1];
   		}    	
   		m_iRouteSize--;
    }

	inline float GetLength(void)
	{
		float fLength = 0.0f;
		int i;
		for(i=1; i<m_iRouteSize; i++)
		{
			fLength += (m_routePoints[i] - m_routePoints[i-1]).Mag();
		}
		return fLength;
	}

private:

    int m_iRouteSize;
    CAnimNameDescriptor m_anims[MAX_NUM_ROUTE_ELEMENTS];
    Vector3 m_routePoints[MAX_NUM_ROUTE_ELEMENTS];
};


#endif
