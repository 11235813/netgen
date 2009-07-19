class Ng_Element
{

  class Ng_Points
  {
  public:
    int num;
    const int * ptr;
  
    int Size() const { return num; }
    int operator[] (int i) const { return ptr[i]-1; }
  };


  class Ng_Vertices
  {
  public:
    int num;
    const int * ptr;
  
    int Size() const { return num; }
    int operator[] (int i) const { return ptr[i]-1; }
  };

  class Ng_Edges
  {
  public:
    int num;
    const int * ptr;
  
    int Size() const { return num; }
    int operator[] (int i) const { return abs (ptr[i])-1; }
  };

  class Ng_Faces
  {
  public:
    int num;
    const int * ptr;
  
    int Size() const { return num; }
    int operator[] (int i) const { return (ptr[i]-1) / 8; }
  };

public:
  NG_ELEMENT_TYPE type;
  NG_ELEMENT_TYPE GetType() const { return type; }

  Ng_Points points;      // all points
  Ng_Vertices vertices;
  Ng_Edges edges;
  Ng_Faces faces;
};



template <int DIM> 
DLL_HEADER int Ng_GetNElements ();

template <int DIM> 
DLL_HEADER Ng_Element Ng_GetElement (int nr);

template <int DIM> 
DLL_HEADER int Ng_GetElementIndex (int nr);


/// Curved Elements:
/// xi..... DIM_EL local coordinates
/// sxi ... step xi
/// x ..... DIM_SPACE global coordinates
/// dxdxi...DIM_SPACE x DIM_EL Jacobian matrix (row major storage)
template <int DIM_EL, int DIM_SPACE> 
DLL_HEADER void Ng_MultiElementTransformation (int elnr, int npts,
                                               const double * xi, int sxi,
                                               double * x, int sx,
                                               double * dxdxi, int sdxdxi);
