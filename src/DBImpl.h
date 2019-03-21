#ifndef DBIMPL_H
#define DBIMPL_H
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

#if __GNUC__ >= 8
#  include <filesystem>
namespace fs = std::filesystem;
#else
#  include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

#include "git_helpers.h"

#include "common.h"

#include <fstream>
#include <variant>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

namespace GitCondDB {
  inline namespace v1 {
    namespace details {
      template <class RET, class FUNC, class... ARGS>
      RET git_call( std::string_view err_msg, std::string_view key, FUNC func, ARGS&&... args ) {
        typename RET::element_type* tmp = nullptr;
        if ( UNLIKELY( func( &tmp, std::forward<ARGS>( args )... ) ) )
          throw std::runtime_error{std::string{err_msg} + " " + std::string{key} + ": " + giterr_last()->message};
        return RET{tmp};
      }

      /// Helper no-op logger to simplify implementations
      struct NullLogger : Logger {
        void warning( std::string_view ) const override {}
        void info( std::string_view ) const override {}
        void debug( std::string_view ) const override {}
      };

      class DBImpl {
      public:
        using dir_content = CondDB::dir_content;

        virtual ~DBImpl() = default;

        virtual void disconnect() const = 0;

        virtual bool connected() const = 0;

        virtual bool exists( const char* object_id ) const = 0;

        virtual std::variant<std::string, dir_content> get( const char* object_id ) const = 0;

        virtual std::chrono::system_clock::time_point commit_time( const char* commit_id ) const = 0;

        inline static std::string_view strip_tag( std::string_view object_id ) {
          if ( const auto pos = object_id.find_first_of( ':' ); pos != object_id.npos ) {
            object_id.remove_prefix( pos + 1 );
          }
          return object_id;
        }

        DBImpl( std::shared_ptr<Logger> logger ) { set_logger( std::move( logger ) ); }

        void set_logger( std::shared_ptr<Logger> logger ) {
          if ( logger ) {
            log.swap( logger );
          } else {
            log = std::make_shared<NullLogger>();
          }
        }
        Logger* logger() const { return log.get(); }

        // logging helpers
        void debug( std::string_view msg ) const { log->debug( msg ); }
        void info( std::string_view msg ) const { log->info( msg ); }
        void warning( std::string_view msg ) const { log->warning( msg ); }

      private:
        std::shared_ptr<Logger> log;
      };

      class GitImpl : public DBImpl {
        using git_object_ptr     = GitCondDB::Helpers::git_object_ptr;
        using git_repository_ptr = GitCondDB::Helpers::git_repository_ptr;

      public:
        GitImpl( std::string_view repository, std::shared_ptr<Logger> logger = nullptr )
            : DBImpl{std::move( logger )}
            , m_repository_url( repository )
            , m_repository{[this]() -> git_repository_ptr::storage_t {
              info( fmt::format( "opening Git repository '{}'", m_repository_url ) );
              auto res = git_call<git_repository_ptr::storage_t>( "cannot open repository", m_repository_url,
                                                                  git_repository_open, m_repository_url.c_str() );
              if ( UNLIKELY( !res ) ) throw std::runtime_error{"invalid Git repository: '" + m_repository_url + "'"};
              return res;
            }} {
          // Initialize Git library
          git_libgit2_init();

          // try access during construction
          m_repository.get();
        }

        ~GitImpl() override {
          // Finalize Git library
          git_libgit2_shutdown();
        }

        void disconnect() const override {
          debug( "disconnect from Git repository" );
          m_repository.reset();
        }

        bool connected() const override { return m_repository.is_set(); }

        bool exists( const char* object_id ) const override {
          git_object* tmp = nullptr;
          git_revparse_single( &tmp, m_repository.get(), object_id );
          bool result = tmp;
          git_object_free( tmp );
          return result;
        }

        std::variant<std::string, dir_content> get( const char* object_id ) const override {
          debug( std::string{"get Git object "} + object_id );
          std::variant<std::string, dir_content> out;
          auto                                   obj = get_object( object_id );
          if ( git_object_type( obj.get() ) == GIT_OBJ_TREE ) {
            debug( "found tree object" );

            dir_content entries;
            entries.root = strip_tag( object_id );

            const git_tree* tree = reinterpret_cast<const git_tree*>( obj.get() );

            const std::size_t     max_i = git_tree_entrycount( tree );
            const git_tree_entry* te    = nullptr;

            for ( std::size_t i = 0; i < max_i; ++i ) {
              te = git_tree_entry_byindex( tree, i );
              ( ( git_tree_entry_type( te ) == GIT_OBJ_TREE ) ? entries.dirs : entries.files )
                  .emplace_back( git_tree_entry_name( te ) );
            }
            out = std::move( entries );
          } else {
            debug( "found blob object" );

            auto blob = reinterpret_cast<const git_blob*>( obj.get() );
            out       = std::string{reinterpret_cast<const char*>( git_blob_rawcontent( blob ) ),
                              static_cast<std::size_t>( git_blob_rawsize( blob ) )};
          }
          return out;
        }

        std::chrono::system_clock::time_point commit_time( const char* commit_id ) const override {
          auto obj = get_object( commit_id, "commit" );
          return std::chrono::system_clock::from_time_t(
              git_commit_time( reinterpret_cast<git_commit*>( obj.get() ) ) );
        }

      private:
        git_object_ptr get_object( const char* commit_id, const std::string& obj_type = "object" ) const {
          return git_call<git_object_ptr>( "cannot resolve " + obj_type, commit_id, git_revparse_single,
                                           m_repository.get(), commit_id );
        }

        std::string m_repository_url;

        mutable git_repository_ptr m_repository;
      };

      class FilesystemImpl : public DBImpl {
      public:
        FilesystemImpl( std::string_view root, std::shared_ptr<Logger> logger = nullptr )
            : DBImpl{std::move( logger )}, m_root( root ) {
          info( fmt::format( "using files from '{}'", m_root.string() ) );
          if ( !is_directory( m_root ) ) throw std::runtime_error{"invalid path " + m_root.string()};
        }

        void disconnect() const override {}

        bool connected() const override { return true; }

        bool exists( const char* object_id ) const override {
          // return true for any tag name (i.e. id without a ':') and existing paths
          const std::string_view id{object_id};
          return id.find_first_of( ':' ) == id.npos || fs::exists( to_path( id ) );
        }

        std::variant<std::string, dir_content> get( const char* object_id ) const override {
          std::variant<std::string, dir_content> out;
          const auto                             path = to_path( object_id );

          debug( std::string{"accessing path "} + path.string() );

          if ( is_directory( path ) ) {
            debug( "found directory" );

            dir_content entries;
            entries.root = strip_tag( object_id );

            for ( auto p : fs::directory_iterator( path ) ) {
              ( is_directory( p.path() ) ? entries.dirs : entries.files ).emplace_back( p.path().filename() );
            }

            out = std::move( entries );
          } else if ( is_regular_file( path ) ) {
            debug( "found regular file" );

            std::ifstream stream{path.string()};
            stream.seekg( 0, stream.end );
            const auto size = stream.tellg();
            stream.seekg( 0 );

            std::string data( static_cast<std::size_t>( size ), 0 );
            stream.read( data.data(), size );

            out = std::move( data );
          } else {
            throw std::runtime_error{std::string{"cannot resolve object "} + object_id};
          }
          return out;
        }

        std::chrono::system_clock::time_point commit_time( const char* ) const override {
          return std::chrono::time_point<std::chrono::system_clock>::max();
        }

      private:
        inline fs::path to_path( std::string_view object_id ) const { return m_root / strip_tag( object_id ); }

        fs::path m_root;
      };

      class JSONImpl : public DBImpl {
        using json = nlohmann::json;

      public:
        JSONImpl( std::string_view data, std::shared_ptr<Logger> logger = nullptr ) : DBImpl{std::move( logger )} {
          if ( data.find_first_of( '{' ) != data.npos ) {
            info( "using JSON data from memory" );
            m_json = json::parse( data );
          } else if ( is_regular_file( fs::path( data ) ) ) {
            info( fmt::format( "loading JSON data from '{}'", data ) );
            std::ifstream stream{std::string{data}};
            stream >> m_json;
          } else {
            throw std::runtime_error{"invalid JSON"};
          }
        }

        void disconnect() const override {}

        bool connected() const override { return true; }

        bool exists( const char* object_id ) const override {
          // return true for any tag name (i.e. id without a ':') and existing paths
          const std::string_view id{object_id};
          return id.find_first_of( ':' ) == id.npos || !m_json.value( to_path( object_id ), json{} ).is_null();
        }

        std::variant<std::string, dir_content> get( const char* object_id ) const override {
          std::variant<std::string, dir_content> out;

          const auto path = to_path( object_id );
          debug( fmt::format( "accessing entry '{}'", path.to_string() ) );

          auto obj = m_json.value( path, json{} );

          if ( UNLIKELY( obj.is_null() ) ) {
            throw std::runtime_error{std::string{"cannot resolve object "} + object_id};
          } else if ( obj.is_object() ) {
            debug( "found object" );

            dir_content entries;

            entries.root = strip_tag( object_id );

            // for(const auto& elem: )
            for ( auto it = obj.begin(); it != obj.end(); ++it ) {
              ( it.value().is_object() ? entries.dirs : entries.files ).emplace_back( it.key() );
            }

            out = std::move( entries );
          } else if ( LIKELY( obj.is_string() ) ) {
            debug( "found string" );
            out = std::string{obj};
          } else {
            throw std::runtime_error{std::string{"invalid type at "} + object_id};
          }
          return out;
        }

        std::chrono::system_clock::time_point commit_time( const char* ) const override {
          return std::chrono::time_point<std::chrono::system_clock>::max();
        }

      private:
        inline json::json_pointer to_path( std::string_view object_id ) const {
          const auto path = strip_tag( object_id );
          return json::json_pointer{path.empty() ? std::string{} : std::string{'/'} + std::string{path}};
        }

        json m_json;
      };
    } // namespace details
  }   // namespace v1
} // namespace GitCondDB

#endif // DBIMPL_H
