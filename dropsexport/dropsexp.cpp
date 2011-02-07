// ------------------------------------------------------------------
/*!
   \file dropsexp.cpp
   \brief Export netgen meshes into a drops output 
      (based on fluent output of Johannes Gerstmayr)
   \author Christoph Lehrenfeld 
   \date October 17, 2010 
*/
// ------------------------------------------------------------------


#include <iostream>
using namespace std;

// for tcltk ...
#include <tcl.h>

// netgen interface
#include <nginterface.h>

// Include the local header for declaration of locally 
// exported functions
#include "dropsexp.h"


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <mystdlib.h>
#include <myadt.hpp>
#include <linalg.hpp>
#include <csg.hpp>
#include <meshing.hpp>

// #include "writeuser.hpp"




// ****** Local function declarations ******
int DE_ExportMesh (ClientData clientData, Tcl_Interp * interp, int argc, const char *argv[]);
namespace netgen
{
  void WriteDropsFormat (const Mesh & mesh, const string & filename);
  extern  AutoPtr<Mesh> mesh;
}
// ****** Start of Code ******

// Initialisation function for Netgen -> Application interface 
// through TCL
int Dropsexp_Init (Tcl_Interp * interp)
{
  cout << "Init drops-export-module"
#ifdef HAVE_CONFIG_H
       << " - " << VERSION
#endif
       << endl;

  Tcl_CreateCommand (interp, "DE_ExportMesh", DE_ExportMesh,
             (ClientData)NULL,
             (Tcl_CmdDeleteProc*) NULL);

  return TCL_OK;
}



int DE_ExportMesh (ClientData clientData,
          Tcl_Interp * interp,
          int argc, const char *argv[])
{
  
  if (!netgen::mesh.Ptr())
    {
      Tcl_SetResult (interp, (char*) "This operation needs a mesh", TCL_STATIC);
      return TCL_ERROR;
    }
  
  string filename (argv[1]);
  string filetype (argv[2]);  
  cout << "filename: " << filename << endl;
  cout << "filetype:   " << filetype << endl;
  
  WriteDropsFormat(*netgen::mesh,filename);
  
  return TCL_OK;
}


