#pragma once

#include <string>
#include <unordered_set>
#include <cpprest/json.h>
#include <utility.h>

void handle_annotation(annotation& ann,
  std::unordered_set<std::string>& terms,
  web::json::value& ret,
  std::string t,
  const web::json::value conf);