#ifndef IOV_HELPERS_H
#define IOV_HELPERS_H
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

#include "common.h"

#include <sstream>
#include <string>
#include <tuple>

namespace GitCondDB {
  namespace Helpers {
    std::tuple<std::string, CondDB::IOV> get_key_iov( const std::string& data, const CondDB::time_point_t t,
                                                      const CondDB::IOV& boundaries  = {},
                                                      const bool         reduce_iovs = true ) {
      std::tuple<std::string, CondDB::IOV> out;
      auto&                                key   = std::get<0>( out );
      auto&                                since = std::get<1>( out ).since;
      auto&                                until = std::get<1>( out ).until;

      if ( UNLIKELY( t < boundaries.since || t >= boundaries.until ) ) {
        since = until = 0;
      } else {
        CondDB::time_point_t current = 0;
        std::string          line;

        std::istringstream stream{data};
        std::string        tmp_key;

        while ( std::getline( stream, line ) ) {
          std::istringstream is{line};
          is >> current >> tmp_key;
          if ( !reduce_iovs || tmp_key != key ) { // if we do not need to reduce IOVs, ignore identical keys
            if ( current > t ) {
              until = current; // what we read is the "until" for the previous key
              break;           // and we need to use the previous key
            }
            key   = std::move( tmp_key );
            since = current; // the time we read is the "since" for the read key
          }
        }
        std::get<1>( out ).cut( boundaries );
      }
      return out;
    }

    std::vector<std::pair<CondDB::IOV, std::string>> parse_IOVs_keys( const std::string& data ) {
      std::vector<std::pair<CondDB::IOV, std::string>> out;
      std::string                                      line;

      CondDB::time_point_t bound;
      std::string          key;

      std::istringstream stream{data};

      while ( std::getline( stream, line ) ) {
        std::istringstream is{line};
        is >> bound >> key;
        if ( LIKELY( !out.empty() ) ) { out.back().first.until = bound; }
        out.emplace_back( CondDB::IOV{bound, CondDB::IOV::max()}, std::move( key ) );
      }

      return out;
    }
  } // namespace Helpers
} // namespace GitCondDB

#endif // IOV_HELPERS_H
