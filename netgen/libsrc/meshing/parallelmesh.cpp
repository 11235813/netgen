#ifdef PARALLEL

#include <meshing.hpp>
#include "paralleltop.hpp"


#ifdef METIS
namespace metis 
{
  extern "C" {
#include <metis.h>
  } 
}
using namespace metis;

#endif


namespace netgen
{

  template <>
  inline MPI_Datatype MyGetMPIType<netgen::PointIndex> ( ) 
  { return MPI_INT; }



  void Mesh :: SendRecvMesh ()
  {
    PrintMessage (1, "Send/Receive mesh");


    {
      // distribute global information
      int nelglob, nvglob;
      if (id == 0)
	{
	  paralleltop -> SetNV (GetNV());
	  paralleltop -> SetNE (GetNE());
	  paralleltop -> SetNSegm (GetNSeg());
	  paralleltop -> SetNSE (GetNSE());
	  
	  nelglob = GetNE();
	  nvglob = GetNV();
	}
      
      MyMPI_Bcast (nelglob);
      MyMPI_Bcast (nvglob);
      
      if (id > 0)
	{
	  paralleltop -> SetNEGlob (nelglob);
	  paralleltop -> SetNVGlob (nvglob);
	}
    }


    {
      // distribute number of local elements
      if (id == 0)
	{
	  Array<int> num_els_on_proc(ntasks);
	  num_els_on_proc = 0;
	  for (ElementIndex ei = 0; ei < GetNE(); ei++)
	    num_els_on_proc[(*this)[ei].GetPartition()]++;
	  
	  MPI_Scatter (&num_els_on_proc[0], 1, MPI_INT,
		       MPI_IN_PLACE, -1, MPI_INT, 0, MPI_COMM_WORLD);
	}
      else
	{
	  int nelloc;
	  MPI_Scatter (NULL, 0, MPI_INT,
		       &nelloc, 1, MPI_INT, 0, MPI_COMM_WORLD);
	  
	  paralleltop -> SetNE (nelloc);
	}
    }





    if (id == 0)
      SendMesh ();
    else
      ReceiveParallelMesh();
  }


  // slaves receive the mesh from the master
  void Mesh :: ReceiveParallelMesh ( )
  {
    int timer = NgProfiler::CreateTimer ("ReceiveParallelMesh");
    NgProfiler::RegionTimer reg(timer);

    int timer_pts = NgProfiler::CreateTimer ("Receive Points");
    int timer_els = NgProfiler::CreateTimer ("Receive Elements");
    int timer_sels = NgProfiler::CreateTimer ("Receive Surface Elements");
    int timer_edges = NgProfiler::CreateTimer ("Receive Edges");


#ifdef SCALASCA
#pragma pomp inst begin(loadmesh)
#endif

    string st;

    bool endmesh = false;

    INDEX_CLOSED_HASHTABLE<int> glob2loc_vert_ht (1);

    // int ve = 0;
    while (!endmesh)
      {
	MyMPI_Recv (st, 0, MPI_TAG_MESH);

	// receive vertices
        if (st == "vertex")
 	  {
	    NgProfiler::RegionTimer reg(timer_pts);

	    Array<int> verts;
	    MyMPI_Recv (verts, 0, MPI_TAG_MESH);

	    int numvert = verts.Size();
	    paralleltop -> SetNV (numvert);

            glob2loc_vert_ht.SetSize (3*numvert+1);
	    
	    for (int vert = 0; vert < numvert; vert++)
	      {
		int globvert = verts[vert];
		paralleltop->SetLoc2Glob_Vert ( vert+1, globvert  );
                glob2loc_vert_ht.Set (globvert, vert+1);
	      }
	    
	    for (int i = 0; i < numvert; i++)
	      AddPoint (netgen::Point<3> (0,0,0));

	    MPI_Datatype mptype = MeshPoint::MyGetMPIType();
	    MPI_Status status;
	    MPI_Recv( &points[1], numvert, mptype, 0, MPI_TAG_MESH, MPI_COMM_WORLD, &status);

	    Array<int> dist_pnums;
	    MyMPI_Recv (dist_pnums, 0, MPI_TAG_MESH);

	    for (int hi = 0; hi < dist_pnums.Size(); hi += 3)
	      paralleltop ->
		SetDistantPNum (dist_pnums[hi+1], dist_pnums[hi], dist_pnums[hi+2]);

	    *testout << "got " << numvert << " vertices" << endl;
 	  }

	if (strcmp (st.c_str(), "volumeelements" ) == 0 )
	  {
	    NgProfiler::RegionTimer reg(timer_els);
	    
	    *testout << "receiving elements" << endl;

	    Element el;

            Array<int> elarray;
            MyMPI_Recv (elarray, 0, MPI_TAG_MESH);

	    for (int ind = 0, elnum = 1; ind < elarray.Size(); elnum++)
	      {
		paralleltop->SetLoc2Glob_VolEl ( elnum,  elarray[ind++]);

		el.SetIndex(elarray[ind++]);
		el.SetNP(elarray[ind++]);

		for ( int j = 0; j < el.GetNP(); j++)
                  el[j] = glob2loc_vert_ht.Get (elarray[ind++]); 
		
		AddVolumeElement (el);
	      }
	  }


	if (strcmp (st.c_str(), "facedescriptor") == 0)
	  {
	    Array<double> doublebuf;
	    MyMPI_Recv( doublebuf, 0, MPI_TAG_MESH );
	    int faceind = AddFaceDescriptor (FaceDescriptor(int(doublebuf[0]), int(doublebuf[1]), int(doublebuf[2]), 0));
	    GetFaceDescriptor(faceind).SetBCProperty (int(doublebuf[3]));
	    GetFaceDescriptor(faceind).domin_singular = doublebuf[4];
	    GetFaceDescriptor(faceind).domout_singular = doublebuf[5];
	  }
       


	if (strcmp (st.c_str(), "surfaceelementsgi") == 0)
	  {
	    NgProfiler::RegionTimer reg(timer_sels);
	    int j;
	    int nep, faceind = 0;
	    int globsel;
	    int * selbuf;
	    selbuf = 0;
	    int bufsize;
	    // receive:
	    // faceindex
	    // nep
	    // tri.pnum
	    // tri.geominfopi.trignum
	    int nlocsel;
	    MyMPI_Recv ( selbuf, bufsize, 0, MPI_TAG_MESH);
	    int ii= 0;
	    int sel = 0;

	    nlocsel = selbuf[ii++];
	    paralleltop -> SetNSE ( nlocsel );

	    while ( ii < bufsize-1 )
	      {
		globsel = selbuf[ii++];
		faceind = selbuf[ii++];
		bool isghost = selbuf[ii++];
		nep = selbuf[ii++];
		Element2d tri(nep);
		tri.SetIndex(faceind);
		for( j=1; j<=nep; j++)
		  {
                    tri.PNum(j) = glob2loc_vert_ht.Get (selbuf[ii++]);

		    // tri.GeomInfoPi(j).trignum = paralleltop->Glob2Loc_SurfEl(selbuf[ii++]);
		    tri.GeomInfoPi(j).trignum = selbuf[ii++];
		    // Frage JS->AS: Brauchst du die trignum irgendwo ?
		    // sie sonst nur bei STL - Geometrien benötigt
		    // die Umrechnung war ein bottleneck !
		  }
		
		tri.SetGhost(isghost);

                paralleltop->SetLoc2Glob_SurfEl ( sel+1, globsel );
                AddSurfaceElement (tri);
		sel ++;
	      }

	    delete [] selbuf ;
	  }

	if (strcmp (st.c_str(), "edgesegmentsgi") == 0)
	  {
	    NgProfiler::RegionTimer reg(timer_edges);
	    int endtime, starttime; 
	    starttime = clock();

	    double * segmbuf = 0;
	    int bufsize;
	    MyMPI_Recv ( segmbuf, bufsize, 0, MPI_TAG_MESH);
	    Segment seg;
	    int globsegi;
	    int ii = 0;
	    int segi = 1;
	    int nsegloc = int ( bufsize / 14 ) ;
	    paralleltop -> SetNSegm ( nsegloc );
	    while ( ii < bufsize )
	      {
		globsegi = int (segmbuf[ii++]);
		seg.si = int (segmbuf[ii++]);

                seg.pnums[0] = glob2loc_vert_ht.Get (int(segmbuf[ii++]));
                seg.pnums[1] = glob2loc_vert_ht.Get (int(segmbuf[ii++]));
		seg.geominfo[0].trignum = int( segmbuf[ii++] );
		seg.geominfo[1].trignum = int ( segmbuf[ii++]);
		seg.surfnr1 = int ( segmbuf[ii++]);
		seg.surfnr2 = int ( segmbuf[ii++]);
		seg.edgenr = int ( segmbuf[ii++]);
		seg.epgeominfo[0].dist = segmbuf[ii++];
		seg.epgeominfo[1].edgenr = int (segmbuf[ii++]);
		seg.epgeominfo[1].dist = segmbuf[ii++];

		seg.singedge_left = segmbuf[ii++];
		seg.singedge_right = segmbuf[ii++];

		seg.epgeominfo[0].edgenr = seg.epgeominfo[1].edgenr;
		
		seg.domin = seg.surfnr1;
		seg.domout = seg.surfnr2;
		if ( seg.pnums[0] >0 && seg.pnums[1] > 0 )
		  {
		    paralleltop-> SetLoc2Glob_Segm ( segi,  globsegi );
		    
		    AddSegment (seg);
		    segi++;
		  }

	      }
	    delete [] segmbuf;
	    endtime = clock();
	    (*testout) << "Receiving Time fde = " << double(endtime - starttime)/CLOCKS_PER_SEC << endl;
	  }
      

	if (strcmp (st.c_str(), "endmesh") == 0)
	  {
	    endmesh = true;
	  }
      }


    int timerloc = NgProfiler::CreateTimer ("Update local mesh");
    int timerloc2 = NgProfiler::CreateTimer ("CalcSurfacesOfNode");

    NgProfiler::RegionTimer regloc(timerloc);
    PrintMessage (2, "Got ", GetNE(), " elements");

    NgProfiler::StartTimer (timerloc2);

    CalcSurfacesOfNode ();

    NgProfiler::StopTimer (timerloc2);

     
    topology -> Update();
    clusters -> Update();
    
    SetNextMajorTimeStamp();
    
    // paralleltop->Print();
    
#ifdef SCALASCA
#pragma pomp inst end(loadmesh)
#endif
  }
  


  
  
