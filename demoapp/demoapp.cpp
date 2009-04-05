// ------------------------------------------------------------------
/*!
   \file demoapp.cpp
   \brief Demonstration application for a Netgen Application Program
   \author Joachim Schoeberl
   \date 
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
#include "demoapp.h"


// ****** Local function declarations ******
int DA_ChooseMe (ClientData clientData, Tcl_Interp * interp, int argc, const char *argv[]);
int DA_PrintMesh (ClientData clientData, Tcl_Interp * interp, int argc, const char *argv[]);
int DA_SetSolution (ClientData clientData, Tcl_Interp * interp, int argc, const char *argv[]);

// ****** Start of Code ******

// Initialisation function for Netgen -> Application interface 
// through TCL
int Demoapp_Init (Tcl_Interp * interp)
{
  cout << "Init demoapp" << endl;

  Tcl_CreateCommand (interp, "DA_ChooseMe", DA_ChooseMe,
		     (ClientData)NULL,
		     (Tcl_CmdDeleteProc*) NULL);

  Tcl_CreateCommand (interp, "DA_PrintMesh", DA_PrintMesh,
		     (ClientData)NULL,
		     (Tcl_CmdDeleteProc*) NULL);

  Tcl_CreateCommand (interp, "DA_SetSolution", DA_SetSolution,
		     (ClientData)NULL,
		     (Tcl_CmdDeleteProc*) NULL);

  return TCL_OK;
}



int DA_ChooseMe (ClientData clientData,
		 Tcl_Interp * interp,
		 int argc, const char *argv[])
{
  cout << "Hi" << endl;
  return TCL_OK;
}



int DA_PrintMesh (ClientData clientData,
		  Tcl_Interp * interp,
		  int argc, const char *argv[])
{
  int np = Ng_GetNP();
  int ne = Ng_GetNE();

  cout << "Points: " << np << endl;
  cout << "Tets:   " << ne << endl;

  double point[3];
  int tet[4];
  
  for (int i = 1; i <= np; i++)
    {
      Ng_GetPoint (i, point);
      cout << "Point " << i << ": ";
      cout << point[0] << " " << point[1] << " " 
	   << point[2] << endl;
    }

  for (int i = 1; i <= ne; i++)
    {
      Ng_GetElement (i, tet);
      cout << "Tet " << i << ": ";
      cout << tet[0] << " " << tet[1] << " " 
	   << tet[2] << " " << tet[3] << endl;
    }

  return TCL_OK;
}



int DA_SetSolution (ClientData clientData,
		    Tcl_Interp * interp,
		    int argc, const char *argv[])
{
  int np = Ng_GetNP();
  
  if (!np)
    {
      Tcl_SetResult (interp, (char*)"This operation needs a mesh", TCL_STATIC);
      return TCL_ERROR;
    }

  double point[3];
  
  double * sol = new double[np];

  for (int i = 1; i <= np; i++)
    {
      Ng_GetPoint (i, point);
      sol[i-1] = point[0];
    }

  
  Ng_SolutionData soldata;
  Ng_InitSolutionData (&soldata);

  soldata.data = sol;
  soldata.name = "My Solution";

  Ng_SetSolutionData (&soldata);

  return TCL_OK;
}

