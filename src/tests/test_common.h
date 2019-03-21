#ifndef TEST_COMMON_H
#define TEST_COMMON_H
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
#include <string>
#include <tuple>
#include <vector>

struct CapturingLogger : GitCondDB::Logger {
  inline void capture( std::string_view level, std::string_view msg ) const {
    logged_messages.emplace_back( level, msg );
    std::cout << std::left << std::setw( 7 ) << level << ':' << ' ' << msg << '\n';
  }
  void warning( std::string_view msg ) const override { capture( "warning", msg ); }
  void info( std::string_view msg ) const override { capture( "info", msg ); }
  void debug( std::string_view msg ) const override { capture( "debug", msg ); }

  bool contains( std::string_view sub ) const {
    return logged_messages.size() && ( std::get<1>( logged_messages.back() ).find( sub ) != std::string::npos );
  }
  bool contains( std::size_t idx, std::string_view sub ) const {
    return ( logged_messages.size() > idx ) && ( std::get<1>( logged_messages[idx] ).find( sub ) != std::string::npos );
  }

  std::size_t size() const { return logged_messages.size(); }

  mutable std::vector<std::tuple<std::string, std::string>> logged_messages;
};

#endif // TEST_COMMON_H
