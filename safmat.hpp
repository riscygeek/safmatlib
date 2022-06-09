/*  Standalone Text Formatting Library.
    Copyright (C) 2022 Benjamin Stürz <benni@stuerz.xyz>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lessser General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef FILE_SAFMAT_HPP
#define FILE_SAFMAT_HPP
#include <string_view>
#include <type_traits>
#include <exception>
#include <concepts>
#include <charconv>
#include <utility>
#include <string>
#include <memory>
#include <cctype>
#include <array>
#include <span>

// Enable support for std::ostream OutputIterators (default=disabled).
#ifndef  SAFMAT_OUT_OSTREAM
# define SAFMAT_OUT_OSTREAM 0
#endif
#if SAFMAT_OUT_OSTREAM
# include <ostream>
#endif

// Enable support for std::FILE* OutputIterators (default=enabled).
#ifndef  SAFMAT_OUT_FILE
# define SAFMAT_OUT_FILE 1
#endif
#if SAFMAT_OUT_FILE
# include <cstdio>
#endif

namespace safmat {
    template<class T>
    struct Formatter;

    class format_error : std::exception {
    private:
        std::string msg;
    public:
        format_error(std::string msg) : msg(std::move(msg)) {}

        const char *what() const noexcept override { return msg.c_str(); }
    };
}

namespace safmat::io {
    struct OutputIteratorBase {
        virtual ~OutputIteratorBase() = default;
        virtual void write(std::string_view) const = 0;
    };

    template<class T>
    struct OutputIteratorImpl;
}

namespace safmat {
    class OutputIterator {
    private:
        std::unique_ptr<const io::OutputIteratorBase> ptr;
    public:
        template<class T>
        OutputIterator(T &object)
            : ptr(std::make_unique<io::OutputIteratorImpl<T>>(object)) {}

        void write(std::string_view s) {
            ptr->write(s);
        }
        void write(char ch) {
            ptr->write(std::string_view{&ch, &ch + 1});
        }
    };

    using InputIterator = decltype(std::string_view{}.begin());

    template<class T>
    concept Formattable = requires (const std::remove_cvref_t<T> &x,
                                    Formatter<std::remove_cvref_t<T>> &fmt,
                                    InputIterator &in,
                                    OutputIterator &out) {
        fmt.parse(in);
        fmt.format_to(out, x);
    };

    class FormatArg {
    private:
        struct FormatArgBase {
            virtual ~FormatArgBase() = default;
            virtual void parse(InputIterator &) = 0;
            virtual void format_to(OutputIterator &) = 0;
            virtual void reset_fmt() = 0;
        };
        template<Formattable T>
        struct FormatArgImpl : FormatArgBase {
            Formatter<T> fmt{};
            T value;

            FormatArgImpl(T value) : value(std::move(value)) {}

            void parse(InputIterator &in) override { fmt.parse(in); }
            void format_to(OutputIterator &out) override { fmt.format_to(out, value); }
            void reset_fmt() override { fmt = Formatter<T>{}; }
        };

        std::unique_ptr<FormatArgBase> ptr;
    public:
        template<Formattable T>
        FormatArg(T x) : ptr(std::make_unique<FormatArgImpl<T>>(std::move(x))) {}
        FormatArg(FormatArg &&) = default;

        FormatArg &operator=(FormatArg &&) = default;
        template<Formattable T>
        FormatArg &operator=(T x) {
            ptr = std::make_unique<FormatArgImpl<T>>(std::move(x));
            return *this;
        }

        void parse(InputIterator &in) const { ptr->parse(in); }
        void format_to(OutputIterator &out) const { ptr->format_to(out); }
        void reset_fmt() const { ptr->reset_fmt(); }
     };

     inline void vformat_to(OutputIterator &out, std::string_view fmt, std::span<FormatArg> args) {
        std::size_t carg{0};
        auto it = begin(fmt);

        while (it != end(fmt)) {
            if (*it == '{') {
                ++it;

                if (*it == '{') {
                    ++it;
                    out.write('{');
                    continue;
                }

                std::size_t idx;
                if (std::isdigit(*it)) {
                    idx = 0;
                    while (std::isdigit(*it))
                        idx = idx * 10 + (*it++ - '0');
                } else {
                    idx = carg++;
                }

                if (idx >= args.size())
                    throw format_error("Format index too large.");

                auto &arg = args[idx];

                arg.reset_fmt();

                if (*it == ':') {
                    ++it;
                    arg.parse(it);
                }

                if (*it != '}')
                    throw format_error("Expected '}'.");

                ++it;

                arg.format_to(out);
            } else if (*it == '}') {
                ++it;
                if (*it == '}') {
                    ++it;
                    out.write('}');
                } else {
                    throw format_error("'}' must be escaped with '}'.");
                }
            } else {
                out.write(*it++);
            }
        }
    }

    template<class... Args>
    void format_to(OutputIterator &out, std::string_view fmt, Args&&... args) {
        std::array<FormatArg, sizeof...(args)> argv{ FormatArg{ std::forward<Args>(args) }... };
        vformat_to(out, fmt, argv);
    }

    template<class... Args>
    std::string format(std::string_view fmt, Args&&... args) {
        std::string str{};
        OutputIterator out{str};
        format_to(out, fmt, std::forward<Args>(args)...);
        return str;
    }

    template<class... Args>
    void print(OutputIterator out, std::string_view fmt, Args&&... args) {
        format_to(out, fmt, std::forward<Args>(args)...);
    }

    template<class... Args>
    void print(std::string_view fmt, Args&&... args) {
        print(stdout, fmt, std::forward<Args>(args)...);
    }

    template<class... Args>
    void println(OutputIterator out, std::string_view fmt, Args&&... args) {
        format_to(out, fmt, std::forward<Args>(args)...);
        out.write('\n');
    }

    template<class... Args>
    void println(std::string_view fmt, Args&&... args) {
        println(stdout, fmt, std::forward<Args>(args)...);
    }
}

// OutputIteratorImpl<T> specializations.
namespace safmat::io {
    template<>
    struct OutputIteratorImpl<std::string> : OutputIteratorBase {
        std::string &str;

        OutputIteratorImpl(std::string &s) : str(s) {}

        void write(std::string_view s) const override {
            str += s;
        }
    };

#if SAFMAT_OUT_OSTREAM
    template<>
    struct OutputIteratorImpl<std::ostream> : OutputIteratorBase {
        std::ostream &stream;

        OutputIteratorImpl(std::ostream &s) : stream(s) {}

        void write(std::string_view s) const override {
            stream << s;
        }
    };
#endif // SAFMAT_OUT_OSTREAM

#if SAFMAT_OUT_FILE
    template<>
    struct OutputIteratorImpl<std::FILE *> : OutputIteratorBase {
        std::FILE *file;

        OutputIteratorImpl(std::FILE *f) : file(f) {}

        void write(std::string_view s) const override {
            std::fwrite(s.data(), 1, s.size(), file);
        }
    };
#endif // SAFMAT_OUT_FILE
}

namespace safmat::concepts {
    template<class T>
    concept StringLike = requires (T x) {
        std::string_view{x};
    };

    template<class C>
    concept FormattableContainer = !StringLike<C> && requires (const C &c) {
        { *begin(c) } -> Formattable;
        { *end(c) } -> Formattable;
    };

    template<FormattableContainer C>
    using elem_type_t = std::remove_cvref_t<decltype(*begin(*(C *)0))>;
}

// Formatter<T> specializations.
namespace safmat {
    template<std::integral T>
    struct Formatter<T> {
        char rep;
        char sign{'-'};
        bool show_prefix{false};

        Formatter(char rep = 'd') : rep(rep) {}

        void parse(InputIterator &in) {
            switch (*in) {
            case '+':
            case '-':
            case ' ':
                sign = *in++;
                break;
            }

            if (*in == '#') {
                show_prefix = true;
                ++in;
            }

            switch (*in) {
            case 'b':
            case 'B':
            case 'c':
            case 'd':
            case 'o':
            case 'x':
            case 'X':
                rep = *in++;
                break;
            if constexpr (std::is_same_v<bool, T>) {
            case 's':
                rep = *in++;
            }
            case '}':
                return;
            default:
                throw format_error("Expected '}'.");
            }
        }

        void format_to(OutputIterator &out, T x) {
            std::to_chars_result result;
            char prefix[3]{};
            char buffer[sizeof (T) * 8 + 2];
            bool is_negative;

            if constexpr (std::is_signed_v<T>) {
                is_negative = x < T{};
                if (is_negative)
                    x = -x;
            } else {
                is_negative = false;
            }

            auto to_chars = [&result, &buffer, x](int base) {
                result = std::to_chars(buffer, buffer + sizeof buffer, x, base);
            };

            switch (rep) {
            case 'b':
            case 'B':
                prefix[0] = '0';
                prefix[1] = rep;
                to_chars(2);
                break;
            case 'c':
                out.write(static_cast<char>(x));
                return;
            case 'd':
            case '\0':
                to_chars(10);
                break;
            case 'o':
                prefix[0] = '0';
                to_chars(8);
                break;
            case 'x':
            case 'X':
                prefix[0] = '0';
                prefix[1] = rep;
                to_chars(16);
                break;
            default:
                throw format_error{"Unimplemented operation"};
            }

            if (result.ec == std::errc{}) {
                if (is_negative) {
                    out.write('-');
                } else if (sign != '-') {
                    out.write(sign);
                }
                if (show_prefix)
                    out.write(prefix);
                out.write(std::string_view{buffer, result.ptr});
            } else {
                throw format_error{"Failed to format integer."};
            }
        }
    };

    template<>
    struct Formatter<bool> : Formatter<unsigned> {
        using F = Formatter<unsigned>;
        Formatter() : Formatter<unsigned>('s') {}
        void format_to(OutputIterator &out, bool x) {
            if (rep == 's') {
                out.write(x ? "true" : "false");
            } else {
                F::format_to(out, x);
            }
        }
    };

    template<>
    struct Formatter<char> : Formatter<int> {
        using F = Formatter<int>;
        Formatter() : Formatter<int>('c') {}
    };

    template<concepts::StringLike T>
    struct Formatter<T> {
        void parse(InputIterator &) {}
        void format_to(OutputIterator &out, const T &x) {
            out.write(x);
        }
    };

    template<concepts::FormattableContainer C>
    struct Formatter<C> : Formatter<concepts::elem_type_t<C>> {
        using T = concepts::elem_type_t<C>;
        using F = Formatter<T>;

        void format_to(OutputIterator &out, const C &c) {
            out.write('[');
            auto it = begin(c);
            const auto e = end(c);
            if (it != e) {
                F::format_to(out, *it++);
                while (it != e) {
                    out.write(", ");
                    F::format_to(out, *it++);
                }
            }
            out.write(']');
        }
    };
}

#endif // FILE_SAFMAT_HPP
