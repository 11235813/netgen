#ifndef FILE_VISUAL
#define FILE_VISUAL

/* *************************************************************************/
/* File:   visual.hpp                                                       */
/* Author: Joachim Schoeberl                                               */
/* Date:   02. Dec. 01                                                     */
/* *************************************************************************/

/* 

Visualization

*/

#ifdef PARALLEL
#define PARALLELGL
#endif

#include "../include/incvis.hpp"

namespace netgen
{
#include "mvdraw.hpp"
#include "soldata.hpp"
#include "vssolution.hpp"
#include "meshdoc.hpp"
}

#endif
