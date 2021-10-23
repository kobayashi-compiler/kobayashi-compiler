#pragma once

#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>

using MemSize = uint64_t;

constexpr MemSize INT_SIZE = 4;

int32_t concat(int32_t bottom, int32_t top);

bool startswith(const std::string &s1, const std::string &s2);

// parse [0-9]+ | '0x'[0-9a-fA-F]+ | '0X'[0-9a-fA-F]+
int32_t parse_int32_literal(const std::string &s);

std::string mangle_global_var_name(const std::string &s);

void unreachable();

struct Configuration {
  static constexpr int DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3;
  int log_level;
  bool simulate_exec;
  std::string input;
  std::set<std::string> disabled_passes;
  std::map<std::string, std::string> args;

  Configuration();  // for default setting
  std::string get_arg(std::string key, std::string default_value);
  int get_int_arg(std::string key, int default_value);
};

extern Configuration global_config;

// set global_config, return <input file, output file>
std::pair<std::string, std::string> parse_arg(int argc, char *argv[]);

template <int level>
struct LogStream {
  template <class T>
  LogStream &operator<<(const T &msg) {
    if (global_config.log_level <= level) std::cerr << msg;
    return *this;
  }
};

extern LogStream<Configuration::DEBUG> debug;
extern LogStream<Configuration::INFO> info;
extern LogStream<Configuration::WARNING> warning;
extern LogStream<Configuration::ERROR> error;
