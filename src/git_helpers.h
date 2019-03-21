#ifndef GIT_HELPERS_H
#define GIT_HELPERS_H
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

#include <algorithm>
#include <functional>
#include <git2.h>
#include <memory>
#include <mutex>
#include <string>

namespace GitCondDB {
  namespace Helpers {

    struct git_object_deleter {
      void operator()( git_object* ptr ) { git_object_free( ptr ); }
    };
    struct git_repository_deleter {
      void operator()( git_repository* ptr ) { git_repository_free( ptr ); }
    };

    using git_object_ptr = std::unique_ptr<git_object, git_object_deleter>;

    /// Helper class to allow on-demand connection to the git repository.
    class git_repository_ptr {
    public:
      using storage_t = std::unique_ptr<git_repository, git_repository_deleter>;
      using factory_t = std::function<storage_t()>;
      using pointer   = storage_t::pointer;
      using reference = std::add_lvalue_reference<storage_t::element_type>::type;

      git_repository_ptr( factory_t factory ) : m_factory( std::move( factory ) ) {}

      pointer get() const {
        {
          std::lock_guard<std::mutex> guard( m_ptr_mutex );
          if ( !m_ptr ) {
            m_ptr = m_factory();
            if ( !m_ptr ) throw std::runtime_error( "unable create object" );
          }
        }
        return m_ptr.get();
      }

      reference operator*() const { return *get(); }

      pointer operator->() const { return get(); }

      explicit operator bool() const { return get(); }

      void reset() {
        std::lock_guard<std::mutex> guard( m_ptr_mutex );
        m_ptr.reset();
      }

      bool is_set() const { return bool{m_ptr}; }

    private:
      factory_t          m_factory;
      mutable storage_t  m_ptr;
      mutable std::mutex m_ptr_mutex;
    };
  } // namespace Helpers
} // namespace GitCondDB
#endif // GIT_HELPERS_H
