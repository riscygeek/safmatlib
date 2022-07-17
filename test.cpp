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
    void format_to(safmat::FormatContext &ctx, const RandomStruct &r) {
        safmat::format_to(ctx.out, "{{ x={}, s='{}', ints={:-#x} }}", r.x, r.s, r.ints);
    }
};

template<>
struct safmat::io::OutputAdapter<std::vector<char>> {
    inline static void write(std::vector<char> *vec, std::string_view s) {
        vec->insert(end(*vec), begin(s), end(s));
    }
};

int main() {
    using namespace std::literals;
    using namespace safmat;

    auto loc = std::source_location::current();
    println("loc = {}", loc);

    try {
        println("'{:x^100}'", "Hello");
        println("Hello {:0{}}", 42, 100);

        println("'{:X^#8x}'", -42);
        println("Hello {} {2} {1}!", "World", "String"s, "StringView"sv);
        println("pi = {}", std::numbers::pi);

        auto vec = std::vector{10, 20, 30};
        println("vec = {}", vec);

        println("{} {0:d} {}", true, 'X');

        println("'{:^11.5}'", "Hello World");

        println("{:-^40}", std::pair{42, "Hello"});

        RandomStruct r{ 42, "Hello World", { 1, 2, 5, 4, 96, 69, -420, 22 } };
        println(std::cout, "r = {}", r);

        std::vector<char> chars{};
        print(chars, "Hello World in the vector of chars.");
        println("{}", chars);
    } catch (const format_error &e) {
        println("ERROR: {}", e.what());
    }
}