  // distribute the mesh to the slave processors
  // call it only for the master !
  void Mesh :: Distribute ()
  {
    if ( id != 0 || ntasks == 1 ) return;
    // metis partition of mesh, only if more than one proc
    
#ifdef SCALASCA
#pragma pomp inst begin(metis)
#endif


    // partition mesh
#ifdef METIS

    ParallelMetis ();
#else
    for (ElementIndex ei = 0; ei < GetNE(); ei++)
      (*this)[ei].SetPartition(ntasks * ei/GetNE() + 1);
#endif


    for (ElementIndex ei = 0; ei < GetNE(); ei++)
      *testout << "el(" << ei << ") is in part " << (*this)[ei].GetPartition() << endl;
    for (SurfaceElementIndex ei = 0; ei < GetNSE(); ei++)
      *testout << "sel(" << int(ei) << ") is in part " << (*this)[ei].GetPartition() << endl;


#ifdef SCALASCA
#pragma pomp inst end(metis)
#endif

    // send partition

    for (int dest = 1; dest < ntasks; dest++)
      MyMPI_Send ("mesh", dest, MPI_TAG_CMD);
    SendRecvMesh (); 


    paralleltop -> UpdateCoarseGrid();

#ifdef SCALASCA
#pragma pomp inst end(loadmesh_seq)
#endif

    // paralleltop -> Print();
  }
  




#ifdef METIS
  void Mesh :: ParallelMetis ( )  
  {
    int timer = NgProfiler::CreateTimer ("Mesh::Partition");
    NgProfiler::RegionTimer reg(timer);

    PrintMessage (3, "Metis called");
      
    if (GetDimension() == 2) 
      {
	PartDualHybridMesh2D ( ); // neloc );
	return;
      }


    int ne = GetNE();
    int nn = GetNP();

    if (ntasks <= 2)
      {
        if (ntasks == 1) return;
        
        for (int i=1; i<=ne; i++)
          VolumeElement(i).SetPartition(1);

        return;
      }


    bool uniform_els = true;
    ELEMENT_TYPE elementtype = TET; // VolumeElement(1).GetType();
    // metis works only for uniform tet/hex meshes
    for ( int el = 2; el <= GetNE(); el++ )
      if ( VolumeElement(el).GetType() != elementtype )
	{
// 	  int nelperproc = ne / (ntasks-1);
// 	  for (int i=1; i<=ne; i++)
// 	    { 
// 	      int partition = i / nelperproc + 1;
// 	      if ( partition >= ntasks ) partition = ntasks-1;
// 	      VolumeElement(i).SetPartition(partition);
// 	    }
	  
	  uniform_els = false;
	  break;
	}

    // uniform_els = false;
    if (!uniform_els)
      {
	PartHybridMesh ( );   // neloc );
	return;
      }



    // uniform (TET) mesh,  JS
    int npe = VolumeElement(1).GetNP();
    
    idxtype *elmnts;
    elmnts = new idxtype[ne*npe];
    
    int etype;
    if (elementtype == TET)
      etype = 2;
    else if (elementtype == HEX)
      etype = 3;
    
    
    for (int i=1; i<=ne; i++)
      for (int j=1; j<=npe; j++)
	elmnts[(i-1)*npe+(j-1)] = VolumeElement(i).PNum(j)-1;
    
    int numflag = 0;
    int nparts = ntasks-1;
    
    int edgecut;
    idxtype *epart, *npart;
    epart = new idxtype[ne];
    npart = new idxtype[nn];
    

//     if ( ntasks == 1 ) 
//       {
// 	(*this) = *mastermesh;
// 	nparts = 4;	   
// 	metis :: METIS_PartMeshDual (&ne, &nn, elmnts, &etype, &numflag, &nparts,
// 				     &edgecut, epart, npart);
// 	cout << "done" << endl;
	
// 	cout << "edge-cut: " << edgecut << ", balance: " << metis :: ComputeElementBalance(ne, nparts, epart) << endl;
	
// 	for (int i=1; i<=ne; i++)
// 	  {
// 	    mastermesh->VolumeElement(i).SetPartition(epart[i-1]);
// 	  }
	
// 	return;
//       }
    

    cout << "call metis ... " << flush;

    int timermetis = NgProfiler::CreateTimer ("Metis itself");
    NgProfiler::StartTimer (timermetis);

    metis :: METIS_PartMeshDual (&ne, &nn, elmnts, &etype, &numflag, &nparts,
				 &edgecut, epart, npart);

    NgProfiler::StopTimer (timermetis);

    cout << "complete" << endl;
    cout << "edge-cut: " << edgecut << ", balance: " << metis :: ComputeElementBalance(ne, nparts, epart) << endl;
    

    // partition numbering by metis : 0 ...  ntasks - 1
    // we want:                       1 ...  ntasks

    for (int i=1; i<=ne; i++)
      VolumeElement(i).SetPartition(epart[i-1] + 1);


    delete [] elmnts; 
    delete [] epart; 
    delete [] npart;
  }
#endif


