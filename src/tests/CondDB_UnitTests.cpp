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

#include "test_common.h"

#include "gtest/gtest.h"

using namespace GitCondDB::v1;

namespace {
  /// helper to extract the file name from a full path
  std::string_view basename( std::string_view path ) {
    // note: if '/' is not found, we get npos and npos + 1 is 0
    return path.substr( path.rfind( '/' ) + 1 );
  }

  // note: copied from LHCb DetCond/src/component/CondDBCommon.cpp
  std::string generateXMLCatalog( const CondDB::dir_content& content ) {
    std::ostringstream xml; // buffer for the XML

    const auto name = basename( content.root );
    // XML header, root element and catalog initial tag
    xml << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>"
        << "<!DOCTYPE DDDB SYSTEM \"git:/DTD/structure.dtd\">"
        << "<DDDB><catalog name=\"" << name << "\">";

    // sub-foldersets are considered as catalogs
    for ( std::string_view f : content.dirs ) { xml << "<catalogref href=\"" << name << '/' << f << "\"/>"; }

    // sub-folders are considered as container of conditions
    for ( std::string_view f : content.files ) {
      // Ignore folders with .xml or .txt extension.
      // We never used .xml for Online conditions and after the Hlt1/Hlt2 split
      // we need to avoid automatic mapping for the .xml files.
      const std::string_view suffix = ( f.length() >= 4 ) ? f.substr( f.length() - 4 ) : std::string_view{""};
      if ( !( suffix == ".xml" || suffix == ".txt" ) ) { xml << "<conditionref href=\"" << name << '/' << f << "\"/>"; }
    }

    // catalog and root element final tag
    xml << "</catalog></DDDB>";

    return xml.str();
  }
} // namespace

TEST( CondDB, Connection ) {
  CondDB db = connect( "test_data/repo.git" );

  EXPECT_TRUE( db.connected() );
  db.disconnect();
  EXPECT_FALSE( db.connected() );
  EXPECT_EQ( std::get<0>( db.get( {"HEAD", "TheDir/TheFile.txt", 0} ) ), "some data\n" );
  EXPECT_TRUE( db.connected() );

  db.disconnect();
  EXPECT_FALSE( db.connected() );
  {
    auto _ = db.scoped_connection();
    EXPECT_FALSE( db.connected() );
    EXPECT_EQ( std::get<0>( db.get( {"HEAD", "TheDir/TheFile.txt", 0} ) ), "some data\n" );
    EXPECT_TRUE( db.connected() );
  }
  EXPECT_FALSE( db.connected() );
}

TEST( CondDB, Access ) {
  {
    CondDB db = connect( "test_data/repo.git" );
    EXPECT_EQ( std::get<0>( db.get( {"HEAD", "TheDir/TheFile.txt", 0} ) ), "some data\n" );
    EXPECT_EQ( std::chrono::system_clock::to_time_t( db.commit_time( "HEAD" ) ), 1483225200 );
  }
  {
    CondDB db = connect( "git:test_data/repo.git" );
    EXPECT_EQ( std::get<0>( db.get( {"HEAD", "TheDir/TheFile.txt", 0} ) ), "some data\n" );
    EXPECT_EQ( std::chrono::system_clock::to_time_t( db.commit_time( "HEAD" ) ), 1483225200 );
  }
  {
    CondDB db = connect( "file:test_data/repo" );
    EXPECT_EQ( std::get<0>( db.get( {"HEAD", "TheDir/TheFile.txt", 0} ) ), "some uncommitted data\n" );
    EXPECT_EQ( db.commit_time( "HEAD" ), std::chrono::time_point<std::chrono::system_clock>::max() );
  }
  {
    CondDB db = connect( R"(json:
                         {"TheDir": {"TheFile.txt": "some JSON (memory) data\n"}}
                         )" );
    EXPECT_EQ( std::get<0>( db.get( {"HEAD", "TheDir/TheFile.txt", 0} ) ), "some JSON (memory) data\n" );
    EXPECT_EQ( db.commit_time( "HEAD" ), std::chrono::time_point<std::chrono::system_clock>::max() );
  }
  {
    CondDB db = connect( "json:test_data/json/basic.json" );
    EXPECT_EQ( std::get<0>( db.get( {"HEAD", "TheDir/TheFile.txt", 0} ) ), "some JSON (file) data\n" );
    EXPECT_EQ( db.commit_time( "HEAD" ), std::chrono::time_point<std::chrono::system_clock>::max() );
  }
}

