#ifndef READPARAM_H
#define READPARAM_H

#include <climits>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// Analyze the PARAM.in parameter string. Example:
// input: 1E-2*1e1 week       output: 60480
inline double analyze_string(std::string &s) {
  double res = 0, v1 = 0, v2 = 0;

  char operation = 'A';

  std::stringstream ss(s);
  if (!(ss >> v1)) {
    std::cout << "Error: cannot parse the string to a number!" << std::endl;
    std::abort();
  }
  res = v1;

  if (ss.good())
    ss >> operation;

  if (ss.good()) {
    if (ss >> v2) {
      if (operation == '+') {
        res = v1 + v2;
      } else if (operation == '-') {
        res = v1 - v2;
      } else if (operation == '*') {
        res = v1 * v2;
      } else if (operation == '/') {
        res = v1 / v2;
      }
    }
  }

  // Assume res is time.
  if (s.find(" m") != std::string::npos) {
    res *= 60;
  } else if (s.find(" h") != std::string::npos) {
    res *= 3600;
  } else if (s.find(" d") != std::string::npos) {
    res *= 3600 * 24;
  } else if (s.find(" w") != std::string::npos) {
    res *= 3600 * 24 * 7;
  } else if (s.find(" y") != std::string::npos) {
    res *= 3600 * 24 * 365.25;
  } else if (s.find(" UT") != std::string::npos) {
    std::cout << "Error: UT is not supported! " << std::endl;
    std::abort();
  }

  return res;
}

class ReadParam {
private:
  std::stringstream ss;
  std::string param;
  bool isVerbose;

  std::string commandSuffix = std::string();

  std::string component = "PC";

public:
  ReadParam() { isVerbose = true; }
  ~ReadParam() {}
  ReadParam &operator=(const std::string &stringIn) {
    param = stringIn;
    ss.clear();
    ss.str(param);
    return (*this);
  }
  ReadParam &operator=(const ReadParam &other) { return (*this); }

  void roll_back() {
    ss.clear();
    ss.str(param);
  }

  void set_verbose(bool in) { isVerbose = in; }

  void set_component(const std::string &in) { component = in; }

  void set_command_suffix(const std::string &in) { commandSuffix = in; }

  inline bool get_next_command(std::string &id) {
    id.clear();
    ss.ignore(INT_MAX, '#');
    while (ss.good()) {
      ss.unget();

      // Example of commandline:
      // 1) #COMMAND
      // 2) #COMMAND_GRID0_GRID1

      std::string commandline;
      ss >> commandline;
      std::string::size_type pos1;
      // Check if this command has suffixes
      pos1 = commandline.find_first_of("_");
      id = commandline.substr(0, pos1);

      if (pos1 != std::string::npos) {
        std::string tmp = "_" + commandSuffix;
        if (commandline.find(tmp) == std::string::npos) {
          // Go to next command.
          ss.ignore(INT_MAX, '#');
          continue;
        }
      }
      if (isVerbose)
        std::cout << "\n"
                  << component << ": " << id << " " << commandSuffix
                  << std::endl;
      ss.ignore(INT_MAX, '\n');
      return true;
    }

    return false;
  }
  //==============================

  void read_var(std::string description, double &var) {
    std::string sVar;
    read_var(description, sVar, false);
    var = analyze_string(sVar);
    if (isVerbose) {
      std::cout << component << ": " << std::left << std::setw(40) << var
                << description << std::endl;
      return;
    }
  }

  void read_var(std::string description, float &var) {
    double dd;
    read_var(description, dd);
    var = (float)dd;
  }

  void read_var(std::string description, int &var) {
    double dd;
    read_var(description, dd);
    var = (int)dd;
  }

  template <class T> void read_var(std::string description, T &var) {

    if (ss.good())
      ss >> var; // If ss is empty, ss.good() is still true.

    if (ss.good()) {
      // If ss is empty, ss.good() is false now.
      // If ss is the last line, ss.good() is still true if there is a
      // '\n' at the end of each line.
      ss.ignore(INT_MAX, '\n');
      if (isVerbose) {
        std::cout << component << ": " << std::left << std::setw(40) << var
                  << description << std::endl;
        return;
      }
    } else {
      if (isVerbose)
        std::cout << "Type T: can not find input parameter. Abort!"
                  << std::endl;
      std::abort();
    }
  }
  //=======================================================

