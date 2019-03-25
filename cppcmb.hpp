/**
 * cppcmb.hpp
 *
 * @author Peter Lenkefi
 * @date 2018-09-05
 * @description Generic parser combinators for C++17.
 */

#ifndef CPPCMB_HPP
#define CPPCMB_HPP

#include <cassert>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

/**
 * Macros used by the library.
 */
#define cppcmb_cat(x, y) cppcmb_prelude_cat(x, y)

#define cppcmb_unique_id(prefix) cppcmb_cat(prefix, __LINE__)

#define cppcmb_fwd(...) \
::std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

#define cppcmb_assert_concept(...) \
static_assert(__VA_ARGS__, "Concept assertion " #__VA_ARGS__ " failed!")

#define cppcmb_requires(...) \
template <cppcmb_requires_t(__VA_ARGS__)>

#define cppcmb_requires_t(...) \
cppcmb_prelude_requires_t1(cppcmb_unique_id(concept_req), __VA_ARGS__)

#define cppcmb_return(...) \
noexcept(noexcept(__VA_ARGS__)) -> decltype(__VA_ARGS__) { return __VA_ARGS__; }

#define cppcmb_assert(msg, ...) \
assert(((void)msg, (__VA_ARGS__)))

/**
 * Macro details.
 */
#define cppcmb_prelude_cat(x, y) x ## y

#define cppcmb_prelude_requires_t1(id, ...) 						\
bool id = false,                                                    \
::std::enable_if_t<id || (__VA_ARGS__), ::std::nullptr_t> = nullptr

namespace cppcmb {

    namespace detail {

        /**
         * @see https://en.cppreference.com/w/cpp/experimental/nonesuch
         */
        struct nonesuch {
            ~nonesuch()                     = delete;
            nonesuch(nonesuch const&)       = delete;
            void operator=(nonesuch const&) = delete;
        };

        /**
         * @see https://en.cppreference.com/w/cpp/experimental/is_detected
         */
        template <typename Default, typename AlwaysVoid,
            template <typename...> typename Op, typename... Args>
        struct detector {
            using value_t = std::false_type;
            using type = Default;
        };

        template <typename Default,
            template <typename...> typename Op, typename... Args>
        struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
            using value_t = std::true_type;
            using type = Op<Args...>;
        };

        template <template <typename...> typename Op, typename... Args>
        using is_detected = typename
            detector<nonesuch, void, Op, Args...>::value_t;

        template <template <typename...> typename Op, typename... Args>
        using detected_t = typename
            detector<nonesuch, void, Op, Args...>::type;

        template <typename Default,
            template <typename...> typename Op, typename... Args>
        using detected_or = detector<Default, void, Op, Args...>;

        // Additional utilities

        template <template <typename...> typename Op, typename... Args>
        inline constexpr auto is_detected_v = is_detected<Op, Args...>::value;

        template <typename Default,
            template <typename...> typename Op, typename... Args>
        using detected_or_t = typename detected_or<Default, Op, Args...>::type;

        template <typename Expected,
            template <typename...> typename Op, typename... Args>
        using is_detected_exact =
            std::is_same<Expected, detected_t<Op, Args...>>;

        template <typename Expected,
            template <typename...> typename Op, typename... Args>
        inline constexpr auto is_detected_exact_v =
            is_detected_exact<Expected, Op, Args...>::value;

        template <typename To, template <typename...> typename Op, typename... Args>
        using is_detected_convertible =
            std::is_convertible<detected_t<Op, Args...>, To>;

        template <typename To, template <typename...> typename Op,
            typename... Args>
        inline constexpr auto is_detected_convertible_v =
            is_detected_convertible<To, Op, Args...>::value;

        /**
         * Reader concept checking.
         */
        template <typename T>
        using element_at_t =
            decltype(std::declval<T>()[std::declval<std::size_t>()]);

        template <typename T>
        using msize_t = decltype(std::size(std::declval<T>()));

        // Readable source concept for the reader
        template <typename T>
        inline constexpr bool is_reader_source_v = std::conjunction_v<
            is_detected<element_at_t, T>,
            is_detected<msize_t, T>
        >;

        /**
         * @see https://en.cppreference.com/w/cpp/types/remove_cvref
         */
        template <typename T>
        struct remove_cvref : std::remove_cv<std::remove_reference_t<T>> {};

        template <typename T>
        using remove_cvref_t = typename remove_cvref<T>::type;

    } /* namespace detail */


    /**
     * The structure that reads from the source element-wise.
     */
    template <typename Src>
    class reader {
    public:
        static_assert(
            detail::is_reader_source_v<Src>,
            "The reader source must have a subscript operator [std::size_t] "
            "and a size() member function!"
        );

    private:
        Src const*  m_Source;
        std::size_t m_Cursor;

    public:
        constexpr reader(Src const& src) noexcept
            : m_Source(std::addressof(src)), m_Cursor(0U) {
        }

        reader(Src const&& src) = delete;

        [[nodiscard]] constexpr auto source() const
            cppcmb_return(*m_Source);

        [[nodiscard]] constexpr auto cursor() const
            cppcmb_return(m_Cursor);

        [[nodiscard]] constexpr auto is_end() const
            cppcmb_return(cursor() == std::size(source()));

        [[nodiscard]] constexpr auto current() const
            cppcmb_return(source()[cursor()]);

        constexpr void seek(std::size_t idx) noexcept {
            cppcmb_assert(
                "seek() argument must be in the bounds of source!",
                idx < std::size(source())
            );
            m_Cursor = idx;
        }

        constexpr void next() noexcept {
            cppcmb_assert(
                "next() must not be called when is_end() is true!",
                !is_end()
            );
            seek(cursor() + 1);
        }
    };

    /**
     * Success "type-constructor". The type that the parser returns when it
     * succeeded.
     */
    template <typename T>
    class success {
    public:
        using value_type = T;

    private:
        value_type  m_Value;
        std::size_t m_Remaining;

    public:
        template <typename TFwd>
        constexpr success(TFwd&& val, std::size_t rem)
            noexcept(noexcept(cppcmb_fwd(val)))
            : m_Value(cppcmb_fwd(val)), m_Remaining(rem) {
        }

        [[nodiscard]] constexpr auto value() &
            cppcmb_return(m_Value);
        [[nodiscard]] constexpr auto value() const&
            cppcmb_return(m_Value);
        [[nodiscard]] constexpr auto value() &&
            cppcmb_return(std::move(m_Value));
        [[nodiscard]] constexpr auto value() const&&
            cppcmb_return(std::move(m_Value));

        constexpr auto remaining() const cppcmb_return(m_Remaining);
    };

    template <typename TFwd>
    success(TFwd&&, std::size_t) -> success<detail::remove_cvref_t<TFwd>>;

    /**
     * Failure "type-constructor". The type that the parser returns when it
     * fails.
     */
    class failure {
    private:
        std::size_t m_Furthest;

    public:
        constexpr failure(std::size_t furth) noexcept
            : m_Furthest(furth) {
        }

        [[nodiscard]] constexpr auto furthest() const
            cppcmb_return(m_Furthest);
    };

    /**
     * The result type of a parser. It's either a success or a failure type.
     */
    template <typename T>
    class result {
    public:
        using success_type = success<T>;
        using failure_type = failure;

    private:
        template <typename U>
        static constexpr bool is_self_v =
            std::is_same_v<detail::remove_cvref_t<U>, result>;

        std::variant<success_type, failure_type> m_Data;

    public:
        template <typename TFwd, cppcmb_requires_t(!is_self_v<TFwd>)>
        constexpr result(TFwd&& val) noexcept(noexcept(cppcmb_fwd(val)))
            : m_Data(cppcmb_fwd(val)) {
        }

        [[nodiscard]] constexpr auto is_success() const
            cppcmb_return(std::holds_alternative<success_type>(m_Data));

        [[nodiscard]] constexpr auto is_failure() const
            cppcmb_return(std::holds_alternative<failure_type>(m_Data));

        [[nodiscard]] constexpr auto success() &
            cppcmb_return(std::get<success_type>(m_Data));
        [[nodiscard]] constexpr auto success() const&
            cppcmb_return(std::get<success_type>(m_Data));
        [[nodiscard]] constexpr auto success() &&
            cppcmb_return(std::get<success_type>(std::move(m_Data)));
        [[nodiscard]] constexpr auto success() const&&
            cppcmb_return(std::get<success_type>(std::move(m_Data)));

        [[nodiscard]] constexpr auto failure() &
            cppcmb_return(std::get<failure_type>(m_Data));
        [[nodiscard]] constexpr auto failure() const&
            cppcmb_return(std::get<failure_type>(m_Data));
        [[nodiscard]] constexpr auto failure() &&
            cppcmb_return(std::get<failure_type>(std::move(m_Data)));
        [[nodiscard]] constexpr auto failure() const&&
            cppcmb_return(std::get<failure_type>(std::move(m_Data)));
    };

    namespace detail {

        class combinator_base {
        private:
            static inline std::size_t s_ID_Counter = 0;

            std::size_t m_ID = s_ID_Counter++;

        public:
            [[nodiscard]] constexpr auto parser_id() const
                cppcmb_return(m_ID);
        };

        template <typename Self>
        class crtp {
        public:
            using self_type = Self;

            [[nodiscard]]
            constexpr self_type& self() & noexcept {
                return static_cast<self_type&>(*this);
            }

            [[nodiscard]]
            constexpr self_type const& self() const& noexcept {
                return static_cast<self_type const&>(*this);
            }
        };

    } /* namespace detail */

    /**
     * Check if a type correctly defives from the combinator base.
     */
    template <typename T>
    using is_combinator = std::is_base_of<detail::combinator_base, T>;

    template <typename T>
    inline constexpr bool is_combinator_v = is_combinator<T>::value;

    // Forward-declare the action combinator
    template <typename Cmb, typename Fn>
    class action;

    /**
     * Every combinator must derive from this (so we can inject the subscript
     * operator).
     */
    #define cppcmb_noexcept_subscript(...) \
    noexcept(noexcept(action(std::declval<__VA_ARGS__>(), cppcmb_fwd(fn))))

    template <typename Self>
    class combinator : private detail::crtp<Self>,
                       private detail::combinator_base {
    public:
        template <typename Fn>
        [[nodiscard]] constexpr auto operator[](Fn&& fn) &
        cppcmb_noexcept_subscript(Self&) {
            return action(this->self(), cppcmb_fwd(fn));
        }

        template <typename Fn>
        [[nodiscard]] constexpr auto operator[](Fn&& fn) const&
        cppcmb_noexcept_subscript(Self const&) {
            return action(this->self(), cppcmb_fwd(fn));
        }

        template <typename Fn>
        [[nodiscard]] constexpr auto operator[](Fn&& fn) &&
        cppcmb_noexcept_subscript(Self&&) {
            return action(std::move(this->self()), cppcmb_fwd(fn));
        }

        template <typename Fn>
        [[nodiscard]] constexpr auto operator[](Fn&& fn) const&&
        cppcmb_noexcept_subscript(Self const&&) {
            return action(std::move(this->self()), cppcmb_fwd(fn));
        }
    };

    #undef cppcmb_noexcept_subscript

    /**
     * Action combinator, applies a function to the result when the sub-parser
     * succeeds.
     */
    template <typename Cmb, typename Fn>
    class action : public combinator<action<Cmb, Fn>> {
    private:
        Cmb m_Parser;
        Fn m_Fn;

    public:
        template <typename CmbFwd, typename FnFwd>
        constexpr action(CmbFwd&& cmb, FnFwd&& fn)
            noexcept(noexcept(cppcmb_fwd(cmb)) && noexcept(cppcmb_fwd(fn)))
            : m_Parser(cppcmb_fwd(cmb)), m_Fn(cppcmb_fwd(fn)) {
        }
        // XXX(LPeter1997): Implement
    };

    template <typename CmbFwd, typename FnFwd>
    action(CmbFwd&&, FnFwd&&) -> action<
        detail::remove_cvref_t<CmbFwd>,
        std::decay_t<FnFwd>
    >;

} /* namespace cppcmb */

/**
 * Detail macro undefines.
 */
#undef cppcmb_prelude_requires_t1
#undef cppcmb_prelude_cat

/**
 * Library macro undefines.
 */
#undef cppcmb_assert
#undef cppcmb_return
#undef cppcmb_requires_t
#undef cppcmb_requires
#undef cppcmb_assert_concept
#undef cppcmb_fwd
#undef cppcmb_unique_id
#undef cppcmb_cat

#endif /* CPPCMB_HPP */
