#pragma once

#include <term.h> 
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cpprest/json.h>
#include <mysql.h>

class mapping_opt;

class go_opt {
public:
  void get(web::json::value& opt);
  bool all_relationships = false;
  bool part_of = false;
  bool regulates = false;
  bool positively_regulates = false;
  bool negatively_regulates = false;
  bool occurs_in = false;
  std::string file_in;
  std::string name;
};

class go {
public:
  void parse(go_opt& opts);
  term_go& get(std::string id);
  std::unordered_set<std::string> parents_of(std::string id);
  bool is_root(const std::string& x) {
    return std::binary_search(_root.begin(), _root.end(), x);
  }
  bool in(std::string id) {
    return _namespaces.find(id) != _namespaces.end() ||
           _alt_id.find(id) != _alt_id.end();
  }
  std::unordered_map<std::string, term_go>& bp() {return _bp;}
  std::unordered_map<std::string, term_go>& mf() {return _mf;}
  std::unordered_map<std::string, term_go>& cc() {return _cc;}
  uint64_t get_hash() {return _hash;}
protected:
  void _rec_parents_of(std::unordered_set<std::string>& s, std::string& id);
  std::unordered_map<std::string, term_go> _bp;
  std::unordered_map<std::string, term_go> _mf;
  std::unordered_map<std::string, term_go> _cc;
  std::unordered_map<std::string, std::string> _alt_id;
  std::unordered_map<std::string, uint8_t> _namespaces;
  std::vector<std::string> _root = {"GO:0000004", "GO:0003674",
                                    "GO:0005554", "GO:0005575",
                                    "GO:0007582",
                                    "GO:0008150", "GO:0008372"};
  uint64_t _hash;
};

class mapping_go {
public:
  void parse(mapping_opt& opt, go& annotation);
  bool in_a(const std::string& id) {
    return _a_to_b_spec.find(id) != _a_to_b_spec.end();
  }
  bool in_b(const std::string& id) {
    return _b_to_a_spec.find(id) != _b_to_a_spec.end();
  }
  std::vector<std::string>& a_to_b(std::string query) {
    return _a_to_b_spec.at(query);
  }
  std::vector<std::string>& b_to_a(std::string query) {
    return _b_to_a_spec.at(query);
  }
  std::unordered_set<std::string> all_a(std::unordered_set<std::string>& b);
  std::unordered_set<std::string> all_b(std::unordered_set<std::string>& a);
  mapping_go make_background(std::string& file_in);
  mapping_go make_background(std::unordered_set<std::string>& gene_set);
  void add(std::unordered_map<std::string, std::vector<std::string>>::iterator it);
  void dedup();
  uint64_t get_hash(){return _hash;}
  std::unordered_map<std::string, std::vector<std::string>>& map() {
    return _a_to_b_spec;
  }
protected:
  std::unordered_map<std::string, std::vector<std::string>> _a_to_b_spec;
  std::unordered_map<std::string, std::vector<std::string>> _b_to_a_spec;
  uint64_t _hash;
};

