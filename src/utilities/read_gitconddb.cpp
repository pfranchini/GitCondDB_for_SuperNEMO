/******************************************************************************
*
*  read_gitconddb
*  ==============
*   - Print a condition for a given repository, source, tag and time
*   - Clone a list of conditions for a given repository, source and tag
* 
*  P. Franchini 2019 for SuperNEMO
*
\******************************************************************************/

#include "GitCondDB.h"
#include "DBImpl.h"
#include <iostream>
#include <stdio.h>
#include <string.h>
#include "DBImpl.h"
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/stat.h> 
#include <sys/types.h> 

using json = nlohmann::json;

void print_usage() {
  printf("Usage: read_gitconddb -v <tag> -r <[file|git]:local_repository_path> -s <source> (-c <condition>) (-t <time>) \n\n");
  // omitting file|git: corresponds to git:
  // should probably avoid using file: because uses the local checkout, whatever tag it is, so the -v tag is not really considered
}

int main(int argc, char **argv) {

  // Cache directory (can also be an env variable)
  const char* HOME = std::getenv("HOME");
  std::string CACHE_DIR = ".cache/snemo";

  const char *tag = "HEAD";
  char *repository = NULL;
  char *source = NULL;
  char *condition = NULL;
  long unsigned int time = 0;
  
  int option_index = 0;
  while (( option_index = getopt(argc, argv, "v:r:s:c:t:")) != -1){
    switch (option_index) {
    case 'v':
      tag = optarg;
      break;
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
    default:
      printf("Option incorrect\n");
      print_usage();
      return 1;
    } 
  }  

  std::cout << std::endl << "Repository: " << repository << std::endl;
  std::cout << "Tag: " << tag << std::endl;
  std::cout << "Source: " << source << std::endl;
  if (condition) std::cout << "Condition: " << condition << std::endl;
  std::cout << "Time: " << time << std::endl << std::endl;

  char path[80];
  strcpy(path, source);
  if (condition) {
    strcat(path, "/");
    strcat(path, condition);
  }
  
  // Print condition and IOV for the given repository, source, (condition), tag and (time)
  auto db = GitCondDB::connect( repository );
  GitCondDB::CondDB::Key key{tag,path,time};
  auto cond = db.get(key);
  std::cout << "Data:\n" << std::get<0>( cond ) << std::endl << std::endl;
  if (time) std::cout << "IOV: [" <<  std::get<1>( cond ).since << ", " << std::get<1>( cond ).until << ")\n\n";
  
  // Clone the full directory for a given repository, source and tag
  if (!condition){

    // Create/read a json file and write each file on the cache
    std::ofstream json_file;
    json_file.open ("repo.json");
    json_file << std::get<0>( cond );
    json_file.close();
    json j;
    std::ifstream i("repo.json");
    i >> j;

    for (auto itr : j["files"]) {

      struct stat st;
      
      // Read each single file 
      std::string file =  itr;  
      strcpy(path, source); 
      strcat(path, "/"); 
      strcat(path, file.c_str());
      auto [file_content, iov] = db.get( {tag, path, time} );  

      // Create directory for tag/
      char full_path[80];
      strcpy(full_path,HOME);
      strcat(full_path,"/");
      strcat(full_path,CACHE_DIR.c_str());
      strcat(full_path,"/");
      strcat(full_path,tag);
      if (stat(full_path, &st) == -1) mkdir(full_path,0755); 
      
      // Create directory for source/
      strcat(full_path,"/");
      strcat(full_path,source);
      if (stat(full_path, &st) == -1) mkdir(full_path,0755); 

      // Write file in the cache directory
      strcat(full_path,"/");
      strcat(full_path,file.c_str());
      std::cout << full_path << std::endl;
      std::ofstream f(full_path);
      f << file_content;
      
    }//for
  }//if
  
  return 0;

}