  void Mesh :: PartHybridMesh () //  Array<int> & neloc ) 
  {
#ifdef METIS
    int ne = GetNE();
    
    int nn = GetNP();
    int nedges = topology->GetNEdges();

    idxtype  *xadj, * adjacency, *v_weights = NULL, *e_weights = NULL;

    int weightflag = 0;
    int numflag = 0;
    int nparts = ntasks - 1;

    int options[5];
    options[0] = 0;
    int edgecut;
    idxtype * part;

    xadj = new idxtype[nn+1];
    part = new idxtype[nn];

    Array<int> cnt(nn+1);
    cnt = 0;

    for ( int edge = 1; edge <= nedges; edge++ )
      {
	int v1, v2;
	topology->GetEdgeVertices ( edge, v1, v2);
	cnt[v1-1] ++;
	cnt[v2-1] ++;
      }

    xadj[0] = 0;
    for ( int n = 1; n <= nn; n++ )
      {
	xadj[n] = idxtype(xadj[n-1] + cnt[n-1]); 
      }

    adjacency = new idxtype[xadj[nn]];
    cnt = 0;

    for ( int edge = 1; edge <= nedges; edge++ )
      {
	int v1, v2;
	topology->GetEdgeVertices ( edge, v1, v2);
	adjacency[ xadj[v1-1] + cnt[v1-1] ] = v2-1;
	adjacency[ xadj[v2-1] + cnt[v2-1] ] = v1-1;
	cnt[v1-1]++;
	cnt[v2-1]++;
      }

    for ( int vert = 0; vert < nn; vert++ )
      {
	FlatArray<int> array ( cnt[vert], &adjacency[ xadj[vert] ] );
	BubbleSort(array);
      }

    metis :: METIS_PartGraphKway ( &nn, xadj, adjacency, v_weights, e_weights, &weightflag, 
			  &numflag, &nparts, options, &edgecut, part );

    Array<int> nodesinpart(ntasks);

    for ( int el = 1; el <= ne; el++ )
      {
	Element & volel = VolumeElement(el);
	nodesinpart = 0;

	//	VolumeElement(el).SetPartition(part[ volel[1] ] + 1);
	
	int el_np = volel.GetNP();
	int partition = 0; 
	for ( int i = 0; i < el_np; i++ )
	  nodesinpart[ part[volel[i]-1]+1 ] ++;

	for ( int i = 1; i < ntasks; i++ )
	  if ( nodesinpart[i] > nodesinpart[partition] ) 
	    partition = i;

	volel.SetPartition(partition);

      }

    /*
    for ( int i=1; i<=ne; i++)
      {
	neloc[ VolumeElement(i).GetPartition() ] ++;
      }
    */

    delete [] xadj;
    delete [] part;
    delete [] adjacency;
#else
    cout << "parthybridmesh not available" << endl;
#endif
  }


  void Mesh :: PartDualHybridMesh ( ) // Array<int> & neloc ) 
  {
#ifdef METIS
    int ne = GetNE();
    
    // int nn = GetNP();
    // int nedges = topology->GetNEdges();
    int nfaces = topology->GetNFaces();

    idxtype  *xadj, * adjacency, *v_weights = NULL, *e_weights = NULL;

    int weightflag = 0;
    int numflag = 0;
    int nparts = ntasks - 1;

    int options[5];
    options[0] = 0;
    int edgecut;
    idxtype * part;

    Array<int, 0> facevolels1(nfaces), facevolels2(nfaces);
    facevolels1 = -1;
    facevolels2 = -1;

    Array<int, 0> elfaces;
    xadj = new idxtype[ne+1];
    part = new idxtype[ne];

    Array<int, 0> cnt(ne+1);
    cnt = 0;

    for ( int el=1; el <= ne; el++ )
      {
	Element volel = VolumeElement(el);
	topology->GetElementFaces(el, elfaces);
	for ( int i = 0; i < elfaces.Size(); i++ )
	  {
	    if ( facevolels1[elfaces[i]-1] == -1 )
	      facevolels1[elfaces[i]-1] = el;
	    else
	      {
		facevolels2[elfaces[i]-1] = el;
		cnt[facevolels1[elfaces[i]-1]-1]++;
		cnt[facevolels2[elfaces[i]-1]-1]++;
	      }
	  }
      }

    xadj[0] = 0;
    for ( int n = 1; n <= ne; n++ )
      {
	xadj[n] = idxtype(xadj[n-1] + cnt[n-1]); 
      }

    adjacency = new idxtype[xadj[ne]];
    cnt = 0;

    for ( int face = 1; face <= nfaces; face++ )
      {
	int e1, e2;
	e1 = facevolels1[face-1];
	e2 = facevolels2[face-1];
	if ( e2 == -1 ) continue;
	adjacency[ xadj[e1-1] + cnt[e1-1] ] = e2-1;
	adjacency[ xadj[e2-1] + cnt[e2-1] ] = e1-1;
	cnt[e1-1]++;
	cnt[e2-1]++;
      }

    for ( int el = 0; el < ne; el++ )
      {
	FlatArray<int> array ( cnt[el], &adjacency[ xadj[el] ] );
	BubbleSort(array);
      }

    int timermetis = NgProfiler::CreateTimer ("Metis itself");
    NgProfiler::StartTimer (timermetis);

    metis :: METIS_PartGraphKway ( &ne, xadj, adjacency, v_weights, e_weights, &weightflag, 
			  &numflag, &nparts, options, &edgecut, part );

    NgProfiler::StopTimer (timermetis);

    Array<int> nodesinpart(ntasks);

    for ( int el = 1; el <= ne; el++ )
      {
	// Element & volel = VolumeElement(el);
	nodesinpart = 0;

	VolumeElement(el).SetPartition(part[el-1 ] + 1);
	
      }

    /*    
    for ( int i=1; i<=ne; i++)
      {
	neloc[ VolumeElement(i).GetPartition() ] ++;
      }
    */

    delete [] xadj;
    delete [] part;
    delete [] adjacency;
#else
    cout << "partdualmesh not available" << endl;
#endif

  }