TEST( CondDB, Directory ) {
  CondDB db = connect( "test_data/lhcb/repo" );

  const std::string default_dir_output =
      R"({"dirs":["Nested"],"files":["Cond1","Cond2","Ignored.txt","Ignored.xml"],"root":"Direct"})";

  const std::string lhcb_dir_output = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>"
                                      "<!DOCTYPE DDDB SYSTEM \"git:/DTD/structure.dtd\">"
                                      "<DDDB><catalog name=\"Direct\">"
                                      "<catalogref href=\"Direct/Nested\"/>"
                                      "<conditionref href=\"Direct/Cond1\"/>"
                                      "<conditionref href=\"Direct/Cond2\"/>"
                                      "</catalog></DDDB>";

  {
    auto [data, iov] = db.get( {"HEAD", "Direct", 0} );
    EXPECT_EQ( iov.since, GitCondDB::CondDB::IOV::min() );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, default_dir_output );
  }

  auto old = db.set_dir_converter( generateXMLCatalog );
  {
    auto [data, iov] = db.get( {"HEAD", "Direct", 0} );
    EXPECT_EQ( iov.since, GitCondDB::CondDB::IOV::min() );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, lhcb_dir_output );
  }

  db.set_dir_converter( old );
  {
    auto [data, iov] = db.get( {"HEAD", "Direct", 0} );
    EXPECT_EQ( iov.since, GitCondDB::CondDB::IOV::min() );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, default_dir_output );
  }
}

TEST( CondDB, IOVAccess ) {
  CondDB db = connect( "test_data/repo" );

  {
    auto [data, iov] = db.get( {"v0", "Cond", 0} );
    EXPECT_EQ( iov.since, 0 );
    EXPECT_EQ( iov.until, 100 );
    EXPECT_EQ( data, "data 0" );
  }
  {
    auto [data, iov] = db.get( {"v0", "Cond", 110} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 0} );
    EXPECT_EQ( iov.since, 0 );
    EXPECT_EQ( iov.until, 100 );
    EXPECT_EQ( data, "data 0" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 110} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 150 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 150} );
    EXPECT_EQ( iov.since, 150 );
    EXPECT_EQ( iov.until, 200 );
    EXPECT_EQ( data, "data 2" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 210} );
    EXPECT_EQ( iov.since, 200 );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, "data 3" );
  }

  // for attempt of invalid retrieval
  {
    auto [data, iov] = db.get( {"v1", "Cond", 210}, {0, 200} );
    EXPECT_FALSE( iov.valid() );
    EXPECT_EQ( data, "" );
  }
}

TEST( CondDB, GetIOVs ) {
  {
    CondDB db = connect( "test_data/repo" );

    std::vector<CondDB::time_point_t> expected{0, 100, 150, 200};

    EXPECT_EQ( db.iov_boundaries( "v1", "Cond" ), expected );
  }
  {
    CondDB db = connect( R"(json:{
                         "Cond": {
                           "IOVs": "0 a\n100 level1\n200 b\n",
                           "level1": {
                             "IOVs": "50 i\n150 level2\n300 k\n",
                             "level2": {
                               "IOVs": "150 x\n170 y\n"
                             }
                           }
                         }
                         })" );

    std::vector<CondDB::time_point_t> expected{0, 100, 150, 170, 200};

    EXPECT_EQ( db.iov_boundaries( "", "Cond" ), expected );
  }
  {
    CondDB db = connect( R"(json:{
                          "Cond": {
                            "IOVs": "0 a\n100 levelA\n200 b\n",
                            "levelA": {
                              "IOVs": "50 i\n150 ../levelB\n300 k\n"
                            },
                            "levelB": {
                              "IOVs": "150 x\n170 y\n"
                            }
                          }
                          })" );

    std::vector<CondDB::time_point_t> expected{0, 100, 150, 170, 200};

    EXPECT_EQ( db.iov_boundaries( "", "Cond" ), expected );
  }
}

TEST( CondDB, Directory_FS ) {
  CondDB db = connect( "file:test_data/lhcb/repo" );

  const std::string dir_output =
      R"({"dirs":["Nested"],"files":["Cond1","Cond2","Ignored.txt","Ignored.xml"],"root":"Direct"})";

  auto [data, iov] = db.get( {"dummy", "Direct", 0} );
  EXPECT_EQ( iov.since, GitCondDB::CondDB::IOV::min() );
  EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
  EXPECT_EQ( data, dir_output );
}

TEST( CondDB, IOVAccess_FS ) {
  CondDB db = connect( "file:test_data/repo" );

  {
    auto [data, iov] = db.get( {"v1", "Cond", 0} );
    EXPECT_EQ( iov.since, 0 );
    EXPECT_EQ( iov.until, 100 );
    EXPECT_EQ( data, "data 0" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 110} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 150 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 150} );
    EXPECT_EQ( iov.since, 150 );
    EXPECT_EQ( iov.until, 200 );
    EXPECT_EQ( data, "data 2" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 210} );
    EXPECT_EQ( iov.since, 200 );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, "data 3" );
  }

  // for attempt of invalid retrieval
  {
    auto [data, iov] = db.get( {"v1", "Cond", 210}, {0, 200} );
    EXPECT_FALSE( iov.valid() );
    EXPECT_EQ( data, "" );
  }
}

