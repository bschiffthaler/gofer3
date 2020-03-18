#include <algorithm>
#include <unordered_set>
#include <string>
#include <cpprest/json.h>
#include <BSlogger.hpp>
#include <set_memberships.h>

void set_membership_go::get_sets(const sql_mapping_result& sql,
                                 const std::string& term) {
  logger log(std::cerr, "set_membership_go::get_sets");

  // g annotated to t globally
  for (const auto& g : sql.term_gene_pop.at(term))
  {
    mt.push_back(g);
  }
  // g annotated to t in test set
  for (const auto& g : sql.term_gene_ts.at(term))
  {
    nt.push_back(g);
  }

  uint64_t nparents = sql.term_parents.at(term).size();

  // g annotated to all parents of t in test set
  std::unordered_map<std::string, uint64_t> ts_parent_map;
  for (const std::string& p : sql.term_parents.at(term))
  {
    for (const std::string& g : sql.term_gene_ts.at(p))
    {
      auto ptr =  ts_parent_map.find(g);
      if (ptr == ts_parent_map.end())
      {
        ts_parent_map[g] = 1;
      }
      else
      {
        ptr->second++;
      }
    }
  }
  for (const auto& ts_p : ts_parent_map)
  {
    if (ts_p.second == nparents)
    {
      npat.push_back(ts_p.first);
    }
  }

  // g annotated to all parents of t globally
  std::unordered_map<std::string, uint64_t> pop_parent_map;
  for (const std::string& p : sql.term_parents.at(term))
  {
    for (const std::string& g : sql.term_gene_pop.at(p))
    {
      auto ptr =  pop_parent_map.find(g);
      if (ptr == pop_parent_map.end())
      {
        pop_parent_map[g] = 1;
      }
      else
      {
        ptr->second++;
      }
    }
  }
  for (const auto& pop_p : pop_parent_map)
  {
    if (pop_p.second == nparents)
    {
      mpat.push_back(pop_p.first);
    }
  }
#ifdef DEBUG
  log(LOG_DEBUG) << "Term: " << id << ", mt: " << mt << ", nt: " << nt
                 << ", mpat: " << mpat << ", npat: " << npat << '\n';
#endif
}

void set_membership_flat::get_sets(std::unordered_set<std::string>& test_set,
                                   mapping_flat& map,
                                   const std::string& id) {
  logger log(std::cerr, "set_memberships_flat::get_sets");
  //log(LOG_INFO) << "Testing " << id << '\n';

  for (auto& p : map.b_to_a(id))
    mt.push_back(p);

  for (auto& p : test_set)
    n.push_back(p);

  for (auto& p : map.a_to_b())
    m.push_back(p.first);

  for (const std::string& g : test_set)
  {
    //log(LOG_INFO) << "Testing gene " << g << '\n';
    auto begin = map.a_to_b(g).begin();
    auto end = map.a_to_b(g).end();
    if (std::binary_search(begin, end, id))
      nt.push_back(g);
  }
#ifdef DEBUG
  log(LOG_DEBUG) << "Term: " << id << ", mt: " << mt << ", nt: " << nt
                 << ", m: " << m << ", n: " << n << '\n';
#endif
}

void set_membership_hierarchical::get_sets(const sql_mapping_result& sql,
    const std::string& term)
{
  logger log(std::cerr, "set_membership_hierarchical::get_sets");

  // g annotated to t globally
  for (const auto& g : sql.term_gene_pop.at(term))
  {
    mt.push_back(g);
  }
  // g annotated to t in test set
  for (const auto& g : sql.term_gene_ts.at(term))
  {
    nt.push_back(g);
  }

  uint64_t nparents = sql.term_parents.at(term).size();

  // g annotated to all parents of t in test set
  std::unordered_map<std::string, uint64_t> ts_parent_map;
  for (const std::string& p : sql.term_parents.at(term))
  {
    for (const std::string& g : sql.term_gene_ts.at(p))
    {
      auto ptr =  ts_parent_map.find(g);
      if (ptr == ts_parent_map.end())
      {
        ts_parent_map[g] = 1;
      }
      else
      {
        ptr->second++;
      }
    }
  }
  for (const auto& ts_p : ts_parent_map)
  {
    if (ts_p.second == nparents)
    {
      npat.push_back(ts_p.first);
    }
  }

  // g annotated to all parents of t globally
  std::unordered_map<std::string, uint64_t> pop_parent_map;
  for (const std::string& p : sql.term_parents.at(term))
  {
    for (const std::string& g : sql.term_gene_pop.at(p))
    {
      auto ptr =  pop_parent_map.find(g);
      if (ptr == pop_parent_map.end())
      {
        pop_parent_map[g] = 1;
      }
      else
      {
        ptr->second++;
      }
    }
  }
  for (const auto& pop_p : pop_parent_map)
  {
    if (pop_p.second == nparents)
    {
      mpat.push_back(pop_p.first);
    }
  }
#ifdef DEBUG
  log(LOG_DEBUG) << "Term: " << id << ", mt: " << mt << ", nt: " << nt
                 << ", mpat: " << mpat << ", npat: " << npat << '\n';
#endif
}

web::json::value set_membership_go::to_json() {
  web::json::value ret;
  ret["mt"] = web::json::value::array();
  ret["nt"] = web::json::value::array();
  ret["mpat"] = web::json::value::array();
  ret["npat"] = web::json::value::array();

  for (std::string& s : mt)
    ret["mt"][ret["mt"].size()] = web::json::value::string(s);

  for (std::string& s : nt)
    ret["nt"][ret["nt"].size()] = web::json::value::string(s);

  for (std::string& s : mpat)
    ret["mpat"][ret["mpat"].size()] = web::json::value::string(s);

  for (std::string& s : npat)
    ret["npat"][ret["npat"].size()] = web::json::value::string(s);

  return ret;
}

web::json::value set_membership_hierarchical::to_json() {
  web::json::value ret;
  ret["mt"] = web::json::value::array();
  ret["nt"] = web::json::value::array();
  ret["mpat"] = web::json::value::array();
  ret["npat"] = web::json::value::array();

  for (std::string& s : mt)
    ret["mt"][ret["mt"].size()] = web::json::value::string(s);

  for (std::string& s : nt)
    ret["nt"][ret["nt"].size()] = web::json::value::string(s);

  for (std::string& s : mpat)
    ret["mpat"][ret["mpat"].size()] = web::json::value::string(s);

  for (std::string& s : npat)
    ret["npat"][ret["npat"].size()] = web::json::value::string(s);

  return ret;
}

web::json::value set_membership_flat::to_json() {
  web::json::value ret;
  ret["mt"] = web::json::value::array();
  ret["nt"] = web::json::value::array();
  ret["m"] = web::json::value::array();
  ret["n"] = web::json::value::array();

  for (std::string& s : mt)
    ret["mt"][ret["mt"].size()] = web::json::value::string(s);

  for (std::string& s : nt)
    ret["nt"][ret["nt"].size()] = web::json::value::string(s);

  for (std::string& s : m)
    ret["m"][ret["m"].size()] = web::json::value::string(s);

  for (std::string& s : n)
    ret["n"][ret["n"].size()] = web::json::value::string(s);

  return ret;
}