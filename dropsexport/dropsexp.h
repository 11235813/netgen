#ifndef DEMOAPP
#define DEMOAPP

/**************************************************************************/
/* File:   dropsexp.h                                                     */
/* Author: Christoph Lehrenfeld                                           */
/* Date:   October 17, 2010                                               */
/**************************************************************************/

/*!
   \file dropsexp.h
   \brief Header - Drops export format
   \author Christoph Lehrenfeld
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
extern "C" LOCAL_EXPORTS int Dropsexp_Init (Tcl_Interp * interp);


// ****** End of Module ******
#endif // #ifndef DEMOAPP

