#pragma once

#include <term.h>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <cpprest/json.h>

class mapping_opt;

class hierarchical_opt {
public:
  void get(web::json::value& opt);
  std::string file_in;
  std::string name;
};

class hierarchical {
public:
  void parse(hierarchical_opt& opts);
  term& get(std::string id);
  std::vector<std::string> parents_of(std::string id);
  bool in(std::string term) {
    return _terms.find(term) != _terms.end();
  }
  bool is_root(std::string s) {
    for (char c : s)
      if (c == ':')
        return false;
    return true;
  }
  std::unordered_map<std::string, term>& terms() {return _terms;}
  uint64_t get_hash(){return _hash;}
protected:
  std::unordered_map<std::string, term> _terms;
  uint64_t _hash;
};


class mapping_hierarchical {
public:
  void parse(mapping_opt& opt, hierarchical& annotation);
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
  mapping_hierarchical make_background(std::string& file_in);
  mapping_hierarchical make_background(std::unordered_set<std::string>& gene_set);
  void dedup();
  void add(std::unordered_map<std::string, std::vector<std::string>>::iterator it);
  uint64_t get_hash(){return _hash;}
  std::unordered_map<std::string, std::vector<std::string>>& map() {
    return _a_to_b_spec;
  } 
protected:
  std::unordered_map<std::string, std::vector<std::string>> _a_to_b_spec;
  std::unordered_map<std::string, std::vector<std::string>> _b_to_a_spec;
  uint64_t _hash;
};

