#pragma once

#include <unordered_set>
#include <string>
#include <datastructures_go.h>
#include <datastructures_flat.h>
#include <datastructures_hierarchical.h>
#include <term.h>
#include <cpprest/json.h>
#include <vector>
#include <sql_interface.h>

class set_membership_go {
public:
  void get_sets(const sql_mapping_result& sql,
                const std::string& term);
  web::json::value to_json();
  std::vector<std::string> mt;
  std::vector<std::string> nt;
  std::vector<std::string> mpat;
  std::vector<std::string> npat;
};

class set_membership_hierarchical {
public:
  void get_sets(const sql_mapping_result& sql,
                const std::string& term);
  web::json::value to_json();
  std::vector<std::string> mt;
  std::vector<std::string> nt;
  std::vector<std::string> mpat;
  std::vector<std::string> npat;
};

class set_membership_flat {
public:
  void get_sets(std::unordered_set<std::string>& test_set,
                mapping_flat& map,
                const std::string& id);
  web::json::value to_json();
  std::vector<std::string> mt;
  std::vector<std::string> nt;
  std::vector<std::string> m;
  std::vector<std::string> n;
};