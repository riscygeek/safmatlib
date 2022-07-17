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
#include <optional>
#include <concepts>
#include <charconv>
#include <variant>
#include <utility>
#include <cstring>
#include <version>
#include <string>
#include <memory>
#include <limits>
#include <cctype>
#include <cmath>
#include <array>
#include <span>

#if __cpp_lib_source_location >= 201907L
# include <source_location>
#endif

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
    struct FormatContext;

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

            OutputImpl(T *out) noexcept : out(out) {}

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
                                    FormatContext &ctx) {
        fmt.parse(in);
        fmt.format_to(ctx, x);
    };

    class FormatArg {
    private:
        struct FormatArgBase {
            virtual ~FormatArgBase() = default;
            virtual void parse(InputIterator &) = 0;
            virtual void format_to(FormatContext &) = 0;
            virtual void reset_fmt() = 0;
            virtual std::optional<std::size_t> to_size_t() const noexcept = 0;
        };
        template<Formattable T>
        struct FormatArgImpl : FormatArgBase {
            Formatter<T> fmt{};
            T value;

            FormatArgImpl(T value) : value(std::move(value)) {}

            void parse(InputIterator &in) override { fmt.parse(in); }
            void format_to(FormatContext &ctx) override { fmt.format_to(ctx, value); }
            void reset_fmt() override { fmt = Formatter<T>{}; }
            std::optional<std::size_t> to_size_t() const noexcept override {
                if constexpr (std::integral<T>) {
                    return std::size_t(value);
                } else {
                    return {};
                }
            }
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
        void format_to(FormatContext &ctx) const { ptr->format_to(ctx); }
        void reset_fmt() const { ptr->reset_fmt(); }
        std::optional<std::size_t> to_size_t() const noexcept { return ptr->to_size_t(); }
        std::size_t expect_size_t() const {
            const auto opt = to_size_t();
            return opt.has_value() ? opt.value() : throw format_error{"Expected size as the nested argument."};
        }
     };

    struct FormatContext {
        Output out;
        std::span<FormatArg> args{};
        std::size_t carg{0};

        const FormatArg &operator[](std::size_t idx) const {
            return idx < args.size() ? args[idx] : throw format_error{"Not enough format arguments."};
        }
        const FormatArg &next() {
            return (*this)[carg++];
        }
     };

     inline void xformat_to(FormatContext &ctx, std::string_view fmt) {
        auto &out = ctx.out;
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
                    idx = ctx.carg++;
                }

                auto &arg = ctx[idx];

                arg.reset_fmt();

                if (*it == ':') {
                    ++it;
                    arg.parse(it);
                }

                if (*it != '}')
                    throw format_error("Expected '}'.");

                ++it;

                arg.format_to(ctx);
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
    void format_to(Output out, std::string_view fmt, Args&&... args) {
        std::array<FormatArg, sizeof...(args)> argv{ FormatArg{ std::forward<Args>(args) }... };
        auto ctx = FormatContext{ out, argv, 0 };
        xformat_to(ctx, fmt);
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

// Concepts used by Formatter<>'s.
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

// Formatter<> helpers.
namespace safmat::internal {
    class NestedSizeArgFormatter {
    private:
        // std::monostate   => unspecified,
        // std::size_t      => direct,
        // std::optional    => indirect,
        std::variant<std::monostate, std::size_t, std::optional<std::size_t>> arg_rep;
    public:
        std::optional<std::size_t> arg;
        bool parse(InputIterator &in) {
            const auto parse_number = [&in] {
                std::size_t n{};
                while (std::isdigit(*in))
                    n = n * 10 + (*in++ - '0');
                return n;
            };

            if (std::isdigit(*in)) {
                arg_rep = parse_number();
                return true;
            } else if (*in == '{') {
                std::optional<std::size_t> idx{};
                ++in;

                if (std::isdigit(*in))
                    idx = parse_number();

                if (*in != '}')
                    throw format_error{"Expected '}' for nested argument."};

                ++in;
                arg_rep = idx;
                return true;
            } else {
                return false;
            }
        }
        void read(FormatContext &ctx) {
            if (arg.has_value())
                return;

            if (auto n = std::get_if<std::size_t>(&arg_rep)) {
                arg = *n;
            } else if (auto idx = std::get_if<std::optional<std::size_t>>(&arg_rep)) {
                arg = (idx->has_value() ? ctx[idx->value()] : ctx.next()).expect_size_t();
            }
        }

    };

    struct PaddedFormatter : private NestedSizeArgFormatter {
        char fill;
        char padding;

        PaddedFormatter(char fill = '<', char padding = ' ') : fill{fill}, padding{padding} {}

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
            NestedSizeArgFormatter::parse(in);
        }
        void parse(InputIterator &in) {
            parse_fill(in);
            parse_width(in);
        }

        void print_padding(Output out, std::size_t len, std::size_t add) {
            const std::string pad(fill == '^' ? (len + add) / 2 : len, padding);
            out.write(pad);
        }

        void read_width(FormatContext &ctx) { NestedSizeArgFormatter::read(ctx); }
        std::size_t width() const { return NestedSizeArgFormatter::arg.value_or(0); }

        void pre_format(Output out, std::size_t len) {
            if (len < width() && (fill == '>' || fill == '^')) {
                print_padding(out, width() - len, 0);
            }
        }
        void post_format(Output out, std::size_t len) {
            if (len < width() && (fill == '<' || fill == '^')) {
                print_padding(out, width() - len, 1);
            }
        }
    };

    struct PrecisionFormatter : private NestedSizeArgFormatter {
        void read_prec(FormatContext &ctx) { NestedSizeArgFormatter::read(ctx); }
        auto prec() const { return NestedSizeArgFormatter::arg; }
        void set_prec(std::size_t n) { arg = n; }

        void parse_prec(InputIterator &in) {
            if (*in == '.') {
                ++in;
                if (!NestedSizeArgFormatter::parse(in))
                    throw format_error{"Expected precision."};
            }
        }
    };

    struct NumericFormatter : PaddedFormatter {
        char sign{'-'};
        char alternate{false};
        char pad_zero{false};

        NumericFormatter() : PaddedFormatter{'>', '\0'} {}

        void parse(InputIterator &in) {
            PaddedFormatter::parse_fill(in);

            // Parse sign.
            if (*in == '+' || *in == '-' || *in == ' ') {
                sign = *in++;
            }

            // Parse alternate form.
            if (*in == '#') {
                alternate = true;
                ++in;
            }

            // Parse '0'.
            if (*in == '0') {
                pad_zero = padding == '\0';
                ++in;
            }

            PaddedFormatter::parse_width(in);
        }

        void format(FormatContext &ctx, std::string_view number, bool negative) {
            const std::string_view sign_str = negative ? "-" : (sign != '-' ? std::string_view{&sign, &sign + 1} : "");
            auto &out = ctx.out;

            PaddedFormatter::read_width(ctx);

            if (pad_zero) {
                out.write(sign_str);

                if (const auto len = number.size(); len < width()) {
                    const std::string pad(width() - len, '0');
                    out.write(pad);
                }

                out.write(number);
            } else {
                const auto len = sign_str.size() + number.size();

                PaddedFormatter::pre_format(out, len);
                out.write(sign_str);
                out.write(number);
                PaddedFormatter::post_format(out, len);
            }
        }

    };

    struct IntegralFormatter : NumericFormatter {
        char rep;

        IntegralFormatter(char rep) : rep{rep} {}

        void parse(InputIterator &in, bool is_bool) {
            NumericFormatter::parse(in);

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
            case '}':
                break;
            case 's':
                if (is_bool) {
                    rep = *in++;
                    break;
                }
                // fallthrough
            default:
                throw format_error("Expected '}'.");
            }
        }
        void format(FormatContext &ctx, std::function<std::string(int)> f, bool negative) {
            std::string number{};

            PaddedFormatter::read_width(ctx);

            switch (rep) {
            case 'b':
            case 'B':
            case 'x':
            case 'X':
                if (alternate) {
                    number += '0';
                    number += rep;
                }
                number += f(std::tolower(rep) == 'b' ? 2 : 16);
                break;
            case 'c':
            case 'd':
            case '\0':
                number = f(rep == 'c' ? 0 : 10);
                break;
            case 'o':
                if (alternate)
                    number += '0';
                number += f(8);
                break;
            case 's':
                number = f(1);
                break;
            default:
                throw format_error{"Unimplemented operation."};
            }

            NumericFormatter::format(ctx, number, negative);
        }
    };

    struct FloatingPointFormatter : NumericFormatter, PrecisionFormatter {
        char rep{'\0'};

        void parse(InputIterator &in) {
            NumericFormatter::parse(in);
            PrecisionFormatter::parse_prec(in);

            // Parse 'L'.
            if (*in == 'L')
                throw format_error{"Locale-specific formatting is not implemented/supported."};

            // Parse rep.
            switch (*in) {
            case 'a':
            case 'A':
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G':
                rep = *in++;
                break;
            case '}':
                break;
            default:
                throw format_error("Expected '}'.");
            }
        }

        void format(FormatContext &ctx, std::function<std::string(std::optional<std::chars_format>)> f, bool negative) {
            std::optional<std::chars_format> fmt{};

            PaddedFormatter::read_width(ctx);
            PrecisionFormatter::read_prec(ctx);

            switch (rep) {
            case 'a':
            case 'A':
                fmt = std::chars_format::hex;
                break;
            case 'e':
            case 'E':
                fmt = std::chars_format::scientific;
                break;
            case 'f':
            case 'F':
                fmt = std::chars_format::fixed;
                break;
            case 'g':
            case 'G':
                fmt = std::chars_format::general;
                break;
            case '\0':
                if (prec().has_value())
                    fmt = std::chars_format::general;
                break;
            default:
                throw format_error{"Unimplemented operation."};
            }

            if (rep != '\0' && !prec().has_value()) {
                set_prec(6);
            }

            auto number = f(fmt);
            if (std::isupper(rep)) {
                std::for_each(begin(number), end(number), [](char &ch){ ch = std::toupper(static_cast<unsigned char>(ch)); });
            }
            NumericFormatter::format(ctx, number, negative);
        }
    };

    struct StringFormatter : PaddedFormatter, PrecisionFormatter {
        void parse(InputIterator &in) {
            PaddedFormatter::parse(in);
            PrecisionFormatter::parse_prec(in);

            if (*in == 's')
                ++in;
        }

        void format_to(FormatContext &ctx, std::string_view s) {
            PaddedFormatter::read_width(ctx);
            PrecisionFormatter::read_prec(ctx);

            const auto len = prec().has_value() ? std::min(prec().value(), s.length()) : s.length();
            auto &out = ctx.out;

            PaddedFormatter::pre_format(out, len);
            out.write(s.substr(0, len));
            PaddedFormatter::post_format(out, len);
        }
    };
}