namespace netgen
{


void WriteDropsFormat (const Mesh & mesh,
            const string & filename)

{
  cout << "start writing fluent export" << endl;
      
  const MeshTopology& meshtopo = mesh.GetTopology();
  
  // Update the mesh topology structures
  const_cast<MeshTopology&> (meshtopo).SetBuildEdges(true);
  const_cast<MeshTopology&> (meshtopo).SetBuildFaces(true);
  const_cast<MeshTopology&> (meshtopo).Update();      
      
  int np = mesh.GetNP();
  int ne = mesh.GetNE();
  int nse = mesh.GetNSE();
  int i, j;

  ofstream outfile (filename.c_str());
  char str[100];

  outfile.precision(6);
  //outfile.setf (ios::fixed, ios::floatfield);
  //outfile.setf (ios::showpoint);
      
  outfile << "(0 \"Exported file from NETGEN \")" << endl;
  outfile << "(0 \"Dimension:\")" << endl;
  outfile << "(2 3)" << endl << endl;

  outfile << "(0 \"Nodes:\")" << endl;


  BitArray uncurvedpoint(np);
  BitArray curvedpoint(np);
  uncurvedpoint.Clear();
  curvedpoint.Clear();
  int nup = 0; //number of uncurved points
  for (i = 1; i <= ne; i++){
    Element el = mesh.VolumeElement(i);
    for (int j=0 ; j<4 ;j++){
      if (!uncurvedpoint.Test(el[j])) 
    nup ++;
      uncurvedpoint.Set(el[j]); 
    }
  }
  curvedpoint.Or(uncurvedpoint);
  curvedpoint.Invert();
  //number of nodes:
  sprintf(str,"(10 (0 1 %x 1))",nup); //hexadecimal!!!
  outfile << str << endl;
  //nodes of zone 1:
  sprintf(str,"(10 (7 1 %x 1)(",nup); //hexadecimal!!!
  outfile << str << endl;
  for (i = 1; i <= np; i++)
    {
      if (curvedpoint.Test(i)){
    continue;
      }
      const Point3d & p = mesh.Point(i);

      //outfile.width(10);
      outfile << p.X() << " ";
      outfile << p.Y() << " ";
      outfile << p.Z() << "\n";
    }
  outfile << "))" << endl << endl;

  //write faces with elements

  outfile << "(0 \"Faces:\")" << endl;

  Element2d face, face2;
  int i2, j2;
  Array<INDEX_3> surfaceelp(0);
  Array<int> surfaceeli(0);
  Array<int> surfaceelbc(0);
  Array<int> locels(0);

  //no cells=no tets
  //no faces=2*tets

  int noverbface = 2*ne-nse/2; //inner faces
  int nfa = noverbface+nse;

  sprintf(str,"(13 (0 1 %x 0))",(noverbface+nse)); //hexadecimal!!! // total face range!
  outfile << str << endl;
      
  sprintf(str,"(13 (4 1 %x 2 3)(",noverbface); //hexadecimal!!!
  outfile << str << endl;

  const_cast<Mesh&> (mesh).BuildElementSearchTree();

  BitArray curvedelements(ne);
  curvedelements.Clear();
  int curvedels = 0;
  int between[6][2]=
  {{0,1},
   {0,2},
   {0,3},
   {1,2},
   {1,3},
   {2,3}};

  int maxind = -1;
  for (int i = 0; i < nse; i++)
    { 
      int ind = mesh.GetFaceDescriptor(mesh.SurfaceElement(i+1).GetIndex()).BCProperty() - 1;
      if (ind > maxind) maxind = ind;
    }
  int nbcs = maxind +1;
  Array<Array<int> *> surfaceelbc2gensurfel(nbcs);
  for (int i=0; i<nbcs; i++) surfaceelbc2gensurfel[i] = new Array<int>();
  int gse = 0; //general surface element
  for (i = 1; i <= ne; i++)
    {
      if (ne > 2000)
    {
      if (i%2000 == 0)
        {
          cout << (double)i/(double)ne*100. << "%" << endl;
        }
    } 
      Array<int> facesidx;
      mesh.GetTopology().GetElementFaces(i,facesidx);
      Element el = mesh.VolumeElement(i);
      //if (inverttets)
      //  el.Invert();
      
        
      //outfile << el.GetIndex() << "    ";
      if (el.GetNP() == 10) 
    {
        int matches = 0;
        /*cout << "found a curved tet" << endl*/;
        //test if element is really curved:
        for (int j = 0; j < 6; j++){
          Point3d pm = Center(mesh.Point(el.PNum(between[j][0]+1)),mesh.Point(el.PNum(between[j][1]+1)));
          Vec3d d = pm - mesh.Point(el.PNum(j+4+1));
          if (d.Length() < 1e-12){
            matches++;
          }
        }             
        if (matches != 6){ 
          curvedelements.Set(i);
          curvedels++;
        }
      }
      else if (el.GetNP() != 4) {cout << "only tet-meshes supported in drops-export!" << endl;}
      
      //faces:
      
      Box3d box;
      el.GetBox(mesh.Points(), box);
      box.IncreaseRel(1e-6);

      mesh.GetIntersectingVolEls(box.PMin(),box.PMax(),locels);
      int nel = locels.Size();
      int locind;

      //cout << "nel=" << nel << endl;

      for (j = 1; j <= el.GetNFaces(); j++)
    {
      el.GetFace(j, face);
      face.Invert();
      int eli2 = 0;
      int stopsig = 0;
          
      for (i2 = 1; i2 <= nel; i2++)
        {
          locind = locels.Get(i2);
          Element el2 = mesh.VolumeElement(locind);

          for (j2 = 1; j2 <= el2.GetNFaces(); j2++)
        {
          el2.GetFace(j2, face2);

          bool same = false;
          
          for (int i = 1; i <= 3; i++)
            {
              if (face2[(0+i)%3] == face[0] && 
              face2[(1+i)%3] == face[1] && 
              face2[(2+i)%3] == face[2])
            {
              same = true; break;
            }
            }
          
          if (same)
            {eli2 = locind; stopsig = 1; break;}
        }
          if (stopsig) break;
        }
          
      if (eli2==i) cout << "error in WriteDropsFormat!!!" << endl;
          
      if (eli2 > i) //dont write faces two times!
        {
          //i: left cell, eli: right cell
          outfile << hex << face.PNum(2) << " "
        << hex << face.PNum(1) << " "
        << hex << face.PNum(3) << " "
        << hex << i  << " "
        << hex << eli2 << "\n";
        }
      if (eli2 == 0) 
        {
          surfaceelp.Append(INDEX_3(face.PNum(2),face.PNum(1),face.PNum(3)));
          surfaceeli.Append(i);
        int a = mesh.GetTopology().GetFace2SurfaceElement(facesidx[j-1]);
        int u = mesh.GetFaceDescriptor(mesh.SurfaceElement(a).GetIndex()).BCProperty() - 1;
        surfaceelbc2gensurfel[u]->Append(gse++);
        }
    }
    }
  cout << curvedels << " out of " << ne << " elements are curved " << endl;
  outfile << "))" << endl;

  int acc=1;    
  for (int b=0; b<nbcs; b++){
    int mysize = surfaceelbc2gensurfel[b]->Size();
    sprintf(str,"(13 (%i %x %x 3 3)(",b+5,(noverbface+acc),noverbface+acc+mysize-1); //hexadecimal!!! //boundary faces
    outfile << str << endl;
    for (j = 1; j <= mysize ; j++){
      int i = (*(surfaceelbc2gensurfel[b]))[j-1] + 1;
      outfile << hex << surfaceelp.Get(i).I1() << " "
        << hex << surfaceelp.Get(i).I2() << " "
        << hex << surfaceelp.Get(i).I3() << " "
        << hex << surfaceeli.Get(i) << " " << 0 << "\n";
    }
    outfile << "))" << endl << endl;    
    
    acc += mysize;
  }

  outfile << "(0 \"Cells:\")" << endl;
      
  sprintf(str,"(12 (0 1 %x 0))",ne); //hexadecimal!!!
  outfile << str << endl;

  sprintf(str,"(12 (1 1 %x 1 2))",ne); //hexadecimal!!!
  outfile << str << endl << endl;



// boundary conditions correspond to which number?
  outfile << "(0 \"Zones:\")\n"
      << "(45 (4 interior default-interior)())\n";
  for (int i = 0; i < nbcs; i++)
      outfile << "(45 (" << 5+i << " bc" << i+1 << ")())\n";
  outfile << endl;
    
      //<< "(45 (2 wall wall)())\n"

  cout << "done" << endl;
  
/*  static int betw_tet[6][3] =
      { { 0, 1, 4 },
        { 0, 2, 5 },
        { 0, 3, 6 },
        { 1, 2, 7 },
        { 1, 3, 8 },
        { 2, 3, 9 } };  */
  string tmp = filename + ".2nd";
  ofstream out2ndo(tmp.c_str());
  for (int i = 1; i <= ne; i++)
    {
      if (!curvedelements.Test(i)) continue;
      Element el = mesh.VolumeElement(i);
      out2ndo << el.PNum(1) << "\t" << el.PNum(2)  << "\t" << el.PNum(3) << "\t" << el.PNum(4) << "\t" ;
      for ( int j = 5; j <= 10; j ++){
    const Point3d & p = mesh.Point(el.PNum(j));
    out2ndo << p.X() << "\t";
    out2ndo << p.Y() << "\t";
    out2ndo << p.Z() << "\t";
      }
      out2ndo << endl;
    }
    
  for (int i=0; i<nbcs; i++) delete surfaceelbc2gensurfel[i];
    
}

}