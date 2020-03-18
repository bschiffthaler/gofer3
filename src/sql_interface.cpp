
#include <cpprest/json.h>
#include <mysql.h>
#include <sql_interface.h>
#include <utility.h>
#include <datastructures_go.h>
#include <datastructures_flat.h>
#include <datastructures_hierarchical.h>
#include <BSlogger.hpp>
#include <vector>

std::string sql_escape(MYSQL * conn, const std::string& s)
{
  char buffer[s.size() * 4];
  mysql_real_escape_string(conn, buffer, s.c_str(), s.size());
  return std::string(buffer);
}

sql_interface::sql_interface(web::json::value config, uint64_t bs) :
  bs(bs), conf(config), ctr(0)
{
  host = config.at("dbhost").as_string();
  user = config.at("dbuser").as_string();
  pass = config.at("dbpass").as_string();
  db = config.at("dbname").as_string();
  port = config.at("dbport").as_integer();
}

void sql_interface::query(MYSQL * conn, std::stringstream& ss)
{
  query(conn, ss.str());
  CLEAR_SS(ss);
}

MYSQL_RES * sql_interface::query_res(MYSQL * conn, std::stringstream& ss)
{
  MYSQL_RES * res = query_res(conn, ss.str());
  CLEAR_SS(ss);
  return res;
}

void sql_interface::query(MYSQL * conn, std::string&& query)
{
  logger log(std::cerr, "sql_interface::query");
  //log(LOG_DEBUG) << query << '\n';
  if (mysql_query(conn, query.c_str()))
  {
    throw std::runtime_error(mysql_error(conn));
  }
  ctr++;
  if (ctr % bs == 0)
  {
    // log(LOG_DEBUG) << "Committing " << ctr << " queries\n";
    commit(conn);
    ctr = 0;
  }
}

MYSQL_RES * sql_interface::query_res(MYSQL * conn, std::string&& query)
{
  logger log(std::cerr, "sql_interface::query_res");
  //log(LOG_DEBUG) << query << '\n';
  if (mysql_query(conn, query.c_str()))
  {
    throw std::runtime_error(mysql_error(conn));
  }
  return mysql_store_result(conn);
}

uint64_t sql_interface::get_tbl_hash(MYSQL * conn, std::string tbl_name)
{
  std::stringstream qbuilder;
  qbuilder << "SELECT tblhash FROM tbl_hashes WHERE tblname = '"
           << sql_escape(conn, tbl_name) << "';";
  MYSQL_RES * res = query_res(conn, qbuilder);
  my_ulonglong nres = mysql_num_rows(res);
  if (nres > 1)
  {
    throw std::runtime_error("Duplicate keys found for table: " + tbl_name);
  }
  if (nres == 1)
  {
    MYSQL_ROW row = mysql_fetch_row(res);
    uint64_t ret = std::stoul(row[0]);
    mysql_free_result(res);
    return ret;
  }
  else
  {
    mysql_free_result(res);
    return 0;
  }
}

