#include <algorithm>
#include <iostream>

int main(int argc, char *argv[]) {
    std::copy(argv, argv + argc, std::ostream_iterator<char *>(std::cout, "\n"));
    // ...
}