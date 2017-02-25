#include "program_option.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>

using namespace std;

void process_n(char const* n) {
  long d;
  cout << "-n: " << n << endl;
  if (sscanf(n, "%ld", &d) != 1) {
    throw option_parser::parse_error("invalid number");
  }
}

int main(int argc, char** argv) {
  option_parser p;
  p('h', "help", "print this useless message",
     [&p, argv](){
      cout << "Usage: " << argv[0] << p.usage() << endl;
      cout << p.description() << endl;
      exit(0);
    })

   ('n', "", "a number", process_n)

   (0, "num", "another number\nthis line is supposed to explain what it does",
     [](char const* n) { cout << "--num: " << n << endl; })

   ('v', "", "print more useless messages than usual",
    [](){ cout << "-v set\n"; })

   (0, "undocumented", "",
    [](){ cout << "--undocumented set\n"; })

   ("First-arg", "required argument",
    [](char const* n) { cout << "First argument: " << n << endl; })

   ("Second-arg", "mandatory argument",
    [](char const* n) { cout << "Second argument: " << n << endl; })

   .defaults_now_optional()
   ("Next-args", "optional arguments",
    [](char const* n) { cout << "Next argument: " << n << endl; })
   ;

  vector <option_parser::parse_error> errs = p.parse_argv(argc, argv);
  for (auto err : errs) {
    if (err.opt_name.empty()) {
      cerr << "Error parsing command line: " << err.message << endl;
    } else {
      cerr << "Error parsing " << err.opt_name << ": " << err.message << endl;
    }
  }
}