void sql_interface::update_annotation_db(annotation& ann, std::string engine,
    bool force)
{

  logger log(std::cerr, "sql_interface::update_annotation_db");
  MYSQL * conn = get_conn();

  query(conn, "CREATE TABLE IF NOT EXISTS tbl_hashes("
        "tblname VARCHAR(255), "
        "tblhash BIGINT UNSIGNED"
        ");");

  mysql_autocommit(conn, 0);

  web::json::array arr = conf["annotation"].as_array();
  for (web::json::value& val : arr)
  {
    std::stringstream ss;
    std::string type = ann.get_type(val["name"].as_string());
    std::string tbbase = val["name"].as_string();
    log.add_snapshot(tbbase);
    if (type == "go")
    {
      go& curr_go = ann.go_by_name(val["name"].as_string());;
      uint64_t hash = curr_go.get_hash();
      uint64_t stored_hash = get_tbl_hash(conn, tbbase);
      // Early exit if we don't need to do anything
      if (stored_hash != 0 && hash == stored_hash)
      {
        log(LOG_INFO) << "Nothing new for " << tbbase << " [skip]\n";
        continue;
      }
      if (force)
      {
        log(LOG_INFO) << "Force rebuild. Dropping tables " << tbbase << '\n';
        ss << "DROP TABLE IF EXISTS " << tbbase << ";";
        query(conn, ss);
        ss << "DROP TABLE IF EXISTS " << tbbase << "_hierarchy;";
        query(conn, ss);
        commit(conn);
      }
      log(LOG_INFO) << "Rebuilding annotation tables for " << tbbase << '\n';
      ss << "CREATE TABLE IF NOT EXISTS "
         << tbbase << "("
         << "term VARCHAR(128), "
         << "summary VARCHAR(4096), "
         << "description TEXT, "
         << "namespace CHAR(2) "
         << ") ENGINE = " << engine << ";";
      query(conn, ss);
      ss << "CREATE TABLE IF NOT EXISTS "
         << tbbase << "_hierarchy ("
         << "term VARCHAR(128), "
         << "parent VARCHAR(128)"
         << ") ENGINE = " << engine << ";";
      query(conn, ss);

      ss << "TRUNCATE " << tbbase << ";";
      query(conn, ss);

      ss << "TRUNCATE " << tbbase << "_hierarchy;";
      query(conn, ss);

      std::vector<std::string> terms;
      for (auto& t : curr_go.bp())
      {
        terms.push_back(t.first);
      }
      for (auto& t : curr_go.mf())
      {
        terms.push_back(t.first);
      }
      for (auto& t : curr_go.cc())
      {
        terms.push_back(t.first);
      }

      for (const auto& t : terms)
      {
        auto& g = curr_go.get(t);
        ss << "INSERT INTO " << tbbase
           << " VALUES ("
           << "'" << sql_escape(conn, g.id()) << "', "
           << "'" << sql_escape(conn, g.name()) << "', "
           << "'" << sql_escape(conn, g.def()) << "', "
           << "'" << sql_escape(conn, g.nspace_s()) << "'"
           << ");";
        query(conn, ss);
        for (auto& p : curr_go.parents_of(g.id()))
        {
          ss << "INSERT INTO " << tbbase << "_hierarchy "
             << "VALUES ("
             << "'" << sql_escape(conn, g.id()) << "', "
             << "'" << sql_escape(conn, p) << "' "
             << ");";
          query(conn, ss);
        }
      }
      log(LOG_INFO) << "Building indices\n";
      // Finally we index relevant cols
      ss << "CREATE UNIQUE INDEX IF NOT EXISTS "
         << "ix_" << tbbase << " "
         << "ON " << tbbase << " (term);";
      query(conn, ss);

      ss << "CREATE INDEX IF NOT EXISTS "
         << "ix_" << tbbase << "_hierarchy "
         << "ON " << tbbase << "_hierarchy (term);";
      query(conn, ss);

      ss << "CREATE INDEX IF NOT EXISTS "
         << "ix_" << tbbase << "_hierarchy2 "
         << "ON " << tbbase << "_hierarchy (parent);";
      query(conn, ss);

      ss << "INSERT INTO tbl_hashes VALUES ("
         << "'" << sql_escape(conn, tbbase) << "', "
         << hash
         << ");";
      query(conn, ss);
      // Final commit for rest of data
      commit(conn);
      log.time_since_last_snap();
    }
    else if (type == "flat")
    {
      log.add_snapshot(tbbase);
      flat& curr_flat = ann.flat_by_name(val["name"].as_string());;
      uint64_t hash = curr_flat.get_hash();
      uint64_t stored_hash = get_tbl_hash(conn, tbbase);
      // Early exit if we don't need to do anything
      if (stored_hash != 0 && hash == stored_hash)
      {
        log(LOG_INFO) << "Nothing new for " << tbbase << " [skip]\n";
        continue;
      }
      if (force)
      {
        log(LOG_INFO) << "Force rebuild. Dropping tables " << tbbase << '\n';
        ss << "DROP TABLE IF EXISTS " << tbbase << ";";
        query(conn, ss);
        commit(conn);
      }
      log(LOG_INFO) << "Rebuilding annotation tables for " << tbbase << '\n';
      ss << "CREATE TABLE IF NOT EXISTS "
         << tbbase << "("
         << "term VARCHAR(128), "
         << "summary VARCHAR(4096), "
         << "description TEXT "
         << ") ENGINE = " << engine << ";";
      query(conn, ss);

      ss << "TRUNCATE " << tbbase << ";";
      query(conn, ss);

      for (auto& kv : curr_flat.terms())
      {
        auto& g = kv.second;
        ss << "INSERT INTO " << tbbase
           << " VALUES ("
           << "'" << sql_escape(conn, g.id()) << "', "
           << "'" << sql_escape(conn, g.name()) << "', "
           << "'" << sql_escape(conn, g.def()) << "' "
           << ");";
        query(conn, ss);
      }
      ss << "INSERT INTO tbl_hashes VALUES ("
         << "'" << sql_escape(conn, tbbase) << "', "
         << hash
         << ");";
      query(conn, ss);
      commit(conn);
      log.time_since_last_snap();
    }
    else if (type == "hierarchical")
    {
      hierarchical& curr_hierarchical =
        ann.hierarchical_by_name(val["name"].as_string());
      uint64_t hash = curr_hierarchical.get_hash();
      uint64_t stored_hash = get_tbl_hash(conn, tbbase);
      // Early exit if we don't need to do anything
      if (stored_hash != 0 && hash == stored_hash)
      {
        log(LOG_INFO) << "Nothing new for " << tbbase << " [skip]\n";
        continue;
      }
      if (force)
      {
        log(LOG_INFO) << "Force rebuild. Dropping tables " << tbbase << '\n';
        ss << "DROP TABLE IF EXISTS " << tbbase << ";";
        query(conn, ss);
        ss << "DROP TABLE IF EXISTS " << tbbase << "_hierarchy;";
        query(conn, ss);
        commit(conn);
      }
      log(LOG_INFO) << "Rebuilding annotation tables for " << tbbase << '\n';
      ss << "CREATE TABLE IF NOT EXISTS "
         << tbbase << "("
         << "term VARCHAR(128), "
         << "summary VARCHAR(4096), "
         << "description TEXT "
         << ") ENGINE = " << engine << ";";
      query(conn, ss);
      ss << "CREATE TABLE IF NOT EXISTS "
         << tbbase << "_hierarchy ("
         << "term VARCHAR(128), "
         << "parent VARCHAR(128)"
         << ") ENGINE = " << engine << ";";
      query(conn, ss);

      ss << "TRUNCATE " << tbbase << ";";
      query(conn, ss);

      ss << "TRUNCATE " << tbbase << "_hierarchy;";
      query(conn, ss);

      for (auto& kv : curr_hierarchical.terms())
      {
        auto& g = kv.second;
        ss << "INSERT INTO " << tbbase
           << " VALUES ("
           << "'" << sql_escape(conn, g.id()) << "', "
           << "'" << sql_escape(conn, g.name()) << "', "
           << "'" << sql_escape(conn, g.def()) << "' "
           << ");";
        query(conn, ss);
        for (auto& p : curr_hierarchical.parents_of(g.id()))
        {
          ss << "INSERT INTO " << tbbase << "_hierarchy "
             << "VALUES ("
             << "'" << sql_escape(conn, g.id()) << "', "
             << "'" << sql_escape(conn, p) << "' "
             << ");";
          query(conn, ss);
        }
      }
      log(LOG_INFO) << "Building indices\n";
      // Finally we index relevant cols
      ss << "CREATE UNIQUE INDEX IF NOT EXISTS "
         << "ix_" << tbbase << " "
         << "ON " << tbbase << " (term);";
      query(conn, ss);
      ss << "CREATE INDEX IF NOT EXISTS "
         << "ix_" << tbbase << "_hierarchy "
         << "ON " << tbbase << "_hierarchy (term);";
      query(conn, ss);
      ss << "CREATE INDEX IF NOT EXISTS "
         << "ix_" << tbbase << "_hierarchy2 "
         << "ON " << tbbase << "_hierarchy (parent);";
      query(conn, ss);
      ss << "INSERT INTO tbl_hashes VALUES ("
         << "'" << sql_escape(conn, tbbase) << "', "
         << hash
         << ");";
      query(conn, ss);
      // Final commit for rest of data
      commit(conn);
      log.time_since_last_snap();
    }
  }

  mysql_autocommit(conn, 1);
  mysql_close(conn);
}

