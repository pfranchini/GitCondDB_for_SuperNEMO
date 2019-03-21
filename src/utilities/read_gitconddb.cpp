#include "GitCondDB.h"
#include <iostream>
#include "DBImpl.h"
#include <getopt.h>

void print_usage() {
  printf("Usage: read_gitconddb -r <[file|git]:local_repository_path> -s <source> -c <condition> -t <time>\n\n");
}

int main(int argc, char **argv) {

  char *repository = NULL;
  char *source = NULL;
  char *condition = NULL;
  long unsigned int time = 0;  
  int option_index = 0;

  while (( option_index = getopt(argc, argv, "r:s:c:t:")) != -1){
    switch (option_index) {
    case 'r':
      repository = optarg;
      break;
    case 's':
      source = optarg;
      break;
    case 'c':
      condition = optarg;
      break;
    case 't':
      time = atof(optarg);
      break;
      //    case '?':
      //      if (optopt == 'r')
      //	fprintf (stderr, "Option -%d requires an argument.\n", optopt);
    default:
      printf("Option incorrect\n");
      print_usage();
      return 1;
    } 
  }  

  std::cout << std::endl << "Repository: " << repository << std::endl;
  std::cout << "Source: " << source << std::endl;
  std::cout << "Condition: " << condition << std::endl;
  std::cout << "Time: " << time << std::endl << std::endl;
  
  //  auto db = GitCondDB::connect( "git:path/to/repo.git" );
  //  auto db = GitCondDB::connect( "test_data/repo.git" );
  //  auto db = GitCondDB::connect( "file:test_data/repo" );
  //  auto db = GitCondDB::connect( "file:/home/hep/pfranchi/SuperNEMO/supernemo_test_cdb.git" );

  auto db = GitCondDB::connect( repository );
  GitCondDB::CondDB::Key key{"HEAD",strcat(strcat(source,"/"),condition),time};
  auto cond = db.get(key);
  
  std::cout << "Data:\n" << std::get<0>( cond );
  std::cout << "IOV: [" <<  std::get<1>( cond ).since << ", " << std::get<1>( cond ).until << ")\n\n";
  
  return 0;

}
