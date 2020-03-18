#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <cpprest/json.h>
#include <cpprest/http_listener.h>
#include <BSlogger.hpp>
#include <term.h>
#include <utility.h>
#include <datastructures_go.h>
#include <datastructures_flat.h>
#include <datastructures_hierarchical.h>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <sql_interface.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

unsigned logger::_loglevel = LOG_TIME;
int main(int argc, char ** argv) {

  logger log(std::cerr, "Gofer3");

  std::string file_config;
  std::string engine;
  bool force;

  try
  {
    po::options_description desc("Create or update Gofer3 DB\n");
    desc.add_options()
    ("help,h", "Show this help message")
    ("engine,e", po::value<std::string>(&engine)->default_value(SQL_ENGINE), "SQL storage engine")
    ("force,f", po::bool_switch(&force)->default_value(false),
     "Force rebuilding of all tables")
    ("in-file", po::value<std::string>(&file_config)->required(), "Config file");

    po::positional_options_description p;
    p.add("in-file", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
              options(desc).positional(p).run(), vm);

    if (! vm.count("in-file"))
    {
      std::cerr << desc << '\n';
      return 1;
    }

    if (vm.count("help") != 0 || argc == 0)
    {
      std::cerr << desc << '\n';
      return 1;
    }

    po::notify(vm);
  }
  catch (std::exception& e)
  {
    log(LOG_ERR) << e.what() << '\n';
  }

  try
  {

    file_config = fs::canonical(fs::path(file_config)).string();

    web::json::value config = parse_config(file_config);

    annotation ann;
    ann.from_json(config);

    sql_interface sql(config);

    if (force)
    {
      MYSQL * conn = sql.get_conn();
      sql.query(conn, "TRUNCATE tbl_hashes;");
      sql.commit(conn);
      sql.close(conn);
    }

    sql.update_annotation_db(ann, engine, force);
    sql.update_mapping_db(ann, engine, force);
  }
  catch (std::system_error& e)
  {
    log(LOG_ERR) << e.what() << '\n';
    return e.code().value();
  }
  catch (std::exception& e)
  {
    log(LOG_ERR) << e.what() << '\n';
    return 1;
  }
  return 0;
}