#include <preprocessor.h>
#include <error.h>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

struct MacroDef {
    int param_count = 0;
    std::vector<std::string> body;
};

static std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
}

// First identifier/directive token on the line, stopping at whitespace or ':'.
static std::string first_word(const std::string& line) {
    size_t i = 0;
    while (i < line.size() && isspace((unsigned char)line[i])) i++;
    const size_t start = i;
    while (i < line.size() && !isspace((unsigned char)line[i]) && line[i] != ':') i++;
    return line.substr(start, i - start);
}

// Strip trailing ; # // comment from a string, string literals respected.
static std::string strip_comment(const std::string& s) {
    bool in_str = false;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '"') { in_str = !in_str; continue; }
        if (in_str) continue;
        if (s[i] == ';' || s[i] == '#') return s.substr(0, i);
        if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') return s.substr(0, i);
    }
    return s;
}

// Split comma-separated args respecting bracket depth.
static std::vector<std::string> split_args(const std::string& s) {
    std::vector<std::string> args;
    std::string cur;
    int depth = 0;
    for (const char c : s) {
        if (c == '(' || c == '[') depth++;
        else if (c == ')' || c == ']') depth--;
        if (c == ',' && depth == 0) {
            if (const std::string t = trim(cur); !t.empty()) args.push_back(t);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (const std::string t = trim(cur); !t.empty()) args.push_back(t);
    return args;
}

// Replace %1 %2 ... in a body line with the caller's args.
static std::string substitute(const std::string& line, const std::vector<std::string>& args) {
    std::string result;
    for (size_t i = 0; i < line.size();) {
        if (line[i] == '%' && i + 1 < line.size() && isdigit((unsigned char)line[i + 1])) {
            size_t j = i + 1;
            while (j < line.size() && isdigit((unsigned char)line[j])) j++;
            const int idx = std::stoi(line.substr(i + 1, j - i - 1));
            if (idx >= 1 && idx <= (int)args.size())
                result += args[idx - 1];
            else
                result += line.substr(i, j - i); // out-of-range: keep literal
            i = j;
        } else {
            result += line[i++];
        }
    }
    return result;
}

std::string preprocess(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open())
        error({filename, 0}, "Cannot open file");

    std::map<std::string, MacroDef> macros;
    std::ostringstream out;

    std::string line;
    int line_num = 0;
    bool in_macro = false;
    std::string macro_name;
    MacroDef macro_def;

    const auto err = [&](const std::string& msg) {
        error({filename, line_num}, msg);
    };

    while (std::getline(file, line)) {
        line_num++;
        const std::string t = trim(line);
        const std::string tok = first_word(t);

        if (in_macro) {
            if (tok == ".endmacro") {
                macros[macro_name] = macro_def;
                in_macro = false;
            } else {
                macro_def.body.push_back(line);
            }
            continue;
        }

        if (tok == ".macro") {
            const std::string rest = trim(t.substr(6)); // skip ".macro"
            std::istringstream iss(rest);
            if (!(iss >> macro_name)) err(".macro requires a name");
            int count = 0;
            iss >> count; // optional; defaults to 0
            in_macro = true;
            macro_def = {count, {}};
            continue;
        }

        if (macros.count(tok)) {
            const MacroDef& def = macros[tok];
            const std::string args_str = strip_comment(trim(t.substr(tok.size())));
            const std::vector<std::string> args = split_args(args_str);
            if ((int)args.size() != def.param_count) {
                err("macro '" + tok + "': expected " + std::to_string(def.param_count) +
                    " args, got " + std::to_string(args.size()));
            }
            for (const std::string& body_line : def.body)
                out << substitute(body_line, args) << '\n';
            continue;
        }

        out << line << '\n';
    }

    if (in_macro) err("unterminated macro definition '" + macro_name + "'");

    return out.str();
}