  void Mesh :: PartDualHybridMesh2D ( ) 
  {
#ifdef METIS
    int ne = GetNSE();
    int nv = GetNV();

    Array<idxtype> xadj(ne+1);
    Array<idxtype> adjacency(ne*4);

    // first, build the vertex 2 element table:
    Array<int, PointIndex::BASE> cnt(nv);
    cnt = 0;
    for (SurfaceElementIndex sei = 0; sei < GetNSE(); sei++)
      for (int j = 0; j < (*this)[sei].GetNP(); j++)
	cnt[ (*this)[sei][j] ] ++;
    
    TABLE<SurfaceElementIndex, PointIndex::BASE> vert2els(cnt);
    for (SurfaceElementIndex sei = 0; sei < GetNSE(); sei++)
      for (int j = 0; j < (*this)[sei].GetNP(); j++)
	vert2els.Add ((*this)[sei][j], sei);
    

    // find all neighbour elements
    int cntnb = 0;
    Array<int> marks(ne);   // to visit each neighbour just once
    marks = -1;
    for (SurfaceElementIndex sei = 0; sei < ne; sei++)
      {
	xadj[sei] = cntnb;
	for (int j = 0; j < (*this)[sei].GetNP(); j++)
	  {
	    PointIndex vnr = (*this)[sei][j];

	    // all elements with at least one common vertex
	    for (int k = 0; k < vert2els[vnr].Size(); k++)   
	      {
		SurfaceElementIndex sei2 = vert2els[vnr][k];
		if (sei == sei2) continue;
		if (marks[sei2] == sei) continue;
		
		// neighbour, if two common vertices
		int common = 0;
		for (int m1 = 0; m1 < (*this)[sei].GetNP(); m1++)
		  for (int m2 = 0; m2 < (*this)[sei2].GetNP(); m2++)
		    if ( (*this)[sei][m1] == (*this)[sei2][m2])
		      common++;
		
		if (common >= 2)
		  {
		    marks[sei2] = sei;     // mark as visited
		    adjacency[cntnb++] = sei2;
		  }
	      }
	  }
      }
    xadj[ne] = cntnb;

    idxtype *v_weights = NULL, *e_weights = NULL;

    int weightflag = 0;
    int numflag = 0;
    int nparts = ntasks - 1;

    int options[5];
    options[0] = 0;
    int edgecut;
    Array<idxtype> part(ne);

    for ( int el = 0; el < ne; el++ )
      BubbleSort (adjacency.Range (xadj[el], xadj[el+1]));
	
    metis :: METIS_PartGraphKway ( &ne, &xadj[0], &adjacency[0], v_weights, e_weights, &weightflag, 
				   &numflag, &nparts, options, &edgecut, &part[0] );

    for (SurfaceElementIndex sei = 0; sei < ne; sei++)
      (*this) [sei].SetPartition (part[sei]+1);
#else
    cout << "partdualmesh not available" << endl;
#endif

  }






