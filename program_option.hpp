#pragma once
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

struct option_parser {
  struct parse_opt_base {
    char short_name;
    std::string long_name;
    std::string description;
    bool has_arg;
    parse_opt_base(char short_name, std::string const& long_name, std::string const& description):
      short_name(short_name), long_name(long_name), description(description), has_arg(false)
    { }
    virtual void parse(char const* arg) const = 0;
    virtual ~parse_opt_base() = default;
  };

  template <typename T>
  struct parse_opt : parse_opt_base {
    T func;

    parse_opt(char short_name, std::string const& long_name,
              std::string const& description, T const& func):
      parse_opt_base(short_name, long_name, description), func(func)
    { }

    virtual void parse(char const* arg) const {
      func();
    }
  };

  template <typename T>
  struct parse_opt_arg : parse_opt_base {
    T func;

    parse_opt_arg(char short_name, std::string const& long_name,
                  std::string const& description, T const& func):
      parse_opt_base(short_name, long_name, description), func(func)
    {
      has_arg = true;
    }

    virtual void parse(char const* arg) const {
      func(arg);
    }
  };

  struct parse_default_base {
    std::string name, description;
    parse_default_base(std::string const& name, std::string const& description):
      name(name), description(description)
    { }
    virtual void parse(char const* arg) const = 0;
    virtual ~parse_default_base() = default;
  };
  template <typename T>
  struct parse_default : parse_default_base {
    T func;
    parse_default(std::string const& name, std::string const& description, T const& func):
      parse_default_base(name, description), func(func)
    { }
    virtual void parse(char const* arg) const {
      func(arg);
    }
  };

  struct parse_error {
    std::string message;
    bool fatal;
    std::string opt_name;
    parse_error(std::string const& message, bool fatal = false):
      parse_error(message, fatal, "")
    { }
    parse_error(std::string const& message, bool fatal, std::string const& opt_name):
      message(message), fatal(fatal), opt_name(opt_name)
    { }
  };

  std::vector <parse_opt_base*> opts;
  std::vector <parse_default_base*> default_opts;
  bool defaults_are_now_optional;
  unsigned num_required_defaults;

  option_parser():
    defaults_are_now_optional(false), num_required_defaults(0)
  { }
  option_parser& defaults_now_optional() {
    defaults_are_now_optional = true;
    return *this;
  }

protected:
  template <typename T, typename U>
  parse_opt_base* make_parse_opt_helper(char short_name, std::string const& long_name, std::string const& description, T const& func, U (T::*)() const) {
    return new parse_opt <T> (short_name, long_name, description, func);
  }
  template <typename T, typename U>
  parse_opt_base* make_parse_opt_helper(char short_name, std::string const& long_name, std::string const& description, T const& func, U (T::*)(char const*) const) {
    return new parse_opt_arg <T> (short_name, long_name, description, func);
  }
public:

  template <typename T>
  option_parser& operator()(char short_name, std::string const& long_name, std::string const& description, T const& func) {
    // resize opts first for exception safety
    opts.push_back(nullptr);
    opts.back() = make_parse_opt_helper(short_name, long_name, description, func, &T::operator());
    return *this;
  }
  option_parser& operator()(char short_name, std::string const& long_name, std::string const& description, void (*func) ()) {
    opts.push_back(nullptr);
    opts.back() = new parse_opt <void (*) ()> (short_name, long_name, description, func);
    return *this;
  }
  option_parser& operator()(char short_name, std::string const& long_name, std::string const& description, void (*func) (char const*)) {
    opts.push_back(nullptr);
    opts.back() = new parse_opt_arg <void (*) (char const*)> (short_name, long_name, description, func);
    return *this;
  }
  template <typename T>
  option_parser& operator()(std::string const& name, std::string const& description, T const& func) {
    default_opts.push_back(nullptr);
    default_opts.back() = new parse_default <T> (name, description, func);
    if (!defaults_are_now_optional) {
      ++num_required_defaults;
    }
    return *this;
  }

protected:
  // -1: no match, 0: --opt only, n: val position in --opt=val
  static int match_long_opt(char const* arg, std::string const& opt_name) {
    if (!std::strncmp(arg, opt_name.c_str(), opt_name.size())) {
      if (arg[opt_name.size()] == '=') {
        return opt_name.size() + 1;
      } else if (arg[opt_name.size()] == '\0') {
        return 0;
      }
    }
    return -1;
  }
public:

  /*
    TODO:
    - coalesced short options e.g. -abc for -a -b -c
    - refactor and optimise
    - use getopt as backend?
   */
  std::vector <parse_error> parse_argv(int argc, char ** argv) const {
    std::vector <parse_error> parse_errors;
    unsigned default_pos = 0;
    for (int i = 1; i < argc; ++i) {
      bool not_opt = false;
      if (argv[i][0] == '-') {
        if (argv[i][1] == '-') {
          bool found = false;
          for (parse_opt_base * p : opts) {
            int match = match_long_opt(&argv[i][2], p->long_name);
            if (match != -1) {
              if (match > 0) {
                if (!p->has_arg) {
                  parse_errors.push_back(parse_error(
                    "Unnecessary value", false, "--" + p->long_name));
                } else {
                  try {
                    p->parse(&argv[i][match+2]);
                  } catch (parse_error err) {
                    err.opt_name = "--" + p->long_name;
                    parse_errors.push_back(err);
                    if (err.fatal) {
                      return parse_errors;
                    }
                  }
                }
              } else {
                try {
                  if (!p->has_arg) {
                    p->parse(argv[i]);
                  } else if (i+1 < argc) {
                    p->parse(argv[i+1]);
                    ++i;
                  } else {
                    parse_errors.push_back(parse_error(
                      "Missing value", false, "--" + p->long_name));
                  }
                } catch (parse_error err) {
                  err.opt_name = "--" + p->long_name;
                  parse_errors.push_back(err);
                  if (err.fatal) {
                    return parse_errors;
                  }
                }
              }
              found = true;
              break;
            }
          }
          if (!found) {
            parse_errors.push_back(parse_error(
              "Unrecognized option", false, argv[i]));
          }
        } else if (argv[i][1] != '\0') {
          bool found = false;
          for (parse_opt_base * p : opts) {
            if (argv[i][1] == p->short_name) {
              if (argv[i][2] != '\0') {
                if (!p->has_arg) {
                  parse_errors.push_back(parse_error(
                    "Extraneous value", false, std::string("-") + p->short_name));
                } else {
                  try {
                    p->parse(&argv[i][2]);
                  } catch (parse_error err) {
                    err.opt_name = std::string("-") + p->short_name;
                    parse_errors.push_back(err);
                    if (err.fatal) {
                      return parse_errors;
                    }
                  }
                }
              } else {
                try {
                  if (!p->has_arg) {
                    p->parse(argv[i]);
                  } else if (i+1 < argc) {
                    p->parse(argv[i+1]);
                    ++i;
                  } else {
                    parse_errors.push_back(parse_error(
                      "Missing value", false, std::string("-") + p->short_name));
                  }
                } catch (parse_error err) {
                  err.opt_name = std::string("-") + p->short_name;
                  parse_errors.push_back(err);
                  if (err.fatal) {
                    return parse_errors;
                  }
                }
              }
              found = true;
              break;
            }
          }
          if (!found) {
            parse_errors.push_back(parse_error(
              "Unrecognized option", false, argv[i]));
          }
        } else {
          not_opt = true;
        }
      } else {
        not_opt = true;
      }
      if (not_opt) {
        if (default_pos < default_opts.size()) {
          try {
            default_opts[default_pos]->parse(argv[i]);
          } catch (parse_error err) {
            parse_errors.push_back(err);
            if (err.fatal) {
              return parse_errors;
            }
          }
          if (default_pos + 1 < default_opts.size()) {
            ++default_pos;
          }
        } else {
          parse_errors.push_back(parse_error(
            std::string("Unexpected argument: ") + argv[i], false));
        }
      }
    }
    if (default_pos < num_required_defaults) {
      parse_errors.push_back(parse_error(
        "Missing positional argument", false, default_opts[default_pos]->name));
    }
    return parse_errors;
  }

  // this omits the "Usage: PROGNAME" prefix
  std::string usage() const {
    std::string usage;
    if (!opts.empty()) {
      usage += " [options...]";
    }
    if (!default_opts.empty()) {
      for (unsigned i = 0; i < num_required_defaults; ++i) {
        usage += ' ';
        usage += default_opts[i]->name;
      }
      for (unsigned i = num_required_defaults; i < default_opts.size(); ++i) {
        usage += " [";
        usage += default_opts[i]->name;
      }
      for (unsigned i = num_required_defaults; i < default_opts.size(); ++i) {
        usage += ']';
      }
    }
    return usage;
  }

  // Print table of all option and argument descriptions.
  // Those with empty descriptions are omitted.
  // TODO: this should output to a stream
  std::string description() const {
    std::string desc;
    size_t long_opt_len = 0;
    // if there are options and args, need to separate them
    bool need_separator = false;
    std::string const opt_arg_separator = "\n";

    // append n spaces
    auto add_indent = [](std::string& s, size_t n) -> void {
      for (size_t i = 0; i < n; ++i) {
        s += ' ';
      }
    };
    // append indented text
    auto add_desc = [&add_indent](std::string& s, std::string const& text, size_t indent) -> void {
      for (char c : text) {
        s += c;
        if (c == '\n') {
          add_indent(s, indent);
        }
      }
    };

    // --options
    for (parse_opt_base const* p : opts) {
      if (p->description.empty()) {
        continue;
      }
      long_opt_len = std::max(long_opt_len, p->long_name.size());
    }
    for (parse_opt_base const* p : opts) {
      if (p->description.empty()) {
        continue;
      }
      size_t size0 = desc.size();
      if (p->short_name) {
        desc += " -";
        desc += p->short_name;
      } else {
        desc += "   ";
      }

      if (!p->long_name.empty()) {
        desc += " --";
      } else {
        desc += "   ";
      }
      desc += p->long_name;
      add_indent(desc, long_opt_len - p->long_name.size());
      desc += "  ";

      size_t indent = desc.size() - size0;
      add_desc(desc, p->description, indent);
      desc += '\n';

      need_separator = true;
    }

    // arguments
    size_t long_arg_len = 0;
    for (parse_default_base const* arg: default_opts) {
      if (arg->description.empty()) {
        continue;
      }
      long_arg_len = std::max(long_arg_len, arg->name.size());
    }
    for (parse_default_base const* arg: default_opts) {
      if (arg->description.empty()) {
        continue;
      }
      // separate from options block
      if (need_separator) {
        desc += opt_arg_separator;
        need_separator = false; // done
      }

      size_t size0 = desc.size();
      desc += "  ";
      desc += arg->name;
      add_indent(desc, long_arg_len - arg->name.size());
      desc += "  ";

      size_t indent = desc.size() - size0;
      add_desc(desc, arg->description, indent);
      desc += '\n';
    }
    return desc;
  }

  ~option_parser() {
    for (parse_opt_base* p : opts) {
      delete p;
    }
    for (parse_default_base* p : default_opts) {
      delete p;
    }
  }
};
