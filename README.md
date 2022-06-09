# Standalone Text Formatting Library
This is a simple library for text formatting.
It is inspired by (but not compatible to) [std::format](https://en.cppreference.com/w/cpp/utility/format/format) and [{fmt}](https://fmt.dev).

## How to use this library in your program.
Just copy the [`safmat.hpp`](safmat.hpp) file into your projects include directory.
Alternatively you can install it into `/usr/local/include`.
Then you should be able to `#include "safmat.hpp"` the file and use it.

## HowTo Enable/Disable features.
Just `#define` the following to 0 or 1, before `#include`ing the file.
- `SAFMAT_OUT_OSTREAM` (`std::ostream&` OutputIterator)
- `SAFMAT_OUT_FILE` (`FILE*` OutputIterator)

## Examples

### 1. Getting started
```
#include "safmat.hpp"

int main() {
    safmat::println("Hello {}!", "World");
}
```

### 2. Implementing your own Formatter
```
<--snip-->
struct Person {
    std::string name;
    unsigned int age;
};

template<>
struct safmat::Formatter<Person> {
    char rep{'s'};  // s=simple x=extended
    
    void parse(safmat::InputIterator &in) {
        switch (*in) {
        case 's':
        case 'x':
            rep = *in++;
            break;
        case '}':
            return;
        default:
            throw safmat::format_error{"Invalid format specifier."};
        }
    }
    
    void format_to(safmat::OutputIterator &out, const Person &p) {
        switch (rep) {
        case 's':
            safmat::format_to("{}", p.name);
            break;
        case 'x':
            safmat::format_to("Person{{ name=\"{}\", age={} }}", p.name, p.age);
            break;
        default:
            throw safmat::format_error{"Unimplemented format specifier."};
        }
    }
};

int main() {
    Person p{"Max Mustermann", 42};
    safmat::println("p = {}", p);
}
```

For more examples look into [test.cpp](test.cpp).

## TODO
- [ ] Implement more Formatter<> specializations.
    - [ ] std::floating_point
    - [ ] std::chrono::*
    - [ ] std::pair
    - [ ] std::tuple (maybe?)
    - [ ] T*
- [ ] Implement more [standard format specifiers](https://en.cppreference.com/w/cpp/utility/format/formatter#Standard_format_specification) and do it properly
- [ ] Replace OutputIterator with something different.
- [ ] Somehow make the library char-independent without templating vformat_to().
- [ ] vformat()
- [ ] vprint(), vprintln()
- [ ] (maybe) implement locale-specific stuff (probably not)
- [ ] Make the library constexpr (requires C++23's constexpr unique_ptr)
