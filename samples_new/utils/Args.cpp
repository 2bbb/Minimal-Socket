/**
 * Author:    Andrea Casalino
 * Created:   01.28.2020
 *
 * report any bug to andrecasa91@gmail.com.
 **/

#include "Args.h"

#include <sstream>
#include <stdexcept>

namespace MinimalSocket::samples {
std::optional<Args> Args::parse(const int argc, const char **argv) {
  std::optional<Args> result;
  try {
    Args parsed(argc, argv);
    result.emplace(std::move(parsed));
  } catch (...) {
  }
  return result;
}

Args::Args(const int argc, const char **argv) {
  if (((argc - 1) % 2) != 0) {
    throw std::runtime_error{""};
  }
  for (int k = 1; k < argc; k += 2) {
    std::string name = argv[k];
    if (name.size() < 3) {
      throw std::runtime_error{""};
    }
    if ((name[0] != '-') || (name[1] != '-')) {
      throw std::runtime_error{""};
    }

    name = std::string{name, 2};
    std::string value = argv[k + 1];
    if (name.empty()) {
      throw std::runtime_error{""};
    }

    arguments_map[name] = value;
  }

  for (const auto &[name, value] : arguments_map) {
    std::cout << "--" << name << ": " << value << std::endl;
  }
  std::cout << std::endl;
}

std::string Args::getValue(const std::string &argument_name,
                           const std::string &default_value) const {
  auto args_it = arguments_map.find(argument_name);
  return (args_it == arguments_map.end()) ? default_value : args_it->second;
}

std::string Args::getValue(const std::string &argument_name) const {
  auto args_it = arguments_map.find(argument_name);
  if (args_it == arguments_map.end()) {
    std::stringstream stream;
    stream << "--" << argument_name << " was not specififed";
    throw std::runtime_error{stream.str()};
  }
  return args_it->second;
}
} // namespace MinimalSocket::samples
