#ifndef GITCONDDB_H
#define GITCONDDB_H
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

#include <gitconddb_export.h>

#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

namespace GitCondDB {
  inline namespace v1 {
    namespace details {
      struct DBImpl;
    }

    struct CondDB;
    struct Logger;

    GITCONDDB_EXPORT CondDB connect( std::string_view repository, std::shared_ptr<Logger> logger = nullptr );

    /// Interface for customizable logger
    struct Logger {
      enum class Level { Debug, Verbose, Quiet, Nothing } level = Level::Quiet;

      virtual void warning( std::string_view msg ) const = 0;
      virtual void info( std::string_view msg ) const    = 0;
      virtual void debug( std::string_view msg ) const   = 0;

      virtual ~Logger() = default;
    };

    struct GITCONDDB_EXPORT CondDB {
      using time_point_t = std::uint_fast64_t;

      struct Key {
        std::string  tag;
        std::string  path;
        time_point_t time_point;
      };

      struct IOV {
        time_point_t since = min();
        time_point_t until = max();

        constexpr static time_point_t min() { return std::numeric_limits<time_point_t>::min(); }
        constexpr static time_point_t max() { return std::numeric_limits<time_point_t>::max(); }

        IOV intersect( const IOV& boundary ) const {
          return {std::max( since, boundary.since ), std::min( until, boundary.until )};
        }
        IOV& cut( const IOV& boundary ) { return *this = boundary.intersect( *this ); }

        bool valid() const { return since < until; }
        bool contains( const time_point_t point ) const { return point >= since && point < until; }
        bool contains( const IOV& other ) const {
          return other.valid() && contains( other.since ) && other.until > since && other.until <= until;
        }
        bool overlaps( const IOV& other ) const { return other.intersect( *this ).valid(); }
      };

      /// RAII object to limit the time the connection to the repository stay open.
      /// The lifetime of the CondDB object must be longer than the AccessGuard.
      class AccessGuard {
        friend struct CondDB;
        AccessGuard( const CondDB& db ) : db( db ) {}

        const CondDB& db;

      public:
        ~AccessGuard() { db.disconnect(); }
      };

      void disconnect() const;

      bool connected() const;

      AccessGuard scoped_connection() const { return AccessGuard( *this ); }

      // This siganture is required because of https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58328
      std::tuple<std::string, IOV> get( const Key& key ) const { return get( key, {} ); }

      std::tuple<std::string, IOV> get( const Key& key, const IOV& bounds ) const;

      std::chrono::system_clock::time_point commit_time( const std::string& commit_id ) const;

      std::vector<time_point_t> iov_boundaries( std::string_view tag, std::string_view path ) const {
        return iov_boundaries( tag, path, {} );
      }
      std::vector<time_point_t> iov_boundaries( std::string_view tag, std::string_view path,
                                                const IOV& boundaries ) const;

      CondDB( CondDB&& ) = default;
      ~CondDB();

      struct dir_content {
        std::string              root;
        std::vector<std::string> dirs;
        std::vector<std::string> files;
      };

      using dir_converter_t = std::function<std::string( const dir_content& content )>;
      dir_converter_t set_dir_converter( dir_converter_t converter ) {
        swap( m_dir_converter, converter );
        return converter;
      }

      void    set_logger( std::shared_ptr<Logger> logger );
      Logger* logger() const;

      bool iov_reduction() const { return m_reduce_iovs; }
      void set_iov_reduction( bool value ) { m_reduce_iovs = value; }

    private:
      CondDB( std::unique_ptr<details::DBImpl> impl );

      void iov_boundaries_accumulate( const std::string& object_id, const IOV& limits,
                                      std::vector<std::pair<IOV, std::string>>& acc ) const;

      std::unique_ptr<details::DBImpl> m_impl;

      dir_converter_t m_dir_converter;

      /// If true, hide IOV boundaries if the payload does not change.
      bool m_reduce_iovs = true;

      friend GITCONDDB_EXPORT CondDB connect( std::string_view repository, std::shared_ptr<Logger> logger );
    };
  } // namespace v1
} // namespace GitCondDB

#endif // GITCONDDB_H