  void Mesh :: SendMesh () const   
  {
    const Mesh * mastermesh = this;    // the original plan was different 


    clock_t starttime, endtime;
    starttime = clock();

    MPI_Request sendrequest[ntasks];

    Array<int> num_els_on_proc(ntasks);
    num_els_on_proc = 0;
    for (ElementIndex ei = 0; ei < GetNE(); ei++)
      num_els_on_proc[(*this)[ei].GetPartition()]++;


    // get number of vertices for each processor
    Array<int> nelloc (ntasks);
    
    nelloc = 0;
    // elarraysize = 1;

    PrintMessage ( 3, "Sending vertices");

    TABLE<ElementIndex> els_of_proc (num_els_on_proc);
    for (ElementIndex ei = 0; ei < GetNE(); ei++)
      els_of_proc.Add ( (*this)[ei].GetPartition(), ei);


    Array<int, PointIndex::BASE> vert_flag (GetNV());
    Array<int, PointIndex::BASE> num_procs_on_vert (GetNV());
    Array<int> num_verts_on_proc (ntasks);

    num_verts_on_proc = 0;
    num_procs_on_vert = 0;
    vert_flag = -1;

    for (int dest = 1; dest < ntasks; dest++)
      {
	FlatArray<ElementIndex> els = els_of_proc[dest];

	for (int hi = 0; hi < els.Size(); hi++)
	  {
	    const Element & el = (*this) [ els[hi] ];
	    for (int i = 0; i < el.GetNP(); i++)
	      {
		PointIndex epi = el[i]; 
		if (vert_flag[epi] < dest)
		  {
		    vert_flag[epi] = dest;

		    num_verts_on_proc[dest]++;
		    num_procs_on_vert[epi]++;

		    paralleltop -> SetDistantPNum ( dest, epi, num_verts_on_proc[dest]);
		  }
	      }

	    // elarraysize[dest] += 3 + el.GetNP();
	    nelloc[dest] ++;
	    paralleltop -> SetDistantEl ( dest, els[hi]+1, nelloc[dest] );
	  }
      }

    TABLE<PointIndex> verts_of_proc (num_verts_on_proc);
    TABLE<int, PointIndex::BASE> procs_of_vert (num_procs_on_vert);
    TABLE<int, PointIndex::BASE> loc_num_of_vert (num_procs_on_vert);

    vert_flag = -1;
    for (int dest = 1; dest < ntasks; dest++)
      {
	FlatArray<ElementIndex> els = els_of_proc[dest];
	for (int hi = 0; hi < els.Size(); hi++)
	  {
	    const Element & el = (*this) [ els[hi] ];
	    
	    for (int i = 0; i < el.GetNP(); i++)
	      {
		PointIndex epi = el[i];
		if (vert_flag[epi] < dest)
		  {
		    vert_flag[epi] = dest;
		    procs_of_vert.Add (epi, dest);
		  }
	      }
	  }
      }

    for (int vert = 1; vert <= mastermesh->GetNP(); vert++ )
      {
	FlatArray<int> procs = procs_of_vert[vert];
	for (int j = 0; j < procs.Size(); j++)
	  {
	    int dest = procs[j];
	    verts_of_proc.Add (dest, vert);
	    loc_num_of_vert.Add (vert, verts_of_proc[dest].Size());
	  }
      }

    for (int dest = 1; dest < ntasks; dest++)
      {
	FlatArray<PointIndex> verts = verts_of_proc[dest];

	MyMPI_Send ("vertex", dest, MPI_TAG_MESH);
	MyMPI_ISend (verts, dest, MPI_TAG_MESH);

	MPI_Datatype mptype = MeshPoint::MyGetMPIType();

	int numv = verts.Size();

	MPI_Datatype newtype;
	Array<int> blocklen (numv);
	blocklen = 1;
	
	MPI_Type_indexed (numv, &blocklen[0], 
			  reinterpret_cast<int*> (&verts[0]), 
			  mptype, &newtype);
	MPI_Type_commit (&newtype);

	MPI_Request request;
	MPI_Isend( &points[0], 1, newtype, dest, MPI_TAG_MESH, MPI_COMM_WORLD, &request);
	MPI_Request_free (&request);
      }



    Array<int> num_distpnums(ntasks);
    num_distpnums = 0;

    for (int vert = 1; vert <= mastermesh -> GetNP(); vert++)
      {
	FlatArray<int> procs = procs_of_vert[vert];
	for (int j = 0; j < procs.Size(); j++)
	  num_distpnums[procs[j]] += 3 * (procs.Size()-1);
      }

    TABLE<int> distpnums (num_distpnums);

    for (int vert = 1; vert <= mastermesh -> GetNP(); vert++)
      {
	FlatArray<int> procs = procs_of_vert[vert];
	for (int j = 0; j < procs.Size(); j++)
	  for (int k = 0; k < procs.Size(); k++)
	    if (j != k)
	      {
		distpnums.Add (procs[j], loc_num_of_vert[vert][j]);
		distpnums.Add (procs[j], procs_of_vert[vert][k]);
		distpnums.Add (procs[j], loc_num_of_vert[vert][k]);
	      }
      }
    
    for ( int dest = 1; dest < ntasks; dest ++ )
      {
	MyMPI_ISend ( distpnums[dest], dest, MPI_TAG_MESH, sendrequest[dest] );
	MPI_Request_free (&sendrequest[dest]);
      }


    endtime = clock();
    (*testout) << "Sending Time verts = " << double(endtime - starttime)/CLOCKS_PER_SEC << endl;




    PrintMessage ( 3, "Sending elements" );
    starttime = clock();

    Array<int> elarraysize (ntasks);
    elarraysize = 0;
    for ( int ei = 1; ei <= mastermesh->GetNE(); ei++)
      {
	const Element & el = mastermesh -> VolumeElement (ei);
	int dest = el.GetPartition();
	elarraysize[dest] += 3 + el.GetNP();
      }

    TABLE<int> elementarrays(elarraysize);

    for ( int ei = 1; ei <= mastermesh->GetNE(); ei++)
      {
	const Element & el = mastermesh -> VolumeElement (ei);
	int dest = el.GetPartition();

	elementarrays.Add (dest, ei);
	elementarrays.Add (dest, el.GetIndex());
	elementarrays.Add (dest, el.GetNP());
	for (int i = 0; i < el.GetNP(); i++)
	  elementarrays.Add (dest, el[i]);
      }

    for (int dest = 1; dest < ntasks; dest ++ )
      {
	MyMPI_Send ( "volumeelements", dest, MPI_TAG_MESH);
	MyMPI_ISend ( elementarrays[dest], dest, MPI_TAG_MESH, sendrequest[dest] );
      }


    endtime = clock();
    (*testout) << "Sending Time els = " << double(endtime - starttime)/CLOCKS_PER_SEC << endl;
    starttime = clock();




    PrintMessage ( 3, "Sending Face Descriptors" );
  

    Array<double> double6(6);
    for ( int dest = 1; dest < ntasks; dest++)
      for ( int fdi = 1; fdi <= mastermesh->GetNFD(); fdi++)
        {
	  MyMPI_Send("facedescriptor", dest, MPI_TAG_MESH);

          double6[0] = GetFaceDescriptor(fdi).SurfNr();
          double6[1] = GetFaceDescriptor(fdi).DomainIn();	
          double6[2] = GetFaceDescriptor(fdi).DomainOut();
          double6[3] = GetFaceDescriptor(fdi).BCProperty();
	  double6[4] = GetFaceDescriptor(fdi).domin_singular;
	  double6[5] = GetFaceDescriptor(fdi).domout_singular;

	  MyMPI_Send ( double6, dest, MPI_TAG_MESH);
        }

    endtime = clock();
    (*testout) << "Sending Time fdi = " << double(endtime - starttime)/CLOCKS_PER_SEC << endl;
    starttime = clock();




    PrintMessage ( 3, "Sending Surface elements" );
  
    Array <int> nlocsel(ntasks), bufsize ( ntasks), seli(ntasks);
    for ( int i = 0; i < ntasks; i++)
      {
	nlocsel[i] = 0;
	bufsize[i] = 1;
	seli[i] = 1;
      }

    for ( int sei = 1; sei <= mastermesh -> GetNSE(); sei ++ )
      {
	int ei1, ei2;
	mastermesh -> GetTopology().GetSurface2VolumeElement (sei, ei1, ei2);
	const Element2d & sel = mastermesh -> SurfaceElement (sei);
	int dest;
	// first element

        for (int j = 0; j < 2; j++)
          {
            int ei = (j == 0) ? ei1 : ei2;
            
            if ( ei > 0 && ei <= mastermesh->GetNE() )
              {
                const Element & el = mastermesh -> VolumeElement (ei);
                dest = el.GetPartition();
		nlocsel[dest] ++;
		bufsize[dest] += 4 + 2*sel.GetNP();
              }
	  }
      }
    
    int ** selbuf = 0;
    selbuf = new int*[ntasks];
    for ( int i = 0; i < ntasks; i++)
      if ( bufsize[i] > 0 )
	{*(selbuf+i) = new int[bufsize[i]];}
      else 
	selbuf[i] = 0;
    


    Array<int> nselloc (ntasks);
    nselloc = 0;

    for ( int dest = 1; dest < ntasks; dest++ )
      {
	MyMPI_Send ( "surfaceelementsgi", dest, MPI_TAG_MESH);  
	selbuf[dest][0] = nlocsel[dest];                  
      }    

    for ( int sei = 1; sei <= mastermesh -> GetNSE(); sei ++ )
      {
	int ei1, ei2;
	mastermesh -> GetTopology().GetSurface2VolumeElement (sei, ei1, ei2);
	const Element2d & sel = mastermesh -> SurfaceElement (sei);
	int dest;

	

	int isghost = 0;

        for (int j = 0; j < 2; j++)
          {
            int ei = (j == 0) ? ei1 : ei2;
            
            if ( ei > 0 && ei <= mastermesh->GetNE() )
              {
                const Element & el = mastermesh -> VolumeElement (ei);
                dest = el.GetPartition();
                if (dest > 0)
                  {
                    // send:
                    // sei
                    // faceind
                    // nep
                    // tri.pnums, tri.geominfopi.trignums

		    selbuf[dest][seli[dest]++] = sei;
		    selbuf[dest][seli[dest]++] = sel.GetIndex();
		    selbuf[dest][seli[dest]++] = isghost;
		    selbuf[dest][seli[dest]++] = sel.GetNP();

                    for ( int ii = 1; ii <= sel.GetNP(); ii++)
                      {
                        selbuf[dest][seli[dest]++] = sel.PNum(ii);
                        selbuf[dest][seli[dest]++] = sel.GeomInfoPi(ii).trignum;
                      }
		    nselloc[dest] ++;
		    paralleltop -> SetDistantSurfEl ( dest, sei, nselloc[dest] );
		    isghost = 1;
                  }
		
              }
	  }
      }


    for ( int dest = 1; dest < ntasks; dest++)
      MyMPI_Send( selbuf[dest], bufsize[dest], dest, MPI_TAG_MESH);

    for ( int dest = 0; dest < ntasks; dest++ )
      {
	if (selbuf[dest])
	  delete [] *(selbuf+dest);
      }
    delete [] selbuf;
    
    
    endtime = clock();
    (*testout) << "Sending Time surfels = " << double(endtime - starttime)/CLOCKS_PER_SEC << endl;
    starttime = clock();


    PrintMessage ( 3, "Sending Edge Segments");
    for ( int dest = 1; dest < ntasks; dest++ )
      MyMPI_Send ( "edgesegmentsgi", dest, MPI_TAG_MESH);    


    Array <int> nlocseg(ntasks), segi(ntasks);
    for ( int i = 0; i < ntasks; i++)
      {
	nlocseg[i] = 0;
	bufsize[i] = 0;
	segi[i] = 0;
      }

    for ( int segi = 1; segi <= mastermesh -> GetNSeg(); segi ++ )
      {
	Array<int> volels;
	const MeshTopology & topol = mastermesh -> GetTopology();
	topol . GetSegmentVolumeElements ( segi, volels );
	// const Segment & segm = mastermesh -> LineSegment (segi);
	// int dest;
        for (int j = 0; j < volels.Size(); j++)
          {
            int ei = volels[j];
            int dest;
            if ( ei > 0 && ei <= mastermesh->GetNE() )
              {
                const Element & el = mastermesh -> VolumeElement (ei);
                dest = el.GetPartition();
		nlocseg[dest] ++;
		bufsize[dest] += 14;
              }
	  }

      }


    double ** segmbuf;
    segmbuf = new double*[ntasks];
    for ( int i = 0; i < ntasks; i++)
      if ( bufsize[i] > 0 )
	segmbuf[i] = new double[bufsize[i]];
      else
	segmbuf[i] = 0;
    
    //     cout << "mastermesh " << mastermesh -> GetNSeg() << "    lineseg " << mastermesh -> LineSegment (1) << endl;
    for ( int ls=1; ls <= mastermesh -> GetNSeg(); ls++)
      {
	Array<int> volels;
	mastermesh -> GetTopology().GetSegmentVolumeElements ( ls, volels );
	const Segment & seg = mastermesh -> LineSegment (ls);

        for (int j = 0; j < volels.Size(); j++)
          {
            int ei = volels[j];
            int dest;
            if ( ei > 0 && ei <= mastermesh->GetNE() )
              {
                const Element & el = mastermesh -> VolumeElement (ei);
                dest = el.GetPartition();


		if ( dest > 0 )
		  {
		    segmbuf[dest][segi[dest]++] = ls;
		    segmbuf[dest][segi[dest]++] = seg.si;
		    segmbuf[dest][segi[dest]++] = seg.pnums[0];
		    segmbuf[dest][segi[dest]++] = seg.pnums[1];
		    segmbuf[dest][segi[dest]++] = seg.geominfo[0].trignum;
		    segmbuf[dest][segi[dest]++] = seg.geominfo[1].trignum;
		    segmbuf[dest][segi[dest]++] = seg.surfnr1;
		    segmbuf[dest][segi[dest]++] = seg.surfnr2;
		    segmbuf[dest][segi[dest]++] = seg.edgenr;
		    segmbuf[dest][segi[dest]++] = seg.epgeominfo[0].dist;
		    segmbuf[dest][segi[dest]++] = seg.epgeominfo[1].edgenr;
		    segmbuf[dest][segi[dest]++] = seg.epgeominfo[1].dist;
		    segmbuf[dest][segi[dest]++] = seg.singedge_right;
		    segmbuf[dest][segi[dest]++] = seg.singedge_left;
		  }
		paralleltop -> SetDistantSegm ( dest, ls, int ( segi[dest] / 14 ) );
	      }
	  }
      }

    PrintMessage ( 3, "sending segments" );

    for ( int dest = 1; dest < ntasks; dest++)
      {
	MyMPI_Send( segmbuf[dest], bufsize[dest], dest, MPI_TAG_MESH);
      }
    
    for ( int dest = 0; dest < ntasks; dest++ )
       {
	 if ( segmbuf[dest] )
	   delete [] segmbuf[dest];
       }
     delete [] segmbuf;
    
    PrintMessage ( 3, "segments sent");

    endtime = clock();
    (*testout) << "Sending Time segments = " << double(endtime - starttime)/CLOCKS_PER_SEC << endl;
    starttime = clock();


    for ( int dest = 1; dest < ntasks; dest++ )
      MyMPI_Send("endmesh", dest, MPI_TAG_MESH);


    for ( int dest = 1; dest < ntasks; dest ++ )
      {
        MPI_Status status;
        MPI_Wait (&sendrequest[dest], &status);
      }
  }







  
  void Mesh :: UpdateOverlap()
  {
    cout << "UpdateOverlap depreciated" << endl;

#ifdef OLDOLD
    (*testout) << "UPDATE OVERLAP" << endl;
    Array<int> * globelnums;

#ifdef SCALASCA
#pragma pomp inst begin(updateoverlap)
#endif

    paralleltop->IncreaseOverlap();
    
    if ( id > 0 )
      {
	int nvglob = paralleltop->GetNVGlob ();
	// int nelglob = paralleltop->GetNEGlob();

	// int nv = GetNV();
	// int ned = topology -> GetNEdges();
	// int nfa = topology -> GetNFaces();
	// int nel = GetNE();

	Array<int,PointIndex::BASE> glob2loc_vert(nvglob);
	glob2loc_vert = -1;

	for ( int locv = 1; locv <= GetNV(); locv++)
	  {
	    int globv = paralleltop->GetLoc2Glob_Vert(locv);
	    glob2loc_vert[globv] = locv;
	  }

	BitArray addedpoint ( paralleltop -> GetNVGlob () );
	BitArray addedel ( paralleltop -> GetNEGlob () );
	addedpoint.Clear();
	addedel.Clear();
	
	Array<int> distvert(ntasks), distel(ntasks), nsenddistel(ntasks);

	nsenddistel = 0;
	
	MyMPI_Allgather (GetNV(), distvert.Range(1, ntasks), MPI_HIGHORDER_COMM);
	MyMPI_Allgather (GetNE(), distel.Range(1, ntasks), MPI_HIGHORDER_COMM);

	BitArray appendedpoint ( GetNP() * ntasks );
	appendedpoint.Clear();
	
	TABLE<double> sendpoints(ntasks);
	TABLE<int> sendelements(ntasks);
	TABLE<int> sendsel(ntasks);

	/*
	TABLE<int> cluster_neighbors(nv+ned+nfa+nel);

	for ( int node = 1; node <= nv+ned+nfa+nel; node++ )
	  {
	    int cluster_rep;
	    cluster_rep = clusters->GetVertexRepresentant(node);

	    if ( node == cluster_rep ) continue;

	    Array<int> dests;
	    int nneigh = 0;
	    if ( node - GetNV() <= 0 ) // cluster representant is vertex
	      {
		int vert = node;
		if ( paralleltop -> IsExchangeVert( vert ) )
		  {
		    nneigh = paralleltop -> GetNDistantPNums(vert);
		    dests.SetSize(2*nneigh);
		    paralleltop -> GetDistantPNums ( vert, &dests[0] );
		  }
	      }
	    else if ( node - GetNV() - ned <= 0 ) // cluster representant is edge
	      {
		int edge = node - GetNV();
		if ( paralleltop -> IsExchangeEdge( edge ) )
		  {
		    nneigh = paralleltop -> GetNDistantEdgeNums(edge);
		    dests.SetSize(2*nneigh);
		    paralleltop -> GetDistantEdgeNums ( edge, &dests[0] );
		  }
	      }
	    else if ( node - GetNV() - ned - nfa <= 0 ) // cluster representant is face
	      {
		int face = node - GetNV() - ned;
		if ( paralleltop -> IsExchangeFace( face ) )
		  {
		    nneigh = paralleltop -> GetNDistantFaceNums(face);
		    dests.SetSize(2*nneigh);
		    paralleltop -> GetDistantFaceNums ( face, &dests[0] );
		  }
	      }
	    else // cluster representant is element
	      {
		int el = node - GetNV() - ned - nfa;
		if ( paralleltop -> IsExchangeElement( el ) )
		  {
		    nneigh = paralleltop -> GetNDistantElNums(el);
		    dests.SetSize(2*nneigh);
		    paralleltop -> GetDistantElNums ( el, &dests[0] );
		  }
	      }

	    for ( int j = 1; j < nneigh; j++ )
	      if ( !cluster_neighbors[cluster_rep].Contains ( dests[2*j] ) )
		{
		  cluster_neighbors.Add( cluster_rep-1, dests[2*j] );
		}
	  }
	*/

	for ( int i = 0; i < ntasks; i++ )
	  {
	    sendelements.Add (i, 0);
	    sendsel.Add(i, 0);
	  }

	Array<int> nsentsel (ntasks);
	nsentsel = 0;

	for ( int seli = 1; seli <= GetNSE(); seli++ )
	  {
	    const Element2d & sel = SurfaceElement(seli);
	    int selnp = sel.GetNP();
	    Array<int> vert (selnp);
	    
	    Array<int> alldests (0), dests;
	    
	    bool isparsel = false;
	    for ( int i = 0; i < selnp; i++ )
	      {
		vert[i] = sel.PNum(i+1);
		if ( paralleltop -> IsExchangeVert ( vert[i] ) )
		  {
		    isparsel = true;
		    paralleltop -> GetVertNeighbours ( vert[i], dests );
		    for ( int j = 0; j < dests.Size(); j++ )
		      if ( !alldests.Contains ( dests[j] ) )
			alldests.Append( dests[j] );
		  }
	      }

	    /*
	    int face = topology->GetSurfaceElementFace(seli);
	    int cluster_rep = clusters->GetFaceRepresentant(face);
	    if ( cluster_neighbors[cluster_rep-1].Size() > 0 )
	      {
		isparsel = true;
		for ( int j = 0; j < cluster_neighbors[cluster_rep-1].Size(); j++ )
		  if ( !alldests.Contains ( cluster_neighbors[cluster_rep-1][j] ) )
		    alldests.Append( cluster_neighbors[cluster_rep-1][j] );

	      }
	    */

	    if ( !isparsel ) continue;
	    
	    for ( int i = 0; i < alldests.Size(); i ++ )
	      {
		// send the surface element to all distant procs:
		
		// loc number, 
		// number of points 
		// global vert numbers
		// surface_element_index
		
		int dest = alldests[i];
		
		// ***************** MISSING id = 0
		if ( dest == 0 ) continue;
		
		sendsel.Add (dest, seli);
		sendsel.Add (dest, selnp);

		for ( int ii=0; ii<selnp; ii++)
		  {
		    sendsel.Add (dest, paralleltop -> GetLoc2Glob_Vert (vert[ii]) );
		  }
		sendsel.Add (dest, sel.GetIndex() );
		nsentsel[dest] ++;
	      }
	  }
	for ( int dest = 1; dest < ntasks; dest++ )
	  sendsel[dest][0] = nsentsel[dest];
	
	for ( int eli = 1; eli <= GetNE(); eli++ )
	  {
	    const Element & el = VolumeElement(eli);
	    int elnp = el.GetNP();
	    Array<int> vert (elnp);
	    
	    Array<int> alldests (0), dests;
	    
	    for ( int i = 0; i < elnp; i++ )
	      {
		vert[i] = el.PNum(i+1);
		if ( paralleltop -> IsExchangeVert ( vert[i] ) )
		  {
		    paralleltop -> GetVertNeighbours ( vert[i], dests );
		    for ( int j = 0; j < dests.Size(); j++ )
		      if ( !alldests.Contains ( dests[j] ) )
			{
			  alldests.Append( dests[j] );
			  paralleltop->SetExchangeElement ( dests[j], eli );
			}
		    paralleltop->SetExchangeElement ( eli );
		  }
	      }

	    /*
	    int cluster_rep = clusters->GetElementRepresentant(eli);
	    if ( cluster_neighbors[cluster_rep-1].Size() > 0 )
	      {
		for ( int j = 0; j < cluster_neighbors[cluster_rep-1].Size(); j++ )
		  if ( !alldests.Contains ( cluster_neighbors[cluster_rep-1][j] ) )
		    {
		      alldests.Append( cluster_neighbors[cluster_rep-1][j] );
		      paralleltop->SetExchangeElement ( cluster_neighbors[cluster_rep-1][j], eli );
		    }
		paralleltop->SetExchangeElement ( eli );

	      }
	    */
	  }

	for ( int eli = 1; eli <= GetNE(); eli++ )
	  {
	    const Element & el = VolumeElement(eli);
	    int elnp = el.GetNP();
	    Array<int> vert (elnp);

	    // 	  append to point list:
	    // 	  local pnum
	    // 	  global pnum
	    // 	  point coordinates

	    Array<Point3d> points(elnp);
	    for ( int i = 0; i < elnp; i++ )
	      {
		vert[i] = el.PNum(i+1);
		points[i] = Point(vert[i]);
		Array<int> knowndests;
		// send point to all dests which get the volume element
		for ( int dest = 0; dest < ntasks; dest ++ )
		  {
		    // nur die neuen verts
		    if ( !paralleltop -> IsExchangeElement ( dest, eli )  ) continue;

		    // jeder vertex nur ein mal
		    if ( appendedpoint.Test( (vert[i]-1) * ntasks + dest ) ) continue;
		
		    appendedpoint.Set( (vert[i]-1) * ntasks + dest );
		    paralleltop -> SetExchangeVert (dest,  vert[i]);
		    paralleltop -> SetExchangeVert ( vert[i] );


		    // append vertex to be sent
		    // loc pnum
		    // glob pnum
		    // coords

		    // local pnum
		    sendpoints.Add (dest, vert[i]);

		    // global pnum
		    sendpoints.Add (dest, paralleltop -> GetLoc2Glob_Vert ( vert[i] ) );
		    // coordinates
		    sendpoints.Add (dest, points[i].X() );
		    sendpoints.Add (dest, points[i].Y() );
		    sendpoints.Add (dest, points[i].Z() );
		  }
	      }


	    for ( int dest = 1; dest < ntasks; dest ++ )
	      {
		// send the volume element to all distant procs:
	    
		// loc number, 
		// glob number
		// number of points 
		// glob vertices
		// element_index
		if ( !paralleltop -> IsExchangeElement ( dest, eli )  ) continue;

		// loc number
		sendelements.Add (dest, eli);
		// glob number
		sendelements.Add (dest, paralleltop -> GetLoc2Glob_VolEl(eli));

		sendelements.Add (dest, elnp);

		for ( int j = 0; j < elnp; j++ )
		  sendelements.Add (dest, paralleltop -> GetLoc2Glob_Vert(vert[j]) );
		sendelements.Add (dest, el.GetIndex() );

		distel[dest]++;
		nsenddistel[dest] ++;
		paralleltop -> SetDistantEl ( dest, eli, -1);// distel[dest] );
	      }


	  }
	for ( int dest = 1; dest < ntasks; dest++ )
	  if ( dest != id )
	    sendelements[dest][0] = nsenddistel[dest];
	// find parallel surface elements, if there, append to sendsel - list

	
	distel = 0;    

	// sizes of sendpoints, sendelements, sendsels
	Array<int> sendsize_pts(ntasks), recvsize_pts(ntasks);
	Array<int> sendsize_els(ntasks), recvsize_els(ntasks);
	Array<int> sendsize_sels(ntasks), recvsize_sels(ntasks);

	for (int i = 0; i < ntasks; i++)
	  {
	    sendsize_pts[i] = sendpoints[i].Size();
	    sendsize_els[i] = sendelements[i].Size();
	    sendsize_sels[i] = sendsel[i].Size();
	  }

	MyMPI_Alltoall (sendsize_pts.Range(1, ntasks), recvsize_pts.Range(1, ntasks), MPI_HIGHORDER_COMM);
	MyMPI_Alltoall (sendsize_els.Range(1, ntasks), recvsize_els.Range(1, ntasks), MPI_HIGHORDER_COMM);
	MyMPI_Alltoall (sendsize_sels.Range(1, ntasks), recvsize_sels.Range(1, ntasks), MPI_HIGHORDER_COMM);
	recvsize_pts[0] = 0;
	recvsize_els[0] = 0;
	recvsize_sels[0] = 0;
	
	TABLE<double> recvpoints(recvsize_pts);
	TABLE<int> recvelements(recvsize_els);
	TABLE<int> recvsel(recvsize_sels);

	recvpoints.SetElementSizesToMaxSizes ();
	recvelements.SetElementSizesToMaxSizes ();
	recvsel.SetElementSizesToMaxSizes ();

	/*
	Array<MPI_Request> sendrequest(3*ntasks), recvrequest(3*ntasks);
	Array<MPI_Status> status(3*ntasks);

	for ( int proc = 1; proc < ntasks; proc++)
	  {
	    MyMPI_ISend ( sendpoints[proc], proc, sendrequest[proc] );
 	    MyMPI_IRecv ( recvpoints[proc], proc, recvrequest[proc] );
	  }

	for ( int proc = 1; proc < ntasks; proc++)
	  {
	    MPI_Wait(&sendrequest[proc], &status[proc]);
	    MPI_Wait(&recvrequest[proc], &status[proc]);
	    MyMPI_ISend ( sendelements[proc], proc, sendrequest[proc+1]);
	    MyMPI_IRecv ( recvelements[proc], proc, recvrequest[proc+1]);
	  }
	for ( int proc = 1; proc < ntasks; proc++)
	  {
	    MPI_Wait(&sendrequest[proc+1], &status[proc+1]);
	    MPI_Wait(&recvrequest[proc+1], &status[proc+1]);
	    MyMPI_ISend ( sendsel[proc], proc, sendrequest[proc+2] );
	    MyMPI_IRecv ( recvsel[proc], proc, recvrequest[proc+2] );
	  }
	for ( int proc = 1; proc < ntasks; proc++)
	  {
	    MPI_Wait(&sendrequest[proc+2], &status[proc+2]);
	    MPI_Wait(&recvrequest[proc+2], &status[proc+2]);
	  }
	*/

	Array<MPI_Request> requests;
	
	for ( int proc = 1; proc < ntasks; proc++)
	  {
	    requests.Append (MyMPI_ISend ( sendpoints[proc], proc ));
	    requests.Append (MyMPI_IRecv ( recvpoints[proc], proc ));
	  }

	for ( int proc = 1; proc < ntasks; proc++)
	  {
	    requests.Append (MyMPI_ISend ( sendelements[proc], proc ));
	    requests.Append (MyMPI_IRecv ( recvelements[proc], proc ));
	  }
	for ( int proc = 1; proc < ntasks; proc++)
	  {
	    requests.Append (MyMPI_ISend ( sendsel[proc], proc ));
	    requests.Append (MyMPI_IRecv ( recvsel[proc], proc ));
	  }

	MPI_Status stat;
	for (int i = 0; i < requests.Size(); i++)
	  MPI_Wait (&requests[i], &stat);





	Array<int> * distpnum2parpnum;
	distpnum2parpnum = new Array<int> [2];
	distpnum2parpnum[0].SetSize(0);
	distpnum2parpnum[1].SetSize(0);
 
	Array<int> firstdistpnum (ntasks);


	for ( int sender = 1; sender < ntasks; sender++)
	  {
	    firstdistpnum[sender] = distpnum2parpnum[0].Size(); 

	    if ( sender == id ) continue;

	    int ii = 0; 
	    // receiving points
	    // dist pnum
	    // glob pnum
	    // coords
	    int numrecvpts = int ( recvpoints[sender].Size() / 5 );

	    paralleltop -> SetNV ( GetNV() + numrecvpts );
	    int expectnp = GetNV () + numrecvpts;

	    // received points
	    while ( ii < recvpoints[sender].Size() )
	      {
		int distpnum = int ( (recvpoints[sender])[ii++] );
		int globpnum = int ( (recvpoints[sender])[ii++] );

		Point3d point;
		point.X() = (recvpoints[sender])[ii++];
		point.Y() = (recvpoints[sender])[ii++];
		point.Z() = (recvpoints[sender])[ii++];

		// append point as ghost
		// if not already there
		int pnum=  glob2loc_vert[globpnum];//paralleltop -> Glob2Loc_Vert ( globpnum );
		if ( pnum <= 0 )
		  {
		    pnum = AddPoint ( point, true );
		  }
		paralleltop -> SetDistantPNum ( 0, pnum, globpnum );
		glob2loc_vert[globpnum] = pnum;
		paralleltop -> SetDistantPNum ( sender, pnum, distpnum );
		paralleltop -> SetExchangeVert ( pnum );
	      }

	    ii = 0;

	    int recvnel = (recvelements[sender])[ii++];

	    paralleltop -> SetNE ( recvnel + GetNE() );

	    while ( ii < recvelements[sender].Size() )
	      {
		// receive list:
		// distant number, 
		// glob number
		// number of points 
		// glob vertices
		// element_index

		int distelnum = (recvelements[sender])[ii++];
		int globelnum = (recvelements[sender])[ii++] ;
		int elnp = (recvelements[sender])[ii++] ;
		Array<int> pnums(elnp), globpnums(elnp);

		// append volel
		ELEMENT_TYPE eltype; 
		switch ( elnp )
		  {
		  case 4: eltype = TET; break;
		  case 5: eltype = PYRAMID; break;
		  case 6: eltype = PRISM; break;
		  case 8: eltype = HEX; break;
		  }
	    
		Element el ( eltype ) ;

		for ( int i = 0; i < elnp; i++ )
		  {
		    globpnums[i] = int ( (recvelements[sender])[ii++] );
		    pnums[i] = glob2loc_vert[globpnums[i]]; //paralleltop -> Glob2Loc_Vert(globpnums[i]);
		  }

		el.SetIndex ( (recvelements[sender])[ii++] );
		el.SetGhost ( 1 );
	

		for ( int i = 0; i < elnp; i++)
		  {
		    (int&) el[i] = pnums[i];
		  }

		int eli = AddVolumeElement (el) + 1;

		paralleltop -> SetDistantEl ( sender, eli, distelnum);
		paralleltop -> SetDistantEl ( 0, eli, globelnum );
		paralleltop -> SetExchangeElement ( eli );
	      }

	    ii = 0;
	    int nrecvsendsel = 0;
	    if ( recvsel[sender].Size() > 0 )
	      nrecvsendsel = (recvsel[sender])[ii++];

	    paralleltop -> SetNSE ( nrecvsendsel + GetNSE() );
 
	    while ( ii < recvsel[sender] . Size() )
	      {
		// receive list:
		// distant number, 
		// number of points 
		// global vert numbers
		// surface_element_index

		int distselnum = (recvsel[sender])[ii++];
		int selnp = (recvsel[sender])[ii++] ;

		Array<int> globpnums(selnp);
		Array<int> pnums(selnp);

		// append volel
		ELEMENT_TYPE eltype; 
		switch ( selnp )
		  {
		  case 4: eltype = QUAD; break;
		  case 3: eltype = TRIG; break;
		  }
	    
		Element2d sel ( eltype ) ;
		for ( int i = 0; i < selnp; i++ )
		  {
		    globpnums[i] = int ( (recvsel[sender])[ii++] );
		    pnums[i] =  glob2loc_vert[globpnums[i]];
		  }

		sel.SetIndex ( (recvsel[sender])[ii++] );
		sel.SetGhost ( 1 );

	       
		for ( int i = 0; i < selnp; i++)
		  {
		    (int&) sel[i] = pnums[i];
		  }

		int seli = AddSurfaceElement (sel);

	      }

   
	  }	    

	delete [] distpnum2parpnum;

      }

    globelnums = new Array<int>;
    if ( id == 0 )
      {
	for ( int dest = 1; dest < ntasks; dest++)
	  {
	    MyMPI_Recv ( *globelnums, dest );
	    for ( int i = 0; i < globelnums->Size(); i++ )
	      {
		paralleltop -> SetDistantEl ( dest, (*globelnums)[i],i+1 );
		paralleltop -> SetExchangeElement ( dest, (*globelnums)[i] );
	      }
	  }
      }
    else
      {
	globelnums -> SetSize(GetNE());
	for ( int i = 0; i < GetNE(); i++ )
	  {
	    (*globelnums)[i] = paralleltop -> GetLoc2Glob_VolEl ( i+1 );
	  }
	MyMPI_Send ( *globelnums, 0 );
      }
    
    delete globelnums;
    // send which elements are where

    topology -> Update();

    // edge, facenums have probably changed as elements were added
    // paralleltop has to be updated


    paralleltop -> UpdateExchangeElements();
    

    paralleltop -> UpdateCoarseGridOverlap();
    //paralleltop -> UpdateTopology();
    
//     *testout << "############################################" << endl << endl;
//     paralleltop -> Print();

    clusters -> Update();
    ;
#ifdef SCALASCA
#pragma pomp inst end(updateoverlap)
#endif


#endif

  }








}



#endif
