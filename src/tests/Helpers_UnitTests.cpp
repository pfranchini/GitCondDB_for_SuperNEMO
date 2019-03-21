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

#include "GitCondDB.h"

#include "DBImpl.h"
#include "iov_helpers.h"

#include "gtest/gtest.h"

using namespace GitCondDB::v1;

TEST( IOVHelpers, ParseIOVs ) {
  using GitCondDB::Helpers::get_key_iov;

  const std::string test_data{"0 a\n"
                              "100 b\n"
                              "200 c\n"
                              "300 d\n"};

  {
    auto [key, iov] = get_key_iov( test_data, 0 );
    EXPECT_TRUE( iov.valid() );
    EXPECT_EQ( key, "a" );
    EXPECT_EQ( iov.since, 0 );
    EXPECT_EQ( iov.until, 100 );
  }
  {
    auto [key, iov] = get_key_iov( test_data, 100 );
    EXPECT_TRUE( iov.valid() );
    EXPECT_EQ( key, "b" );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 200 );
  }
  {
    auto [key, iov] = get_key_iov( test_data, 230 );
    EXPECT_TRUE( iov.valid() );
    EXPECT_EQ( key, "c" );
    EXPECT_EQ( iov.since, 200 );
    EXPECT_EQ( iov.until, 300 );
  }
  {
    auto [key, iov] = get_key_iov( test_data, 500 );
    EXPECT_TRUE( iov.valid() );
    EXPECT_EQ( key, "d" );
    EXPECT_EQ( iov.since, 300 );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
  }

  {
    auto [key, iov] = get_key_iov( test_data, 240, {210, 1000} );
    EXPECT_TRUE( iov.valid() );
    EXPECT_EQ( key, "c" );
    EXPECT_EQ( iov.since, 210 );
    EXPECT_EQ( iov.until, 300 );
  }
  {
    auto [key, iov] = get_key_iov( test_data, 500, {210, 1000} );
    EXPECT_TRUE( iov.valid() );
    EXPECT_EQ( key, "d" );
    EXPECT_EQ( iov.since, 300 );
    EXPECT_EQ( iov.until, 1000 );
  }

  {
    auto [key, iov] = get_key_iov( test_data, 2000, {210, 1000} );
    EXPECT_FALSE( iov.valid() );
    EXPECT_EQ( key, "" );
  }
}

using IOV = CondDB::IOV;

TEST( IOV, Validity ) {
  const IOV reference{10, 20};

  const IOV bad1{10, 10};
  const IOV bad2{10, 0};

  EXPECT_TRUE( reference.valid() );
  EXPECT_FALSE( bad1.valid() );
  EXPECT_FALSE( bad2.valid() );
}

TEST( IOV, Intersect ) {
  for ( CondDB::time_point_t a : std::vector<CondDB::time_point_t>{90, 100, 110} ) {
    for ( CondDB::time_point_t b : std::vector<CondDB::time_point_t>{190, 200, 210} ) {
      const auto res   = IOV{100, 200}.intersect( IOV{a, b} );
      const auto since = std::max<CondDB::time_point_t>( a, 100 );
      const auto until = std::min<CondDB::time_point_t>( b, 200 );

      EXPECT_EQ( res.since, since );
      EXPECT_EQ( res.until, until );
    }
  }
  {
    const auto res = IOV{100, 200}.intersect( {0, 10} );
    EXPECT_FALSE( res.valid() );
  }
  {
    const auto res = IOV{100, 200}.intersect( {10, 0} );
    EXPECT_FALSE( res.valid() );
  }
}

TEST( IOV, Cut ) {
  IOV iov{100, 200};

  EXPECT_EQ( iov.since, 100 );
  EXPECT_EQ( iov.until, 200 );

  iov.cut( {100, 190} );
  EXPECT_EQ( iov.since, 100 );
  EXPECT_EQ( iov.until, 190 );

  iov.cut( {110, 190} );
  EXPECT_EQ( iov.since, 110 );
  EXPECT_EQ( iov.until, 190 );

  iov.cut( {100, 180} );
  EXPECT_EQ( iov.since, 110 );
  EXPECT_EQ( iov.until, 180 );

  iov.cut( {120, 200} );
  EXPECT_EQ( iov.since, 120 );
  EXPECT_EQ( iov.until, 180 );

  iov.cut( {130, 170} );
  EXPECT_EQ( iov.since, 130 );
  EXPECT_EQ( iov.until, 170 );

  iov.cut( {0, 10} );
  EXPECT_FALSE( iov.valid() );
}

#define CONTAINS_TRUE( x, y )                                                                                          \
  {                                                                                                                    \
    const IOV other{x, y};                                                                                             \
    EXPECT_TRUE( reference.contains( other ) );                                                                        \
  }

#define CONTAINS_FALSE( x, y )                                                                                         \
  {                                                                                                                    \
    const IOV other{x, y};                                                                                             \
    EXPECT_FALSE( reference.contains( other ) );                                                                       \
  }

TEST( IOV, Contains ) {
  const IOV reference{10, 20};

  EXPECT_TRUE( reference.contains( 10 ) );
  EXPECT_TRUE( reference.contains( 15 ) );
  EXPECT_FALSE( reference.contains( 5 ) );
  EXPECT_FALSE( reference.contains( 25 ) );

  EXPECT_TRUE( reference.contains( reference ) );

  CONTAINS_TRUE( 11, 19 );
  CONTAINS_TRUE( 10, 15 );
  CONTAINS_TRUE( 15, 20 );
  CONTAINS_FALSE( 5, 15 );
  CONTAINS_FALSE( 15, 25 );
  CONTAINS_FALSE( 5, 10 );
  CONTAINS_FALSE( 20, 25 );

  const IOV bad{15, 15};
  EXPECT_FALSE( bad.contains( 15 ) );
  EXPECT_FALSE( bad.contains( 5 ) );
  EXPECT_FALSE( bad.contains( 20 ) );
  EXPECT_FALSE( reference.contains( bad ) );
  EXPECT_FALSE( bad.contains( reference ) );
}

#define OVERLAPS_TRUE( a, b, x, y )                                                                                    \
  {                                                                                                                    \
    const IOV reference{a, b}, other{x, y};                                                                            \
    EXPECT_TRUE( reference.overlaps( other ) );                                                                        \
  }

#define OVERLAPS_FALSE( a, b, x, y )                                                                                   \
  {                                                                                                                    \
    const IOV reference{a, b}, other{x, y};                                                                            \
    EXPECT_FALSE( reference.overlaps( other ) );                                                                       \
  }

TEST( IOV, Overlaps ) {
  OVERLAPS_TRUE( 10, 20, 10, 20 );
  OVERLAPS_TRUE( 10, 20, 11, 19 );
  OVERLAPS_TRUE( 10, 20, 5, 25 );

  OVERLAPS_TRUE( 10, 20, 15, 20 );
  OVERLAPS_TRUE( 10, 20, 15, 25 );
  OVERLAPS_TRUE( 10, 20, 10, 15 );
  OVERLAPS_TRUE( 10, 20, 5, 15 );

  OVERLAPS_TRUE( 10, 20, 10, 25 );
  OVERLAPS_TRUE( 10, 20, 5, 20 );

  OVERLAPS_FALSE( 10, 20, 0, 5 );
  OVERLAPS_FALSE( 10, 20, 25, 30 );
}

int main( int argc, char** argv ) {
  ::testing::InitGoogleTest( &argc, argv );
  return RUN_ALL_TESTS();
}
