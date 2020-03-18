#include <string>
#include <unordered_set>
#include <cpprest/json.h>
#include <utility.h>
#include <handle_annotation.h>
#include <BSlogger.hpp>
#include <sql_interface.h>

void handle_annotation(annotation& ann,
  std::unordered_set<std::string>& terms,
  web::json::value& ret,
  std::string t,
  const web::json::value conf)
{
  logger log(std::cerr, "annotation");
  log(LOG_INFO) << "Got annotation task " << t << '\n';
  std::string type = ann.get_type(t);
  ret[t] = web::json::value::array();
  uint64_t i = 0;
  if (type == "go")
  {
    go& anno = ann.go_by_name(t);

    for (const auto& term : terms)
    {
      auto r = anno.get(term).to_json();
      sql_interface sql(conf);
      auto parents = sql.get_parents(t, term);
      r["parents"] = web::json::value::array();
      uint64_t j = 0;
      for (const auto& p : parents)
      {
        r["parents"][j++] = web::json::value::string(p);
      }
      ret[t][i++] = r;
    }
  }
  else if (type == "flat")
  {
    flat& anno = ann.flat_by_name(t);
    for (const auto& term : terms)
    {
      auto r = anno.get(term);
      ret[t][i++] = r.to_json();
    }
  }
  else if (type == "hierarchical")
  {
    hierarchical& anno = ann.hierarchical_by_name(t);
    for (const auto& term : terms)
    {
      auto r = anno.get(term).to_json();
      sql_interface sql(conf);
      auto parents = sql.get_parents(t, term);
      r["parents"] = web::json::value::array();
      uint64_t j = 0;
      for (const auto& p : parents)
      {
        r["parents"][j++] = web::json::value::string(p);
      }
      ret[t][i++] = r;
    }
  }
}