// Formatter<T> specializations.
namespace safmat {
    template<std::integral T>
    struct Formatter<T> : internal::IntegralFormatter {
        Formatter(char rep = 'd') : IntegralFormatter{rep} {}

        void parse(InputIterator &in) {
            IntegralFormatter::parse(in, std::is_same_v<T, bool>);
       }

        void format_to(FormatContext &ctx, T x) {
            bool is_negative;

            if constexpr (std::is_signed_v<T>) {
                is_negative = x < T{};
                if (is_negative)
                    x = -x;
            } else {
                is_negative = false;
            }

            const auto f = [x](int base) -> std::string {
                if (base == 0) {
                    return std::string(1, static_cast<char>(x));
                } else if (base == 1) {
                    return x ? "true" : "false";
                }

                char buffer[sizeof (T) * 8 + 2];
                const auto result = std::to_chars(buffer, buffer + sizeof buffer, x, base);
                if (result.ec == std::errc{}) {
                    *result.ptr = '\0';
                    return buffer;
                } else {
                    throw format_error{"Number too big."};
                }
            };

            IntegralFormatter::format(ctx, f, is_negative);
        }
    };

    template<>
    struct Formatter<bool> : Formatter<unsigned> {
        Formatter() : Formatter<unsigned>('s') {}
    };

