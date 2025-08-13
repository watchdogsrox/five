// 
// ai/spatialarray.h 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AI_SPATIALARRAYNODE_H
#define AI_SPATIALARRAYNODE_H


//------------------------------------------------------------------------------
// CSpatialArrayNode

class CSpatialArrayNode
{
public:
	CSpatialArrayNode()
			: m_Offs(kOffsInvalid)
	{}

	bool IsInserted() const
	{	return m_Offs != kOffsInvalid;	}

protected:
	friend class CSpatialArray;

	static const u16 kOffsInvalid = 0xffff;

	u16				m_Offs;
};

//------------------------------------------------------------------------------

#endif	// AI_SPATIALARRAYNODE_H

/* End of file ai/spatialarraynode.h */