void sql_interface::update_mapping_db(annotation& ann, std::string engine,
                                      bool force)
{
  logger log(std::cerr, "sql_interface::update_mapping_db");
  MYSQL * conn = get_conn();

  std::stringstream qb;

  mysql_autocommit(conn, 0);
  web::json::array arr = conf["org"].as_array();
  for (web::json::value& org : arr)
  {
    enrichment enr;
    enr.from_json(org, ann);
    std::string base = org["uri"].as_string();

    for (auto& e : enr.mapping_gos)
    {
      std::string map_name = base + "_" + e.first;
      std::string p_anno = enr.mapping_opt_gos[e.first].annotation + "_hierarchy";
      uint64_t hash = e.second.get_hash();
      uint64_t stored_hash = get_tbl_hash(conn, map_name);
      // Early exit if we don't need to do anything
      if (stored_hash != 0 && hash == stored_hash)
      {
        log(LOG_INFO) << "Nothing new for " << map_name << " [skip]\n";
        continue;
      }
      if (force)
      {
        log(LOG_INFO) << "Force rebuild. Dropping tables " << map_name << '\n';
        qb << "DROP TABLE IF EXISTS " << map_name << ";";
        query(conn, qb);
        qb << "DROP TABLE IF EXISTS " << map_name << "_truepath;";
        query(conn, qb);
        commit(conn);
      }
      log(LOG_INFO) << "Rebuilding mapping tables for " << map_name << '\n';
      qb << "CREATE TABLE IF NOT EXISTS "
         << map_name << " ("
         << "gene VARCHAR(128), "
         << "term VARCHAR(128)"
         << ") ENGINE = " << engine << ";";
      query(conn, qb);
      qb << "CREATE TABLE IF NOT EXISTS "
         << map_name << "_truepath ("
         << "gene VARCHAR(128), "
         << "term VARCHAR(128), "
         << "parent VARCHAR(128) "
         << ") ENGINE = " << engine << ";";
      query(conn, qb);
      qb << "TRUNCATE " << map_name << ";";
      query(conn, qb);
      qb << "TRUNCATE " << map_name << "_truepath;";
      query(conn, qb);
      commit(conn);
      for (auto& gt : e.second.map())
      {
        for (const auto& value : gt.second)
        {
          qb << "INSERT INTO " << map_name
             << " VALUES ("
             << "'" << sql_escape(conn, gt.first) << "', "
             << "'" << sql_escape(conn, value) << "' "
             << ");";
          query(conn, qb);
        }
      }
      log(LOG_INFO) << "Making true path for each term\n";
      qb << "INSERT INTO " << map_name << "_truepath "
         << "SELECT "
         << map_name << ".gene, "
         << map_name << ".term, "
         << p_anno << ".parent "
         << "FROM " << map_name << " "
         << "JOIN " << p_anno
         << " ON "
         << map_name << ".term = " << p_anno << ".term";
      query(conn, qb);
      log(LOG_INFO) << "Building indices\n";
      // Finally we index relevant cols
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_gene_" << map_name << " "
         << "ON " << map_name << " (gene);";
      query(conn, qb);
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_term_" << map_name << " "
         << "ON " << map_name << " (term);";
      query(conn, qb);
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_gene_" << map_name << " "
         << "ON " << map_name << "_truepath (gene);";
      query(conn, qb);
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_term_" << map_name << " "
         << "ON " << map_name << "_truepath (term);";
      query(conn, qb);
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_parent_" << map_name << " "
         << "ON " << map_name << "_truepath (parent);";
      query(conn, qb);
      qb << "INSERT INTO tbl_hashes VALUES ("
         << "'" << sql_escape(conn, map_name) << "', "
         << hash
         << ");";
      query(conn, qb);
      commit(conn);
    }
    for (auto& e : enr.mapping_flats)
    {
      std::string map_name = base + "_" + e.first;
      uint64_t hash = e.second.get_hash();
      uint64_t stored_hash = get_tbl_hash(conn, map_name);
      // Early exit if we don't need to do anything
      if (stored_hash != 0 && hash == stored_hash)
      {
        log(LOG_INFO) << "Nothing new for " << map_name << " [skip]\n";
        continue;
      }
      if (force)
      {
        log(LOG_INFO) << "Force rebuild. Dropping tables " << map_name << '\n';
        qb << "DROP TABLE IF EXISTS " << map_name << ";";
        query(conn, qb);
        commit(conn);
      }
      log(LOG_INFO) << "Rebuilding mapping tables for " << map_name << '\n';
      qb << "CREATE TABLE IF NOT EXISTS "
         << map_name << " ("
         << "gene VARCHAR(128), "
         << "term VARCHAR(128)"
         << ") ENGINE = " << engine << ";";
      query(conn, qb);
      qb << "TRUNCATE " << map_name << ";";
      query(conn, qb);
      commit(conn);
      for (auto& gt : e.second.map())
      {
        for (const auto& value : gt.second)
        {
          qb << "INSERT INTO " << map_name
             << " VALUES ("
             << "'" << sql_escape(conn, gt.first) << "', "
             << "'" << sql_escape(conn, value) << "' "
             << ");";
          query(conn, qb);
        }
      }
      log(LOG_INFO) << "Building indices\n";
      // Finally we index relevant cols
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_gene_" << map_name << " "
         << "ON " << map_name << " (gene);";
      query(conn, qb);

      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_term_" << map_name << " "
         << "ON " << map_name << " (term);";
      query(conn, qb);
      qb << "INSERT INTO tbl_hashes VALUES ("
         << "'" << sql_escape(conn, map_name) << "', "
         << hash
         << ");";
      query(conn, qb);
      commit(conn);
    }
    for (auto& e : enr.mapping_hierarchicals)
    {
      std::string map_name = base + "_" + e.first;
      std::string p_anno = enr.mapping_opt_hierarchicals[e.first].annotation + "_hierarchy";
      uint64_t hash = e.second.get_hash();
      uint64_t stored_hash = get_tbl_hash(conn, map_name);
      // Early exit if we don't need to do anything
      if (stored_hash != 0 && hash == stored_hash)
      {
        log(LOG_INFO) << "Nothing new for " << map_name << " [skip]\n";
        continue;
      }
      if (force)
      {
        log(LOG_INFO) << "Force rebuild. Dropping tables " << map_name << '\n';
        qb << "DROP TABLE IF EXISTS " << map_name << ";";
        query(conn, qb);
        qb << "DROP TABLE IF EXISTS " << map_name << "_truepath;";
        query(conn, qb);
        commit(conn);
      }
      log(LOG_INFO) << "Rebuilding mapping tables for " << map_name << '\n';
      qb << "CREATE TABLE IF NOT EXISTS "
         << map_name << " ("
         << "gene VARCHAR(128), "
         << "term VARCHAR(128)"
         << ") ENGINE = " << engine << ";";
      query(conn, qb);
      qb << "CREATE TABLE IF NOT EXISTS "
         << map_name << "_truepath ("
         << "gene VARCHAR(128), "
         << "term VARCHAR(128), "
         << "parent VARCHAR(128) "
         << ") ENGINE = " << engine << ";";
      query(conn, qb);
      qb << "TRUNCATE " << map_name << ";";
      query(conn, qb);
      qb << "TRUNCATE " << map_name << "_truepath;";
      query(conn, qb);
      commit(conn);
      for (auto& gt : e.second.map())
      {
        for (const auto& value : gt.second)
        {
          qb << "INSERT INTO " << map_name
             << " VALUES ("
             << "'" << sql_escape(conn, gt.first) << "', "
             << "'" << sql_escape(conn, value) << "' "
             << ");";
          query(conn, qb);
        }
      }
      log(LOG_INFO) << "Making true path for each term\n";
      qb << "INSERT INTO " << map_name << "_truepath "
         << "SELECT "
         << map_name << ".gene, "
         << map_name << ".term, "
         << p_anno << ".parent "
         << "FROM " << map_name << " "
         << "JOIN " << p_anno
         << " ON "
         << map_name << ".term = " << p_anno << ".term";
      query(conn, qb);
      log(LOG_INFO) << "Building indices\n";
      // Finally we index relevant cols
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_gene_" << map_name << " "
         << "ON " << map_name << " (gene);";
      query(conn, qb);

      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_term_" << map_name << " "
         << "ON " << map_name << " (term);";
      query(conn, qb);
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_gene_" << map_name << " "
         << "ON " << map_name << "_truepath (gene);";
      query(conn, qb);
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_term_" << map_name << " "
         << "ON " << map_name << "_truepath (term);";
      query(conn, qb);
      qb << "CREATE INDEX IF NOT EXISTS "
         << "ix_parent_" << map_name << " "
         << "ON " << map_name << "_truepath (parent);";
      query(conn, qb);
      qb << "INSERT INTO tbl_hashes VALUES ("
         << "'" << sql_escape(conn, map_name) << "', "
         << hash
         << ");";
      query(conn, qb);
      commit(conn);
    }
  }
  mysql_autocommit(conn, 1);
  mysql_close(conn);
}

