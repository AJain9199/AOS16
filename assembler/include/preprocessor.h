#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <string>

// Reads filename, expands .macro/.endmacro definitions, returns expanded source.
std::string preprocess(const std::string& filename);

#endif //PREPROCESSOR_H