    template<>
    struct Formatter<char> : Formatter<int> {
        Formatter() : Formatter<int>('c') {}
    };

    template<std::floating_point T>
    struct Formatter<T> : internal::FloatingPointFormatter {
        void format_to(FormatContext &ctx, T x) {
            const auto f = [this, x](std::optional<std::chars_format> fmt) -> std::string {
                const auto v = std::abs(x);

                const auto ilen = std::max(width(), (v != T{}) ?  static_cast<std::size_t>(std::ceil(std::log10(v))) : 1);
                const auto flen = prec().value_or(std::numeric_limits<T>::digits10 * 2);
                const auto len = ilen + flen + 3;

                std::unique_ptr<char []> buffer(new char[len]);

                std::to_chars_result r;
                if (!fmt.has_value()) {
                    r = std::to_chars(buffer.get(), buffer.get() + len, v);
                } else if (prec().has_value()) {
                    r = std::to_chars(buffer.get(), buffer.get() + len, v, fmt.value(), prec().value());
                } else {
                    r = std::to_chars(buffer.get(), buffer.get() + len, v, fmt.value());
                }

                if (r.ec == std::errc{}) {
                    *r.ptr = '\0';
                    return buffer.get();
                } else {
                    throw format_error{"Number too long."};
                }
            };
            format(ctx, f, x < T{});
        }
    };

    template<concepts::StringLike T>
    struct Formatter<T> : internal::StringFormatter {};

    template<Formattable A, Formattable B>
    struct Formatter<std::pair<A, B>> : internal::PaddedFormatter {
        using T = std::pair<A, B>;

        void format_to(FormatContext &ctx, const T &p) {
            const auto &[a, b] = p;
            internal::PaddedFormatter::read_width(ctx);

            const auto f = format("({}, {})", a, b);

            internal::PaddedFormatter::pre_format(ctx.out, f.length());
            ctx.out.write(f);
            internal::PaddedFormatter::post_format(ctx.out, f.length());
        }
    };

    template<concepts::FormattableContainer C>
    struct Formatter<C> : Formatter<concepts::elem_type_t<C>> {
        using T = concepts::elem_type_t<C>;
        using F = Formatter<T>;

        void format_to(FormatContext &ctx, const C &c) {
            auto &out = ctx.out;
            auto it = begin(c);
            const auto e = end(c);

            out.write('[');
            if (it != e) {
                F::format_to(ctx, *it++);
                while (it != e) {
                    out.write(", ");
                    F::format_to(ctx, *it++);
                }
            }
            out.write(']');
        }
    };

#if __cpp_lib_source_location >= 201907L
    template<>
    struct Formatter<std::source_location> : internal::PaddedFormatter {
	void format_to(FormatContext &ctx, const std::source_location &loc) {
	    auto &out = ctx.out;
	    safmat::format_to(out, "{}:{}:{}", loc.file_name(), loc.line(), loc.column());
	}
    };
#endif
}

#endif // FILE_SAFMAT_HPP
