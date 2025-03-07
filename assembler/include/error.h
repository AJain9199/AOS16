#ifndef ERROR_H
#define ERROR_H

#include <string>
#include <iostream>

typedef std::pair<std::string, int> loc;

inline void error(const loc &src, const std::string &msg) {
    std::cerr << src.first << ": " << src.second << ": " << msg << std::endl;
    exit(1);
}

#endif //ERROR_H
