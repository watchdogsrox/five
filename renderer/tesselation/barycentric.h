#ifndef BARYCENTRIC_H
#define BARYCENTRIC_H
	
template<class T> class barycentricSpliter
{
public:
	typedef Functor3<const T &,const T &,const T &> pushTriFunctor;
	
	void Split(const T* vertices, typename barycentricSpliter<T>::pushTriFunctor pushVtx);
	int GetVtxCount	() { return 18; }

};

template<class T> void barycentricSpliter<T>::Split(const T* vertices, typename barycentricSpliter<T>::pushTriFunctor pushTri)
{
	T p1 = vertices[0];
	T p2 = vertices[1];
	T p3 = vertices[2];
	
	
	T p4 = p2;
	p4.add(p1);
	p4.div(2.0f);
	
	T p5 = p3;
	p5.add(p2);
	p5.div(2.0f);

	T p6 = p1;
	p6.add(p3);
	p6.div(2.0f);
	
	T b = p1;
	b.add(p2);
	b.add(p3);
	b.div(3.0f);
	
	pushTri(p1,p4,b);
	pushTri(p2,p4,b);
	pushTri(p2,p5,b);

	pushTri(p3,p5,b);
	pushTri(p3,p6,b);
	pushTri(p1,p6,b);
}

#endif // BARYCENTRIC_H