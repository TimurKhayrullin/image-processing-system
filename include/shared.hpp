#ifndef SHARED_HPP
#define SHARED_HPP

#include <string>
#include <iostream>

inline void print_banner(const std::string& name) {
    std::cout << "=== " << name << " ===" << std::endl;
}

#endif // SHARED_HPP