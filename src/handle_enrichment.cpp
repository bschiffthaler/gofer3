#include <cpprest/json.h>
#include <cpprest/http_listener.h>
#include <BSlogger.hpp>
#include <term.h>
#include <test.h>
#include <utility.h>
#include <datastructures_go.h>
#include <datastructures_flat.h>
#include <datastructures_hierarchical.h>
#include <handle_enrichment.h>
#include <sql_interface.h>

void handle_enrichment(enrichment& e, annotation& a, std::string& t,
                       web::json::value& ret, std::string uri,
                       const web::json::value json,
                       std::unordered_set<std::string>& test_set,
                       const web::json::value org,
                       const web::json::value conf)
{
  logger log(std::cerr, uri);
  log(LOG_INFO) << "Got enrichment task " << t << '\n';
  bool names = true;
  bool defs = true;
  ret[t] = web::json::value();
  if (e.types.find(t) == e.types.end())
  {
    append_error(ret, "Error: Enrichment type not supported by server.");
    return;
  }
  if (json.has_field("include_names"))
  {
    if (! json.at("include_names").is_boolean())
    {
      append_error(ret, "Error: 'include_names' found in JSON, but value"
                   " cannot be interpreted as boolean");
      return;
    }
    names = json.at("include_names").as_bool();
  }
  if (json.has_field("include_defs"))
  {
    if (! json.at("include_defs").is_boolean())
    {
      append_error(ret, "Error: 'include_defs' found in JSON, but value"
                   " cannot be interpreted as boolean");
      return;
    }
    defs = json.at("include_defs").as_bool();
  }
  if (json.has_field("alpha"))
  {
    if (! json.at("alpha").is_number())
    {
      append_error(ret, "Error: 'alpha' found in JSON, but value cannot "
                   "be interpreted as a number");
      return;
    }
  }

  std::string type = e.types[t];

  if (type == "go")
  {
    ret[t] = web::json::value::array();
    mapping_opt& mopt = e.mapping_opt_gos[t];
    std::vector<test_go> results;
    std::vector<double> pvals;
    std::vector<std::string> terms;

    std::shared_ptr<mapping_go> map;
    std::string anno = e.mapping_opt_gos[t].annotation;
    mapping_go custom;
    if (json.has_field("background"))
    {
      if (json.at("background").is_string())
      {
        std::string key = t + "_" + json.at("background").as_string();
        if (e.background_gos.find(key) == e.background_gos.end())
        {
          append_error(ret, "Background specified, but not found in memory."
                       "Did you mean to provide an array of genes?");
          return;
        }
        map = std::make_shared<mapping_go>(e.background_gos[key]);
      }
      else if (json.at("background").is_array())
      {
        web::json::array arr = json.at("background").as_array();
        std::unordered_set<std::string> gene_set;
        for (web::json::value& gene : arr)
          gene_set.insert(gene.as_string());
        for (const std::string& gene : test_set)
          gene_set.insert(gene);
        // FIX ME: Check sanity of background
        custom = e.mapping_gos[t].make_background(gene_set);
        map = std::make_shared<mapping_go>(custom);
      }
    }
    else
    {
      map = std::make_shared<mapping_go>(e.mapping_gos[t]);
    }

    std::unordered_set<std::string> filter_set;
    for (const std::string& id : test_set)
      if ( map->in_a(id) )
        filter_set.insert(id);

    std::unordered_set<std::string> pop;
    for (const auto& g : map->map())
    {
      pop.insert(g.first);
    }

    sql_interface sql(conf);
    sql_mapping_result res = sql.get_annotation(org, filter_set, pop, t, anno);

    // Make a set of terms that will be tested, specifically, all terms with
    // any annotation to any gene will be tested
    auto& test_terms = res.all_terms;
    if (test_terms.size() == 0)
    {
      append_error(ret, "No terms annotated to any of the genes");
      return;
    }

    for (const std::string& term : test_terms)
    {
      // Do not test root terms
      if (a.go_by_name(mopt.annotation).is_root(term)) continue;
      test_go test;
      test.get_cardinality(res, term);
      test.calc_pval();
      if (test.pval > 0)
      {
        results.push_back(test);
        pvals.push_back(test.pval);
        terms.push_back(term);
      }
      else
      {
        append_error(ret, "An error signal was encountered "
                     "processing term " + term +
                     ". If you are using a custom background, double check that "
                     "the test set is a subset of the background, otherwise "
                     "double check results.");
      }
    }
    std::vector<double> padj = bh_adjust(pvals);
    for (size_t i = 0; i < results.size(); i++)
    {
      results[i].padj = padj[i];
    }
    for (size_t i = 0; i < results.size(); i++)
    {
      test_go& result = results[i];
      term_go& term = a.go_by_name(mopt.annotation).get(terms[i]);
      if (json.has_field("alpha"))
      {
        double alpha = json.at("alpha").as_number().to_double();
        if (result.padj < alpha)
          ret[t][ret[t].size()] = result.to_json(term, names, defs);
      }
      else
      {
        double alpha = 1.0;
        if (result.padj < alpha)
          ret[t][ret[t].size()] = result.to_json(term, names, defs);
      }
    }
  }
  else if (type == "flat")
  {
    ret[t] = web::json::value::array();
    mapping_opt& mopt = e.mapping_opt_flats[t];
    std::vector<test_fisher> results;
    std::vector<double> pvals;
    std::vector<std::string> terms;

    std::shared_ptr<mapping_flat> map;
    mapping_flat custom;
    if (json.has_field("background"))
    {
      if (json.at("background").is_string())
      {
        std::string key = t + "_" + json.at("background").as_string();
        if (e.background_flats.find(key) == e.background_flats.end())
        {
          ret[t] = web::json::value::string("Background specified, but not "
                                            "found in memory");
          return;
        }
        map = std::make_shared<mapping_flat>(e.background_flats[key]);
      }
      else if (json.at("background").is_array())
      {
        web::json::array arr = json.at("background").as_array();
        std::unordered_set<std::string> gene_set;
        for (web::json::value& gene : arr)
          gene_set.insert(gene.as_string());
        for (const std::string& gene : test_set)
          gene_set.insert(gene);
        // FIX ME: Check sanity of background
        custom = e.mapping_flats[t].make_background(gene_set);
        map = std::make_shared<mapping_flat>(custom);
      }
    }
    else
    {
      map = std::make_shared<mapping_flat>(e.mapping_flats[t]);
    }

    std::unordered_set<std::string> filter_set;
    for (const std::string& id : test_set)
      if ( map->in_a(id) )
        filter_set.insert(id);

    auto test_terms = map->all_b(filter_set);
    if (test_terms.size() == 0)
      return;

    for (const std::string& term : test_terms)
    {
      test_fisher test;
      test.get_cardinality(filter_set, *map, term);
      test.calc_pval();
      if (test.pval > 0)
      {
        results.push_back(test);
        pvals.push_back(test.pval);
        terms.push_back(term);
      }
      else
      {
        append_error(ret, "An error signal was encountered "
                     "processing term " + term +
                     ". If you are using a custom background, double check that "
                     "the test set is a subset of the background, otherwise "
                     "double check results.");
      }
    }
    std::vector<double> padj = bh_adjust(pvals);
    for (size_t i = 0; i < results.size(); i++)
    {
      results[i].padj = padj[i];
    }
    for (size_t i = 0; i < results.size(); i++)
    {
      test_fisher& result = results[i];
      term& term = a.flat_by_name(mopt.annotation).get(terms[i]);
      if (json.has_field("alpha"))
      {
        double alpha = json.at("alpha").as_number().to_double();
        if (result.padj < alpha)
          ret[t][ret[t].size()] = result.to_json(term, names, defs);
      }
      else
      {
        if (result.padj < 1.0)
          ret[t][ret[t].size()] = result.to_json(term, names, defs);
      }
    }
  }
  else if (type == "hierarchical")
  {
    ret[t] = web::json::value::array();
    mapping_opt& mopt = e.mapping_opt_hierarchicals[t];
    std::string anno = e.mapping_opt_hierarchicals[t].annotation;
    std::vector<test_hierarchical> results;
    std::vector<double> pvals;
    std::vector<std::string> terms;

    std::shared_ptr<mapping_hierarchical> map;
    mapping_hierarchical custom;
    if (json.has_field("background"))
    {
      if (json.at("background").is_string())
      {
        std::string key = t + "_" + json.at("background").as_string();
        if (e.background_hierarchicals.find(key) == e.background_hierarchicals.end())
        {
          ret[t] = web::json::value::string("Background specified, but not "
                                            "found in memory");
          return;
        }
        map = std::make_shared<mapping_hierarchical>(e.background_hierarchicals[key]);
      }
      else if (json.at("background").is_array())
      {
        web::json::array arr = json.at("background").as_array();
        std::unordered_set<std::string> gene_set;
        for (web::json::value& gene : arr)
          gene_set.insert(gene.as_string());
        for (const std::string& gene : test_set)
          gene_set.insert(gene);
        // FIX ME: Check sanity of background
        custom = e.mapping_hierarchicals[t].make_background(gene_set);
        map = std::make_shared<mapping_hierarchical>(custom);
      }
    }
    else
    {
      map = std::make_shared<mapping_hierarchical>(e.mapping_hierarchicals[t]);
    }

    std::unordered_set<std::string> filter_set;
    for (const std::string& id : test_set)
      if ( map->in_a(id) )
        filter_set.insert(id);

    std::unordered_set<std::string> pop;
    for (const auto& g : map->map())
    {
      pop.insert(g.first);
    }

    sql_interface sql(conf);
    sql_mapping_result res = sql.get_annotation(org, filter_set, pop, t, anno);

    auto& test_terms = res.all_terms;
    if (test_terms.size() == 0)
      return;

    for (const std::string& term : test_terms)
    {
      if (a.hierarchical_by_name(mopt.annotation).is_root(term)) continue;
      test_hierarchical test;
      test.get_cardinality(res, term);;
      test.calc_pval();
      if (test.pval > 0)
      {
        results.push_back(test);
        pvals.push_back(test.pval);
        terms.push_back(term);
      }
      else
      {
        append_error(ret, "An error signal was encountered "
                     "processing term " + term +
                     ". If you are using a custom background, double check that "
                     "the test set is a subset of the background, otherwise "
                     "double check results.");
      }
    }
    std::vector<double> padj = bh_adjust(pvals);
    for (size_t i = 0; i < results.size(); i++)
    {
      results[i].padj = padj[i];
    }
    for (size_t i = 0; i < results.size(); i++)
    {
      test_hierarchical& result = results[i];
      term& term = a.hierarchical_by_name(mopt.annotation).get(terms[i]);
      if (json.has_field("alpha"))
      {
        double alpha = json.at("alpha").as_number().to_double();
        if (result.padj < alpha)
          ret[t][ret[t].size()] = result.to_json(term, names, defs);
      }
      else
      {
        if (result.padj < 1.0)
          ret[t][ret[t].size()] = result.to_json(term, names, defs);
      }
    }
  }
}