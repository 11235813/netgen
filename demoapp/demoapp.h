#ifndef DEMOAPP
#define DEMOAPP

/**************************************************************************/
/* File:   demoapp.h                                                      */
/* Author: Joachim Schoeberl                                              */
/* Date:                                                                  */
/**************************************************************************/

/*!
   \file demoapp.h
   \brief Header - Demonstration application for a Netgen Application Program
   \author Joachim Schoeberl
   \date 
*/

// DLL Import Headers for Netgen functions

#ifdef WIN32
   #define LOCAL_EXPORTS __declspec(dllexport)
#else
   #define LOCAL_EXPORTS 
#endif


// ****** Functions exported by the Application ******
// initialize Tcl commands
// Must always be <appname>_Init(Tcl_Interp*)
extern "C" LOCAL_EXPORTS int Demoapp_Init (Tcl_Interp * interp);


// ****** End of Module ******
#endif // #ifndef DEMOAPP