TEST( CondDB, IOVAccess_JSON ) {
  CondDB db = connect( R"(json:
                       {"Cond": {"IOVs": "0 v0\n100 group\n200 v2\n",
                                 "v0": "data 0",
                                 "v1": "data 1",
                                 "v2": "data 2",
                                 "group": {"IOVs": "50 ../v1"}}}
                       )" );

  {
    auto [data, iov] = db.get( {"v1", "Cond", 0} );
    EXPECT_EQ( iov.since, 0 );
    EXPECT_EQ( iov.until, 100 );
    EXPECT_EQ( data, "data 0" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 110} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 200 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"v1", "Cond", 210} );
    EXPECT_EQ( iov.since, 200 );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, "data 2" );
  }

  // for attempt of invalid retrieval
  {
    auto [data, iov] = db.get( {"v1", "Cond", 210}, {0, 200} );
    EXPECT_FALSE( iov.valid() );
    EXPECT_EQ( data, "" );
  }
}

TEST( CondDB, Logging ) {
  auto logger = std::make_shared<CapturingLogger>();

  CondDB db = connect( R"(json:
                       {"Cond": {"IOVs": "0 v0\n100 group\n200 v2\n",
                                 "v0": "data 0",
                                 "v1": "data 1",
                                 "v2": "data 2",
                                 "group": {"IOVs": "50 ../v1"}}}
                       )" );

  EXPECT_NE( logger.get(), db.logger() );
  db.set_logger( logger );
  EXPECT_EQ( logger.get(), db.logger() );

  db.set_logger( nullptr );
  EXPECT_NE( nullptr, db.logger() );

  {
    details::NullLogger nl;
    nl.debug( "nothing" );
    nl.info( "nothing" );
    nl.warning( "nothing" );
  }
}

TEST( CondDB, IOVReduction ) {
  CondDB db = connect( R"(json:
                       {"Cond": {"IOVs": "0 v0\n100 v1\n150 v1\n200 v2\n250 v2\n",
                                 "v0": "data 0",
                                 "v1": "data 1",
                                 "v2": "data 2"},
                        "Cond2": {"IOVs": "0 v0\n50 v1\n100 group\n200 v2\n",
                                  "v0": "data 0",
                                  "v1": "data 1",
                                  "v2": "data 2",
                                  "group": {"IOVs": "100 ../v1"}}}
                       )" );

  EXPECT_TRUE( db.iov_reduction() );

  {
    auto [data, iov] = db.get( {"", "Cond", 0} );
    EXPECT_EQ( iov.since, 0 );
    EXPECT_EQ( iov.until, 100 );
    EXPECT_EQ( data, "data 0" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond", 110} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 200 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond", 160} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 200 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond", 210} );
    EXPECT_EQ( iov.since, 200 );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, "data 2" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond", 260} );
    EXPECT_EQ( iov.since, 200 );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, "data 2" );
  }

  {
    auto [data, iov] = db.get( {"", "Cond2", 60} );
    EXPECT_EQ( iov.since, 50 );
    EXPECT_EQ( iov.until, 100 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond2", 110} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 200 );
    EXPECT_EQ( data, "data 1" );
  }

  // disable reduction
  db.set_iov_reduction( false );
  EXPECT_FALSE( db.iov_reduction() );
  {
    auto [data, iov] = db.get( {"", "Cond", 0} );
    EXPECT_EQ( iov.since, 0 );
    EXPECT_EQ( iov.until, 100 );
    EXPECT_EQ( data, "data 0" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond", 110} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 150 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond", 160} );
    EXPECT_EQ( iov.since, 150 );
    EXPECT_EQ( iov.until, 200 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond", 210} );
    EXPECT_EQ( iov.since, 200 );
    EXPECT_EQ( iov.until, 250 );
    EXPECT_EQ( data, "data 2" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond", 260} );
    EXPECT_EQ( iov.since, 250 );
    EXPECT_EQ( iov.until, GitCondDB::CondDB::IOV::max() );
    EXPECT_EQ( data, "data 2" );
  }

  {
    auto [data, iov] = db.get( {"", "Cond2", 60} );
    EXPECT_EQ( iov.since, 50 );
    EXPECT_EQ( iov.until, 100 );
    EXPECT_EQ( data, "data 1" );
  }
  {
    auto [data, iov] = db.get( {"", "Cond2", 110} );
    EXPECT_EQ( iov.since, 100 );
    EXPECT_EQ( iov.until, 200 );
    EXPECT_EQ( data, "data 1" );
  }
}

int main( int argc, char** argv ) {
  ::testing::InitGoogleTest( &argc, argv );
  return RUN_ALL_TESTS();
}
