#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <regex>

#include <boost/filesystem.hpp>

#include "defaults.hpp"
#include <clang/Tooling/JSONCompilationDatabase.h>

namespace color_coded
{
  namespace conf
  {
    namespace fs = boost::filesystem;

    namespace detail
    {
      inline std::string make_absolute(std::string line, fs::path const &base)
      {
        static std::regex const reg
        {
          "\\s*(-I|-isystem|-iquote|--sysroot=)\\s*(.*)",
          std::regex::optimize
        };
        static std::smatch match;
        if(std::regex_search(line, match, reg))
        {
          auto const &str(match[2].str());
          if(str.size() && str[0] != '/')
          { line = match[1].str() + fs::absolute(str, base).string(); }
        }

        return line;
      }
    }

    inline args_t load_compilation_database(std::string const &file, std::string filename)
    {
      std::string error;
      auto const database_ptr(::clang::tooling::JSONCompilationDatabase::loadFromFile(file, error));
      if(!database_ptr)
      { return {}; }

      auto compile_commands(database_ptr->getCompileCommands(filename));
      if(compile_commands.empty())
      {
        auto const ext = fs::path(filename).extension();
        if(!(ext == ".h" || ext == ".hpp"))
        { return {}; }

        auto const alternative_c = fs::path(filename).replace_extension("c");
        auto const alternative_cpp = fs::path(filename).replace_extension("cpp");

        compile_commands = database_ptr->getCompileCommands(alternative_c.string());
        filename = alternative_c.string();
        if(compile_commands.empty())
        {
          compile_commands = database_ptr->getCompileCommands(alternative_cpp.string());
          filename = alternative_cpp.string();
        }

        if(compile_commands.empty())
        { return {}; }
      }

      auto commands(compile_commands[0].CommandLine);
      commands.erase(std::remove(commands.begin(), commands.end(), filename), commands.end());

      return commands;
    }

    inline args_t load_color_coded(std::string const &file, std::string const &filetype)
    {
      std::ifstream ifs{ file };
      if(!ifs.is_open())
      { return {}; }

      auto const pre_additions(pre_constants(filetype));
      args_t args{ pre_additions };

      auto const &base(fs::path{ file }.parent_path());
      std::string tmp;
      while(std::getline(ifs, tmp))
      { args.emplace_back(detail::make_absolute(std::move(tmp), base)); }

      return args;
    }

    inline args_t load(std::string const &file, std::string const &filetype, std::string const &filename)
    {
      if(file.empty())
      { return defaults(filetype); }

      static auto const post_additions(post_constants());

      args_t args;
      if (fs::path(file).filename() == "compile_commands.json")
      { args = load_compilation_database(file, filename); }
      else
      { args = load_color_coded(file, filetype); }

      if(args.empty())
      { return defaults(filetype); }

      std::copy(post_additions.begin(), post_additions.end(),
                std::back_inserter(args));

      return args;
    }

    inline args_t load(std::string const &file, std::string const &filetype)
    { return load(file, filetype, "");}
  }
}
