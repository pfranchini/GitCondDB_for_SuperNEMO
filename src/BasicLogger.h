#ifndef BASICLOGGER_H
#define BASICLOGGER_H
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

#include <GitCondDB.h>

#include <iomanip>
#include <iostream>

using namespace GitCondDB::v1;

struct BasicLogger : Logger {
  inline void print( std::string_view level, std::string_view msg ) const {
    std::cout << std::left << std::setw( 7 ) << level << ':' << ' ' << msg << '\n';
  }
  void warning( std::string_view msg ) const override {
    if ( level <= Level::Quiet ) print( "warning", msg );
  }
  void info( std::string_view msg ) const override {
    if ( level <= Level::Verbose ) print( "info", msg );
  }
  void debug( std::string_view msg ) const override {
    if ( level <= Level::Debug ) print( "debug", msg );
  }
};

#endif // BASICLOGGER_H
