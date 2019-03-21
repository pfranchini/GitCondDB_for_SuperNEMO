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
#include <GitCondDBVersion.h>

#include "DBImpl.h"

#include "iov_helpers.h"

#include "BasicLogger.h"

#include <regex>
#include <sstream>
#include <tuple>

#include <nlohmann/json.hpp>

#include <cassert>

using namespace GitCondDB::v1;

namespace {
  /// helper to normalize relative paths
  std::string normalize( std::string path ) {
    // regex for entries to be removed, i.e. "/parent/../" and "/./"
    static const std::regex ignored_re{"(/[^/]+/\\.\\./)|(/\\./)"};
    std::string             old_path;
    while ( old_path.length() != path.length() ) {
      old_path.swap( path );
      path = std::regex_replace( old_path, ignored_re, "/" );
    }
    return path;
  }

  inline std::string format_obj_id( const std::string& tag, const std::string& path ) {
    return tag + ':' + normalize( path );
  }
  inline std::string format_obj_id( const std::string_view& tag, const std::string_view& path ) {
    return format_obj_id( std::string{tag}, std::string{path} );
  }
  inline std::string format_obj_id( const CondDB::Key& key ) { return format_obj_id( key.tag, key.path ); }

  std::string json_dir_converter( const CondDB::dir_content& content ) {
    using json = nlohmann::json;
    return json{{"root", content.root}, {"dirs", content.dirs}, {"files", content.files}}.dump();
  }
} // namespace

CondDB::CondDB( std::unique_ptr<details::DBImpl> impl )
    : m_impl{std::move( impl )}, m_dir_converter{json_dir_converter} {
  assert( m_impl );
}
CondDB::~CondDB() {}

void CondDB::set_logger( std::shared_ptr<Logger> logger ) { m_impl->set_logger( std::move( logger ) ); }

Logger* CondDB::logger() const { return m_impl->logger(); }

void CondDB::disconnect() const { m_impl->disconnect(); }

bool CondDB::connected() const { return m_impl->connected(); }

std::tuple<std::string, CondDB::IOV> CondDB::get( const Key& key, const IOV& bounds ) const {
  const std::string object_id = format_obj_id( key );
  auto              data      = m_impl->get( object_id.c_str() );
  if ( data.index() == 1 ) { // we got a directory
    auto& content = std::get<1>( data );
    if ( find( begin( content.files ), end( content.files ), "IOVs" ) != end( content.files ) ) {
      auto info = GitCondDB::Helpers::get_key_iov( std::get<0>( m_impl->get( ( object_id + "/IOVs" ).c_str() ) ),
                                                   key.time_point, bounds, m_reduce_iovs );
      if ( LIKELY( std::get<1>( info ).valid() ) ) {
        Key new_key = key;
        new_key.path += '/' + std::get<0>( info );
        return get( new_key, std::get<1>( info ) );
      } else {
        return info;
      }
    } else {
      std::vector<std::string> dirs;
      auto&                    files = content.files;
      dirs.reserve( content.dirs.size() );
      for ( auto& f : content.dirs ) {
        ( m_impl->exists( ( object_id + '/' + f + "/IOVs" ).c_str() ) ? files : dirs ).emplace_back( std::move( f ) );
      }
      content.dirs = std::move( dirs );
      std::sort( begin( content.files ), end( content.files ) );
      std::sort( begin( content.dirs ), end( content.dirs ) );
      return {m_dir_converter( content ), {}};
    }
  } else {
    return {std::get<0>( data ), bounds};
  }
}

std::chrono::system_clock::time_point CondDB::commit_time( const std::string& commit_id ) const {
  return m_impl->commit_time( commit_id.c_str() );
}

CondDB GitCondDB::v1::connect( std::string_view repository, std::shared_ptr<Logger> logger ) {
  if ( !logger ) logger = std::make_shared<BasicLogger>();

  if ( repository.substr( 0, 5 ) == "file:" ) {
    return {std::make_unique<details::FilesystemImpl>( repository.substr( 5 ), std::move( logger ) )};
  } else if ( repository.substr( 0, 5 ) == "json:" ) {
    return {std::make_unique<details::JSONImpl>( repository.substr( 5 ), std::move( logger ) )};
  } else if ( repository.substr( 0, 4 ) == "git:" ) {
    return {std::make_unique<details::GitImpl>( repository.substr( 4 ), std::move( logger ) )};
  } else {
    return {std::make_unique<details::GitImpl>( repository, std::move( logger ) )};
  }
}

void CondDB::iov_boundaries_accumulate( const std::string& object_id, const CondDB::IOV& limits,
                                        std::vector<std::pair<CondDB::IOV, std::string>>& acc ) const {
  // get all iovs in the current obj_id
  auto iovs_file = object_id + "/IOVs";
  if ( !m_impl->exists( iovs_file.c_str() ) ) {
    acc.emplace_back( limits, object_id );
  } else {
    const auto tmp = Helpers::parse_IOVs_keys( std::get<0>( m_impl->get( iovs_file.c_str() ) ) );
    std::for_each( begin( tmp ), end( tmp ), [&acc, &object_id, &limits, this]( const auto& entry ) {
      if ( limits.overlaps( entry.first ) )
        iov_boundaries_accumulate( normalize( object_id + '/' + entry.second ), limits.intersect( entry.first ), acc );
    } );
  }
}

std::vector<CondDB::time_point_t> CondDB::iov_boundaries( std::string_view tag, std::string_view path,
                                                          const IOV& boundaries ) const {
  std::vector<CondDB::time_point_t> out;

  const auto object_id = format_obj_id( tag, path );

  if ( UNLIKELY( !boundaries.valid() || !m_impl->exists( object_id.c_str() ) ) ) return out;

  std::vector<std::pair<CondDB::IOV, std::string>> tmp;
  iov_boundaries_accumulate( object_id, boundaries, tmp );
  std::transform( begin( tmp ), end( tmp ), back_inserter( out ),
                  []( const auto& entry ) { return entry.first.since; } );

  return out;
}
