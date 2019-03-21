#include "GitCondDB.h"
#include <iostream>
#include "DBImpl.h"

int main() {
  //  auto db = GitCondDB::connect( "git:path/to/repo.git" );
  //  auto db = GitCondDB::connect( "test_data/repo.git" );
  //  auto db = GitCondDB::connect( "file:test_data/repo" );
  auto db = GitCondDB::connect( "file:/home/hep/pfranchi/SuperNEMO/supernemo_test_cdb.git" );
  GitCondDB::CondDB::Key key{
    "HEAD",
      "detector1/condition1",
      1543622300000000000 /* time_point */
      };
  auto cond = db.get(key);
  std::cout << "data:\n" << std::get<0>( cond ) << '\n';
  std::cout << "IOV: [" <<  std::get<1>( cond ).since << ", " << std::get<1>( cond ).until << ")\n\n";
  
  GitCondDB::CondDB::Key key2{
    "HEAD",
      "detector1/condition1",
      1545264000000000001 /* time_point */
      };
  auto cond2 = db.get(key2);
  std::cout << "data:\n" << std::get<0>( cond2 ) << '\n';
  std::cout << "IOV: [" <<  std::get<1>( cond2 ).since << ", " << std::get<1>( cond2 ).until << ")\n\n";



}
