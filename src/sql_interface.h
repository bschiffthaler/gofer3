#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <cpprest/json.h>
#include <mysql.h>
#include <utility.h>
#include <datastructures_go.h>
#include <datastructures_flat.h>
#include <datastructures_hierarchical.h>

#define CLEAR_SS(ss) ss.str(""); ss.clear();
#define SQL_BATCHSIZE 1024
#define SQL_ENGINE "InnoDB"

typedef std::unordered_map<std::string, std::unordered_set<std::string>> map_set;

struct sql_mapping_result
{
  std::unordered_set<std::string> all_terms;
  map_set term_parents; // Parental relationships for terms (==hierarchy)
  map_set term_gene_ts; // Genes annotated to term x in test set
  map_set term_gene_pop; // Same as above for population
};

std::string sql_escape(MYSQL * conn, const std::string& s);

class sql_interface
{
public:
  sql_interface(web::json::value config, uint64_t bs = SQL_BATCHSIZE);
  inline void close(MYSQL * conn)
  {
    mysql_close(conn);
  }
  inline void commit(MYSQL * conn)
  {
    //logger log(std::cerr, "sql_interface::query");
    //log(LOG_DEBUG) << query << '\n';
    if (mysql_commit(conn))
    {
      throw std::runtime_error(mysql_error(conn));
    }
  }
  inline MYSQL * get_conn()
  {
    MYSQL * db_handle = mysql_init(NULL);
    return mysql_real_connect(db_handle, host.c_str(), user.c_str(),
                              pass.c_str(), db.c_str(), port, NULL, 0);
  }
  void update_annotation_db(annotation& ann, std::string engine = SQL_ENGINE,
    bool force = false);
  void update_mapping_db(annotation& ann, std::string engine = SQL_ENGINE,
    bool force = false);
  void query(MYSQL * conn, std::string&& query);
  void query(MYSQL * conn, std::stringstream& ss);
  MYSQL_RES * query_res(MYSQL * conn, std::string&& query);
  MYSQL_RES * query_res(MYSQL * conn, std::stringstream& ss);
  uint64_t get_tbl_hash(MYSQL * conn, std::string tbl_name);
  sql_mapping_result
  get_annotation(const web::json::value org,
                 const std::unordered_set<std::string>& genes,
                 const std::unordered_set<std::string>& population,
                 std::string& target,
                 std::string& anno);
  std::vector<std::string> get_parents(std::string ann, std::string term);
protected:
  std::string db;
  std::string host;
  std::string user;
  std::string pass;
  unsigned int port;
  web::json::value conf;
  uint64_t ctr;
  uint64_t bs;
};



// -> SQL query, count unique genes
// g annotated to parents of t globally
//   1. get parents of t
//   2. join
