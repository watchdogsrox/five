#ifndef LINEARTESSELATOR_H
#define LINEARTESSELATOR_H

template<class T> class linearTesselator
{
public:
	typedef Functor1<const T &> pushVtxFunctor;
	typedef Functor3<const T &,const T &,const T &> pushTriFunctor;
	
	void StripTesselate(const T* vertices, float level, typename linearTesselator<T>::pushVtxFunctor pushVtx);
	int StripGetVtxCount	(float level)
	{
		float tesselateLevel = Floorf(level);
		int iTesselate = (int)tesselateLevel;
		
		return iTesselate + (iTesselate) * (iTesselate + 2) - 1;
	}

	void ListTesselate(const T* vertices, float level, typename linearTesselator<T>::pushTriFunctor pushVtx);
	int ListGetVtxCount	(float level)
	{
		float tesselateLevel = Floorf(level);
		int iTesselate = (int)tesselateLevel;

		return iTesselate * iTesselate * 3;
	}

};


template<class T> void linearTesselator<T>::StripTesselate(const T* vertices, float l, typename linearTesselator<T>::pushVtxFunctor pushVtx)
{
	float tesselateLevel = Floorf(l);
	T v01;
	v01 = vertices[1];
	v01.sub(vertices[0]);
	v01.div(tesselateLevel);

	T v02;
	v02 = vertices[2];
	v02.sub(vertices[0]);
	v02.div(tesselateLevel);

	T v21;
	v21 = vertices[1];
	v21.sub(vertices[2]);
	v21.div(tesselateLevel);
	
	T prevVtx = vertices[0];
	T currVtx = vertices[0];
	T deltaA = v02;
	T deltaB = v21;

	pushVtx(prevVtx);
	
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;
	
	{
		for(int jj=0;jj<iLevel;jj++)
		{
			currVtx.add(deltaA);
			pushVtx(currVtx);

			currVtx.add(deltaB);
			pushVtx(currVtx);
		}
		iLevel--;
		
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
		
		T tmp = deltaA;
		deltaA = deltaB;
		deltaB = tmp;
		 
		currVtx.add(deltaA);
	}
	
	for(int ii=1;ii<iTesselate - 1;ii++)
	{
		pushVtx(currVtx);
		pushVtx(currVtx);
		
		for(int jj=0;jj<iLevel;jj++)
		{
			currVtx.add(deltaA);
			pushVtx(currVtx);

			currVtx.add(deltaB);
			pushVtx(currVtx);
		}
		iLevel--;
		
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
		
		T tmp = deltaA;
		deltaA = deltaB;
		deltaB = tmp;
		 
		currVtx.add(deltaA);

	}

	if( iTesselate > 1 )
	{
		pushVtx(currVtx);
		pushVtx(currVtx);

		currVtx.add(deltaA);
		pushVtx(currVtx);

		currVtx.add(deltaB);
		pushVtx(currVtx);
	}
}

template<class T> void linearTesselator<T>::ListTesselate(const T* vertices, float l, typename linearTesselator<T>::pushTriFunctor pushTri)
{
	float tesselateLevel = Floorf(l);
	T v01;
	v01 = vertices[1];
	v01.sub(vertices[0]);
	v01.div(tesselateLevel);

	T v02;
	v02 = vertices[2];
	v02.sub(vertices[0]);
	v02.div(tesselateLevel);

	T v21;
	v21 = vertices[1];
	v21.sub(vertices[2]);
	v21.div(tesselateLevel);
	
	T prevVtx = vertices[0];
	T currVtx = vertices[0];
	T deltaA = v02;
	T deltaB = v21;

	T p1,p2,p3;
	
	int iTesselate = (int)tesselateLevel;
	int iLevel = iTesselate;
	
	p1 = currVtx;

	currVtx.add(deltaA);
	p2 = currVtx;

	currVtx.add(deltaB);
	p3 = currVtx;		
	pushTri(p1,p2,p3);
	
	for(int jj=1;jj<iLevel;jj++)
	{
		currVtx.add(deltaA);
		p1 = p2;
		p2 = p3;
		p3 = currVtx;
		pushTri(p1,p2,p3);
		
		currVtx.add(deltaB);
		p1 = p2;
		p2 = p3;
		p3 = currVtx;
		pushTri(p1,p2,p3);
	}
	iLevel--;
	
	deltaA.mul(-1.0f);
	deltaB.mul(-1.0f);
	
	T tmp = deltaA;
	deltaA = deltaB;
	deltaB = tmp;
	 
	currVtx.add(deltaA);
	
	for(int ii=1;ii<iTesselate - 1;ii++)
	{
		p1 = currVtx;

		currVtx.add(deltaA);
		p2 = currVtx;

		currVtx.add(deltaB);
		p3 = currVtx;
		
		pushTri(p1,p2,p3);
		
		for(int jj=1;jj<iLevel;jj++)
		{
			currVtx.add(deltaA);
			
			p1 = p2;
			p2 = p3;
			p3 = currVtx;
			pushTri(p1,p2,p3);

			currVtx.add(deltaB);
			
			p1 = p2;
			p2 = p3;
			p3 = currVtx;
			pushTri(p1,p2,p3);
		}
		iLevel--;
		
		deltaA.mul(-1.0f);
		deltaB.mul(-1.0f);
		
		T tmp = deltaA;
		deltaA = deltaB;
		deltaB = tmp;
		 
		currVtx.add(deltaA);
	}

	if( iTesselate > 1 )
	{
		p1 = currVtx;

		currVtx.add(deltaA);
		p2 = currVtx;

		currVtx.add(deltaB);
		p3 = currVtx;

		pushTri(p1,p2,p3);
	}
}

#endif // LINEARTESSELATOR_H