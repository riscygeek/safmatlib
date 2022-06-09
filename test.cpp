#define SAFMAT_OUT_OSTREAM 1
#include <iostream>
#include <vector>
#include <set>
#include "safmat.hpp"

struct RandomStruct {
    int x;
    const char *s;
    std::set<int> ints;
};

template<>
struct safmat::Formatter<RandomStruct> {
    void parse(safmat::InputIterator &) {}
    void format_to(safmat::OutputIterator &out, const RandomStruct &r) {
        safmat::format_to(out, "{{ x={}, s='{}', ints={:-#x} }}", r.x, r.s, r.ints);
    }
};

int main() {
    using namespace std::literals;
    using namespace safmat;
    print("Hello {}!\n", 42);
    println("Hello {} {2} {1}!", "World", "String"s, "StringView"sv);

    auto vec = std::vector{10, 20, 30};
    println("vec = {}", vec);

    println("{} {}", true, 'X');

    RandomStruct r{ 42, "Hello World", { 1, 2, 5, 4, 96, 69, -420, 22 } };
    println(std::cout, "r = {}", r);
}
