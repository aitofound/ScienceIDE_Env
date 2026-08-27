// SPDX-License-Identifier: GPL-2.0
// Fortran namelist reader for LAPS mhd.input decks.
//
// The deck is the only input this program takes.  Every quantity the solver
// needs is read from it, including the grid, so a patched deck moves the run
// without a rebuild of anything but the image the grader builds anyway.
//
// The parser is deliberately permissive in the same places gfortran's namelist
// reader is: group order does not matter, names are case-insensitive, logicals
// accept T/F/.true./.false., and an unlisted name keeps the upstream default.
#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace laps {

inline std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// name -> raw token, with the namelist group forgotten.  LAPS itself puts each
// name in exactly one group, so flattening loses nothing and spares the caller
// from tracking which group upstream happened to file a variable under.
class Deck {
  public:
    void load(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) throw std::runtime_error("cannot open deck: " + path);
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
        parse(text);
    }

    bool has(const std::string& key) const { return kv_.count(lower(key)) != 0; }

    double real(const std::string& key, double dflt) const {
        auto it = kv_.find(lower(key));
        if (it == kv_.end()) return dflt;
        return std::strtod(it->second.c_str(), nullptr);
    }

    long integer(const std::string& key, long dflt) const {
        auto it = kv_.find(lower(key));
        if (it == kv_.end()) return dflt;
        return std::strtol(it->second.c_str(), nullptr, 10);
    }

    bool logical(const std::string& key, bool dflt) const {
        auto it = kv_.find(lower(key));
        if (it == kv_.end()) return dflt;
        std::string v = lower(it->second);
        // gfortran accepts T, .TRUE., .T., TRUE and the F equivalents.
        size_t i = 0;
        while (i < v.size() && v[i] == '.') ++i;
        if (i >= v.size()) return dflt;
        return v[i] == 't';
    }

    const std::map<std::string, std::string>& all() const { return kv_; }

  private:
    void parse(const std::string& text) {
        // Fortran comments start with '!' outside a quoted string; LAPS decks
        // carry none, but a patched deck may.
        std::string s;
        s.reserve(text.size());
        bool in_quote = false;
        char quote = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (in_quote) {
                if (c == quote) in_quote = false;
                s.push_back(c);
                continue;
            }
            if (c == '\'' || c == '"') { in_quote = true; quote = c; s.push_back(c); continue; }
            if (c == '!') {                       // to end of line
                while (i < text.size() && text[i] != '\n') ++i;
                s.push_back('\n');
                continue;
            }
            s.push_back(c);
        }

        // Namelist syntax: &group name = value [,] ... /
        // Values are single scalars in every LAPS deck; a comma or newline or
        // the next `name =` ends one.
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] != '&') { ++i; continue; }
            ++i;                                   // past '&'
            while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            // body of the group, up to a '/' at statement level
            while (i < s.size() && s[i] != '/') {
                while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) ||
                                        s[i] == ',')) ++i;
                if (i >= s.size() || s[i] == '/') break;
                size_t nb = i;
                while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) ||
                                        s[i] == '_')) ++i;
                std::string name = lower(s.substr(nb, i - nb));
                while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                if (i >= s.size() || s[i] != '=') { if (i < s.size()) ++i; continue; }
                ++i;                               // past '='
                while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                size_t vb = i;
                while (i < s.size() && s[i] != ',' && s[i] != '\n' && s[i] != '/') ++i;
                std::string val = s.substr(vb, i - vb);
                while (!val.empty() &&
                       std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
                if (!name.empty()) kv_[name] = val;
            }
            if (i < s.size()) ++i;                 // past '/'
        }
    }

    std::map<std::string, std::string> kv_;
};

}  // namespace laps
