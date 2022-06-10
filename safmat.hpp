/*  Standalone Text Formatting Library.
    Copyright (C) 2022 Benjamin Stürz <benni@stuerz.xyz>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
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
#include <functional>
#include <exception>
#include <concepts>
#include <charconv>
#include <utility>
#include <cstring>
#include <string>
#include <memory>
#include <cctype>
#include <array>
#include <span>

// Enable support for std::ostream Output (default=disabled).
#ifndef  SAFMAT_OUT_OSTREAM
# define SAFMAT_OUT_OSTREAM 0
#endif
#if SAFMAT_OUT_OSTREAM
# include <ostream>
#endif

// Enable support for std::FILE* Output (default=enabled).
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
    template<class T>
    struct OutputAdapter;

    template<class T>
    concept OutputConcept = requires (T *out, std::string_view s) {
        OutputAdapter<T>::write(out, s);
    };

    class Output {
    private:
        struct OutputBase {
            virtual ~OutputBase() = default;
            virtual void write(std::string_view s) const = 0;
        };
        template<OutputConcept T>
        struct OutputImpl : OutputBase {
            T *out;

            OutputImpl(T *out) : out(out) {}

            void write(std::string_view s) const override {
                OutputAdapter<T>::write(out, s);
            }
        };
        std::shared_ptr<const OutputBase> ptr;
    public:
        Output(const Output &) = default;
        Output(Output &&) = default;
        template<OutputConcept T>
        Output(T *out) : ptr(std::make_unique<const OutputImpl<T>>(out)) {}
        template<OutputConcept T>
        Output(T &out) : ptr(std::make_unique<const OutputImpl<T>>(&out)) {}

        Output &operator=(const Output &) = default;
        Output &operator=(Output &&) = default;

        void write(std::string_view s) const { ptr->write(s); }
        void write(char ch) const { ptr->write({ &ch, &ch + 1 }); }
    };
}

namespace safmat {
    using io::Output;
    using InputIterator = decltype(std::string_view{}.begin());

    template<class T>
    concept Formattable = requires (const std::remove_cvref_t<T> &x,
                                    Formatter<std::remove_cvref_t<T>> &fmt,
                                    InputIterator &in,
                                    Output out) {
        fmt.parse(in);
        fmt.format_to(out, x);
    };

    class FormatArg {
    private:
        struct FormatArgBase {
            virtual ~FormatArgBase() = default;
            virtual void parse(InputIterator &) = 0;
            virtual void format_to(Output) = 0;
            virtual void reset_fmt() = 0;
        };
        template<Formattable T>
        struct FormatArgImpl : FormatArgBase {
            Formatter<T> fmt{};
            T value;

            FormatArgImpl(T value) : value(std::move(value)) {}

            void parse(InputIterator &in) override { fmt.parse(in); }
            void format_to(Output out) override { fmt.format_to(out, value); }
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
        void format_to(Output out) const { ptr->format_to(out); }
        void reset_fmt() const { ptr->reset_fmt(); }
     };

     inline void vformat_to(Output out, std::string_view fmt, std::span<FormatArg> args) {
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
    void format_to(Output &out, std::string_view fmt, Args&&... args) {
        std::array<FormatArg, sizeof...(args)> argv{ FormatArg{ std::forward<Args>(args) }... };
        vformat_to(out, fmt, argv);
    }

    template<class... Args>
    std::string format(std::string_view fmt, Args&&... args) {
        std::string str{};
        Output out{str};
        format_to(out, fmt, std::forward<Args>(args)...);
        return str;
    }

    template<class... Args>
    void print(Output out, std::string_view fmt, Args&&... args) {
        format_to(out, fmt, std::forward<Args>(args)...);
    }

    template<class... Args>
    void print(std::string_view fmt, Args&&... args) {
        print(stdout, fmt, std::forward<Args>(args)...);
    }

    template<class... Args>
    void println(Output out, std::string_view fmt, Args&&... args) {
        format_to(out, fmt, std::forward<Args>(args)...);
        out.write('\n');
    }

    template<class... Args>
    void println(std::string_view fmt, Args&&... args) {
        println(stdout, fmt, std::forward<Args>(args)...);
    }
}

// OutputAdapter<T> specializations.
namespace safmat::io {
    template<>
    struct OutputAdapter<std::string> {
        inline static void write(std::string *out, std::string_view s) {
            *out += s;
        }
    };

#if SAFMAT_OUT_OSTREAM
    template<>
    struct OutputAdapter<std::ostream> {
        inline static void write(std::ostream *out, std::string_view s) {
            *out << s;
        }
    };
#endif // SAFMAT_OUT_OSTREAM

#if SAFMAT_OUT_FILE
    template<>
    struct OutputAdapter<FILE> {
        inline static void write(FILE *out, std::string_view s) {
            std::fwrite(s.data(), 1, s.size(), out);
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
    namespace internal {
        inline void parse_signh0(InputIterator &in, char &sign, bool &show_prefix, char &padding, bool &pad_zero) {
            if (*in == '+' || *in == '-' || *in == ' ') {
                sign = *in++;
            }

            if (*in == '#') {
                show_prefix = true;
                ++in;
            }

            if (*in == '0') {
                pad_zero = padding == '\0';
                ++in;
            }
        }
        inline void parse_prec(InputIterator &in, std::size_t &prec) {
            if (*in == '.') {
                ++in;
                if (!std::isdigit(*in))
                    throw format_error{"Expected number after '.'."};

                prec = 0;
                while (std::isdigit(*in))
                    prec = prec * 10 + (*in++ - '0');
            }
        }

        struct PaddedFormatter {
            char fill{'>'};
            char padding{'\0'};
            std::size_t width{0};

            void parse_fill(InputIterator &in) {
                auto is_fill = [](char ch) {
                    return ch == '<' || ch == '>' || ch == '^';
                };
                if (is_fill(*in)) {
                    padding = ' ';
                    fill = *in++;
                } else if (*in && is_fill(in[1])) {
                    padding = *in++;
                    fill = *in++;
                }
            }
            void parse_width(InputIterator &in) {
                // Parse width.
                width = 0;
                while (std::isdigit(*in)) {
                    width = width * 10 + (*in++ - '0');
                }
            }

            void print_padding(Output out, std::size_t len, std::size_t add) {
                const std::string pad(fill == '^' ? (len + add) / 2 : len, padding);
                out.write(pad);
            }

            void pre_format(Output out, std::size_t len) {
                if (len < width && (fill == '>' || fill == '^')) {
                    print_padding(out, width - len, 0);
                }
            }
            void post_format(Output out, std::size_t len) {
                if (len < width && (fill == '<' || fill == '^')) {
                    print_padding(out, width - len, 1);
                }
            }
        };

        struct IntegralFormatter : PaddedFormatter {
            char sign{'-'};
            char rep;
            bool show_prefix{false};
            bool pad_zero{false};

            IntegralFormatter(char rep) : rep{rep} {}

            void parse(InputIterator &in, bool is_bool) {
                PaddedFormatter::parse_fill(in);
                parse_signh0(in, sign, show_prefix, padding, pad_zero);
                PaddedFormatter::parse_width(in);

                // Parse 'L'.
                if (*in == 'L')
                    throw format_error{"Locale-specific formatting is not implemented/supported."};

                // Parse rep.
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
                case 's':
                    if (is_bool) {
                        rep = *in++;
                        break;
                    }
                    // fallthrough
                case '}':
                    return;
                default:
                    throw format_error("Expected '}'.");
                }
            }
            void format(Output out, std::function<std::string(int)> f, bool negative) {
                char prefix[3]{};
                std::string number;

                switch (rep) {
                case 'b':
                case 'B':
                    prefix[0] = '0';
                    prefix[1] = rep;
                    number = f(2);
                    break;
                case 'c':
                    number = f(0);
                    break;
                case 'd':
                case '\0':
                    number = f(10);
                    break;
                case 'o':
                    prefix[0] = '0';
                    number = f(8);
                    break;
                case 'x':
                case 'X':
                    prefix[0] = '0';
                    prefix[1] = rep;
                    number = f(16);
                    break;
                default:
                    throw format_error{"Unimplemented operation."};
                }

                if (!show_prefix)
                    prefix[0] = '\0';

                const std::string_view sign_str = negative && rep != 'c' ? "-" : (sign != '-' && rep != 'c' ? std::string_view{&sign, &sign + 1} : "");

                if (pad_zero) {
                    out.write(sign_str);
                    out.write(prefix);

                    if (const auto len = number.size(); len < width) {
                        const std::string pad(width - len, '0');
                        out.write(pad);
                    }

                    out.write(number);
                } else {
                    const auto len = sign_str.size() + std::strlen(prefix) + number.size();

                    PaddedFormatter::pre_format(out, len);
                    out.write(sign_str);
                    out.write(prefix);
                    out.write(number);
                    PaddedFormatter::post_format(out, len);
                }
            }
        };

    }

    template<std::integral T>
    struct Formatter<T> : internal::IntegralFormatter {
        Formatter(char rep = 'd') : IntegralFormatter{rep} {}

        void parse(InputIterator &in) {
            IntegralFormatter::parse(in, std::is_same_v<T, bool>);
       }

        void format_to(Output out, T x) {
            bool is_negative;

            if constexpr (std::is_signed_v<T>) {
                is_negative = x < T{};
                if (is_negative)
                    x = -x;
            } else {
                is_negative = false;
            }

            const auto f = [x](int base) {
                if (base == 0) {
                    return std::string(1, static_cast<char>(x));
                }
                char buffer[sizeof (T) * 8 + 2];
                const auto result = std::to_chars(buffer, buffer + sizeof buffer, x, base);
                if (result.ec == std::errc{}) {
                    *result.ptr = '\0';
                    return std::string{buffer};
                } else {
                    throw format_error{"Number too big."};
                }
            };

            IntegralFormatter::format(out, f, is_negative);
        }
    };

    template<>
    struct Formatter<bool> : Formatter<unsigned> {
        using F = Formatter<unsigned>;
        Formatter() : Formatter<unsigned>('s') {}
        void format_to(Output out, bool x) {
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
        void format_to(Output out, const T &x) {
            out.write(x);
        }
    };

    template<concepts::FormattableContainer C>
    struct Formatter<C> : Formatter<concepts::elem_type_t<C>> {
        using T = concepts::elem_type_t<C>;
        using F = Formatter<T>;

        void format_to(Output out, const C &c) {
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