sql_mapping_result sql_interface::get_annotation(const web::json::value org,
    const std::unordered_set<std::string>& genes,
    const std::unordered_set<std::string>& population,
    std::string& target,
    std::string& anno)
{
  logger log(std::cerr, "sql_interface::update_mapping_db");
  MYSQL * conn = get_conn();
  std::stringstream qb;
  sql_mapping_result ret;

  std::string mapping = org.at("uri").as_string() + "_" + target;
  std::string truepath = mapping + "_truepath";
  for (const std::string& g : genes)
  {
    qb << "SELECT term,parent FROM "
       << truepath <<  " WHERE gene = '"
       << sql_escape(conn, g) << "';";
    MYSQL_RES * res = query_res(conn, qb);
    my_ulonglong nres = mysql_num_rows(res);
    for (my_ulonglong i = 0; i < nres; i++)
    {
      MYSQL_ROW row = mysql_fetch_row(res);
      ret.all_terms.insert(std::string(row[0]));
      ret.all_terms.insert(std::string(row[1]));
      ret.term_gene_ts[std::string(row[0])].insert(g);
      ret.term_gene_ts[std::string(row[1])].insert(g);
    }
    mysql_free_result(res);
  }
  for (const std::string& t : ret.all_terms)
  {
    qb << "SELECT DISTINCT gene FROM " << truepath << " "
       << "WHERE term = '" << sql_escape(conn, t) << "' OR "
       << "parent = '" << sql_escape(conn, t) << "';";
    MYSQL_RES * res = query_res(conn, qb);
    my_ulonglong nres = mysql_num_rows(res);
    for (my_ulonglong i = 0; i < nres; i++)
    {
      MYSQL_ROW row = mysql_fetch_row(res);
      std::string g(row[0]);
      if (population.find(g) != population.end())
      {
        ret.term_gene_pop[t].insert(std::string(row[0]));
      }
    }
    mysql_free_result(res);
    qb << "SELECT DISTINCT parent FROM " << anno << "_hierarchy "
       << "WHERE term = '" << sql_escape(conn, t) << "';";
    MYSQL_RES * res_l = query_res(conn, qb);
    nres = mysql_num_rows(res_l);
    for (my_ulonglong i = 0; i < nres; i++)
    {
      MYSQL_ROW row = mysql_fetch_row(res_l);
      ret.term_parents[t].insert(std::string(row[0]));
    }
    mysql_free_result(res_l);
  }
  mysql_close(conn);
  return ret;
}

std::vector<std::string>
sql_interface::get_parents(std::string ann, std::string term)
{
  std::vector<std::string> ret;
  MYSQL * conn = get_conn();
  std::stringstream qb;
  qb << "SELECT DISTINCT parent FROM " << ann << "_hierarchy "
     << "WHERE term = '" << sql_escape(conn, term) << "';";
  MYSQL_RES * res_l = query_res(conn, qb);
  my_ulonglong nres = mysql_num_rows(res_l);
  for (my_ulonglong i = 0; i < nres; i++)
  {
    MYSQL_ROW row = mysql_fetch_row(res_l);
    ret.push_back(std::string(row[0]));
  }
  mysql_free_result(res_l);
  mysql_close(conn);
  return ret;
}