  inline void read_var(std::string description, bool &var) {
    std::string text;

    if (ss.good())
      ss >> text;

    if (ss.good()) {
      ss.ignore(INT_MAX, '\n');
      var = false;
      if (text == "T")
        var = true;
      if (isVerbose) {
        std::cout << component << ": " << std::left << std::setw(40) << text
                  << description << std::endl;
        return;
      }
    } else {
      if (isVerbose)
        std::cout << "Bool: can not find input parameter. Abort!" << std::endl;
      std::abort();
    }
  }
  //==========================================================

  inline void read_var(std::string description, std::string &var,
                       bool doAllowPrint = true) {
    // Read input until \t or next line or three successive white spaces.
    std::string text;

    std::string temp;
    bool isEmpty = false;
    if (ss.good()) {
      std::getline(ss, temp);
      std::string delimiter = "\t\n ";
      // Make sure temp is not empty or does not contain white characters only.
      isEmpty = temp.empty() ||
                temp.find_first_not_of(delimiter) == std::string::npos;
    }

    if (!isEmpty) {
      std::string::size_type pos;

      // Replace the first three successive white spaces with tab.
      pos = temp.find("   "); // Three white spaces.
      if (pos != std::string::npos) {
        temp.replace(pos, pos + 3, "\t");
      }

      std::string delimiter = "\t\n"; // Not include space ' '.
      pos = temp.find_first_of(delimiter);
      if (pos != std::string::npos)
        temp.erase(pos);
      // Do not have '\'. Exclude unvisible junk characters.
      std::string char0 =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01"
          "23456789_=+-.~&*[]|{}@!#$%^()/?<>,;:";
      var = temp.substr(0, temp.find_last_of(char0) + 1);
      if (isVerbose && doAllowPrint) {
        std::cout << component << ": " << std::left << std::setw(40) << var
                  << description << std::endl;
        return;
      }
    } else {
      if (isVerbose)
        std::cout << "String: can not find input parameter. Abort!"
                  << std::endl;
      std::abort();
    }
  }

  inline void skip_lines(int nlines) {
    for (int i = 0; i < nlines; i++) {
      ss.ignore(INT_MAX, '\n');
    }
  }

  //==========================================================
  // Optional parameter read. Like read_var, but returns false (and leaves the
  // stream position unchanged) instead of aborting when the next line is
  // missing or is the start of a new #COMMAND. Used for parameters that are
  // NOT required in every PARAM.in, so omitting them keeps the default value.
  template <class T>
  bool read_optional(std::string description, T &var) {
    if (!ss.good()) return false;
    std::streampos pos = ss.tellg();
    std::string line;
    std::getline(ss, line);
    if (line.empty() || line[0] == '#') {
      ss.clear();            // getline may have hit EOF; restore good bit
      ss.seekg(pos);         // un-consume the line
      return false;
    }
    // The line is "<value> <name>" (value first, parameter name second), the
    // standard SWMF PARAM convention.  Only consume the line when the name
    // matches the requested parameter, so a parameter block may list a subset
    // of the optional parameters (each individually optional) with the values
    // first, e.g. the #WAVEIC block:
    //     0.02  frac
    //     1     waveMode
    //     3.0   anisoTPerpOverTPar
    std::istringstream iss(line);
    std::string name, value;
    iss >> value >> name;
    if (value.empty() || name != description) {
      ss.seekg(pos);
      return false;
    }
    std::istringstream vs(value);
    T parsed;
    vs >> parsed;
    if (vs.fail()) {
      ss.seekg(pos);
      return false;
    }
    var = parsed;
    if (isVerbose) {
      std::cout << component << ": " << std::left << std::setw(40) << var
                << description << " (optional)" << std::endl;
    }
    return true;
  }

  // String specialisation (already line-oriented).
  bool read_optional(std::string description, std::string &var) {
    if (!ss.good()) return false;
    std::streampos pos = ss.tellg();
    std::string line;
    std::getline(ss, line);
    if (line.empty() || line[0] == '#') {
      ss.clear();
      ss.seekg(pos);
      return false;
    }
    std::istringstream iss(line);
    std::string name, value;
    iss >> value >> name;
    if (value.empty() || name != description) {
      ss.seekg(pos);
      return false;
    }
    var = value;
    if (isVerbose) {
      std::cout << component << ": " << std::left << std::setw(40) << var
                << description << " (optional)" << std::endl;
    }
    return true;
  }
};

inline void char_to_string(std::string &ss, char *chararray, int length,
                           int linelength) {
  for (int i = linelength; i < length; i += linelength)
    chararray[i - 1] = '\n';
  ss.assign(chararray, length);
}

inline std::string char_to_string(char *chars, int length, int linelength) {
  for (int i = linelength; i < length; i += linelength)
    chars[i - 1] = '\n';
  return std::string(chars, length);
}

#endif
