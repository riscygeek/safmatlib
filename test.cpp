#define SAFMAT_OUT_OSTREAM 1
#include <iostream>
#include <numbers>
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
    void format_to(safmat::Output out, const RandomStruct &r) {
        safmat::format_to(out, "{{ x={}, s='{}', ints={:-#x} }}", r.x, r.s, r.ints);
    }
};

int main() {
    using namespace std::literals;
    using namespace safmat;

    println("'{:X^#8x}'", -42);
    println("Hello {} {2} {1}!", "World", "String"s, "StringView"sv);
    println("pi = {}", std::numbers::pi);

    auto vec = std::vector{10, 20, 30};
    println("vec = {}", vec);

    println("{} {0:d} {}", true, 'X');

    println("'{:^11.5}'", "Hello World");

    RandomStruct r{ 42, "Hello World", { 1, 2, 5, 4, 96, 69, -420, 22 } };
    println(std::cout, "r = {}", r);
}
