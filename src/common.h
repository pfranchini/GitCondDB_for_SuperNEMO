#ifndef COMMON_H
#define COMMON_H
/*****************************************************************************\
* (c) Copyright 2018 CERN for the benefit of the LHCb Collaboration           *
*                                                                             *
* This software is distributed under the terms of the Apache version 2        *
* licence, copied verbatim in the file "COPYING".                             *
*                                                                             *
* In applying this licence, CERN does not waive the privileges and immunities *
* granted to it by virtue of its status as an Intergovernmental Organization  *
* or submit itself to any jurisdiction.                                       *
\*****************************************************************************/

// -------------- LIKELY/UNLIKELY macros (begin)
// Use compiler hinting to improve branch prediction for linux
#ifdef __GNUC__
#  define LIKELY( x ) __builtin_expect( ( x ), 1 )
#  define UNLIKELY( x ) __builtin_expect( ( x ), 0 )
#else
#  define LIKELY( x ) x
#  define UNLIKELY( x ) x
#endif
// -------------- LIKELY/UNLIKELY macros (end)

#endif // COMMON_H
