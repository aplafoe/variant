#include <iostream>
#include <variant>
#include <vector>
#include <map>

template<typename T, typename...Types>
struct max_alignment {
    static constexpr size_t value = 0;
};

template<typename T>
struct max_alignment<T> {
    static constexpr size_t value = alignof(T);
};

template<typename T, typename U>
struct max_alignment<T, U> {
    static constexpr size_t value = alignof(T) > alignof(U) ? alignof(T) : alignof(U);
};

template<typename T, typename U, typename...Types>
struct max_alignment<T, U, Types...> {
    static constexpr size_t value = alignof(T) < alignof(U) ? alignof(U) : max_alignment<T, Types...>::value;
};

template<typename T, typename...Types>
struct max_size {
    static constexpr size_t value = 0;
};

template<typename T>
struct max_size<T> {
    static constexpr size_t value = sizeof(T);
};

template<typename T, typename U>
struct max_size<T, U> {
    static constexpr size_t value = sizeof(T) > sizeof(U) ? sizeof(T) : sizeof(U);
};

template<typename T, typename U, typename...Types>
struct max_size<T, U, Types...> {
    static constexpr size_t value = sizeof(T) < sizeof(U) ? sizeof(U) : max_size<T, Types...>::value;
};

template<size_t N, typename T, typename...Types>
struct get_index_by_type {
    static constexpr size_t value = -1;
};

template<size_t N, typename T, typename U, typename...Types>
struct get_index_by_type<N, T, U, Types...> {
    static constexpr size_t value = std::is_same_v<T, U> ? N : get_index_by_type<N + 1, T, Types...>::value;
};

template<size_t N, typename...Args>
constexpr decltype(auto) get_value_from_pack_by_index() noexcept {
    using type_t = std::decay_t<std::remove_all_extents_t<decltype(std::get<N>(std::tuple<Args...>{}))>>;
    type_t value = type_t{};
    return value;
}

class bad_variant_access final : public std::exception {
public:
    bad_variant_access() = default;
    const char* what() const noexcept override final {
        return "bad variant access";
    }
};

template<typename...Types>
class variant {
private:
    size_t current_idx = 0;
    alignas(max_alignment<Types...>::value) std::byte data[max_size<Types...>::value];
    template<size_t N>
    constexpr void destruct_impl() noexcept {
        using types = std::tuple<Types...>;
        if constexpr (N == std::tuple_size_v<types>) {
            return;
        }
        else {
            using type = std::tuple_element_t<N, types>;
            if (current_idx == N) {
                reinterpret_cast<type*>(data)->~type();
                return;
            }
            destruct_impl<N + 1>();
        }
    }
    constexpr void destruct() noexcept {
        destruct_impl<0>();
    }
public:
    constexpr explicit variant() noexcept {
        current_idx = 0;
        new(&data) std::byte{};
    }
    template<typename T>
    constexpr variant(T&& arg) noexcept {
        static_assert((std::is_same_v<std::decay_t<T>, std::decay_t<Types>> || ...), "This type can not be used!");
        current_idx = get_index_by_type<0, T, Types...>::value;
        new(&data) T(std::forward<T>(arg));
    }
    template<typename T>
    variant& operator=(T&& arg) noexcept {
        static_assert((std::is_same_v<std::decay_t<T>, std::decay_t<Types>> || ...), "This type can not be used!");
        destruct();
        current_idx = get_index_by_type<0, T, Types...>::value;
        new(&data) T(std::forward<T>(arg));
        return *this;
    }
    constexpr size_t index() const noexcept {
        return current_idx;
    }
    ~variant() {
        destruct();
    }
    void swap(variant<Types...>& var) noexcept {
        std::swap(data, var.data);
        std::swap(current_idx, var.current_idx);
    }
    template<typename T, typename...Args, typename = std::enable_if_t<std::is_constructible_v<T, Args...>>>
    constexpr T& emplace(Args&&...args) {
        static_assert((std::is_same_v<std::decay_t<T>, std::decay_t<Types>> || ...), "This type can not be used!");
        destruct();
        current_idx = get_index_by_type<0, T, Types...>::value;
        new(&data) T(std::forward<Args>(args)...);
        return *(reinterpret_cast<T*>(data));
    }
    template<typename T, typename U>
    constexpr T& emplace(std::initializer_list<U> il) {
        static_assert(std::is_constructible_v<T, U> && (std::is_same_v<std::decay_t<T>, std::decay_t<Types>> || ...), "Type can not construct from std::initialier_list<T>!");
        destruct();
        current_idx = get_index_by_type<0, T, Types...>::value;
        new(&data) T(il);
        return *(reinterpret_cast<T*>(data));
    }
    template<size_t N, typename...Args>
    constexpr std::tuple_element_t<N, std::tuple<Args...>>& emplace(Args&&...args) {
        using type_t = std::tuple_element_t<N, std::tuple<Args...>>;
        static_assert(N < sizeof...(Args) && std::is_constructible_v<type_t, Args...>, "Type can not construct from Args...");
        destruct();
        current_idx = N;
        new(&data) type_t(std::forward<Args>(args)...);
        return *(reinterpret_cast<type_t*>(data));
    }
    template<size_t N, typename U, typename...Args>
    constexpr std::tuple_element_t<N, std::tuple<Types...>>& emplace(std::initializer_list<U> il, Args&&...args) {
        using type_t = std::tuple_element_t<N, std::tuple<Types...>>;
        static_assert(N < sizeof...(Types) && std::is_constructible_v<type_t, U>, "Type can not construct from std::initialier_list<T>!");
        destruct();
        current_idx = N;
        new(&data) type_t(il);
        return *(reinterpret_cast<type_t*>(data));
    }
    template<typename T, typename...Args>
    friend constexpr bool holds_alternative(const variant<Args...>& var) noexcept;
    template<typename T, typename...Args>
    friend constexpr bool holds_alternative(variant<Args...>& var) noexcept;
    template<typename T, typename...Args>
    friend constexpr bool holds_alternative(variant<Args...>&& var) noexcept;
    template<size_t N, typename...Args>
    friend constexpr std::tuple_element_t<N, std::tuple<Args...>>& get(variant<Args...>& var);
    template<typename T, typename...Args>
    friend constexpr T& get(variant<Args...>& var);
    template<size_t N, typename...Args>
    friend constexpr decltype(auto) get(const variant<Args...>& var);
    template<typename T, typename...Args>
    friend constexpr decltype(auto) get(const variant<Args...>& var);
    template<typename T, typename...Args>
    friend constexpr T&& get(variant<Args...>&& var);
    template<size_t N, typename...Args>
    friend constexpr decltype(auto) get(variant<Args...>&& var);
    template<size_t N, typename...Args>
    friend constexpr decltype(auto) get_if(variant<Args...>* var) noexcept;
    template<size_t N, typename...Args>
    friend constexpr decltype(auto) get_if(const variant<Args...>* var) noexcept;
    template<typename T, typename...Args>
    friend constexpr decltype(auto) get_if(variant<Args...>* var) noexcept;
    template<typename T, typename...Args>
    friend constexpr decltype(auto) get_if(const variant<Args...>* var) noexcept;

};

template<typename T, typename...Args>
constexpr bool holds_alternative(const variant<Args...>& var) noexcept {
    return get_index_by_type<0, T, Args...>::value == var.current_t;
}

template<typename T, typename...Args>
constexpr bool holds_alternative(variant<Args...>& var) noexcept {
    return get_index_by_type<0, T, Args...>::value == var.current_t;
}

template<typename T, typename...Args>
constexpr bool holds_alternative(variant<Args...>&& var) noexcept {
    return get_index_by_type<0, T, Args...>::value == var.current_t;
}

template<size_t N, typename...Args>
constexpr std::tuple_element_t<N, std::tuple<Args...>>& get(variant<Args...>& var) {
    if (N != var.current_idx) {
        throw bad_variant_access{};
    }
    using type_t = std::tuple_element_t<N, std::tuple<Args...>>;
    return *(reinterpret_cast<type_t*>(var.data));
}

template<typename T, typename...Args>
constexpr T& get(variant<Args...>& var) {
    static_assert((std::is_same_v<std::decay_t<T>, std::decay_t<Args>> || ...), "This type can not be used!");
    if (get_index_by_type<0, T, Args...>::value != var.current_idx) {
        throw bad_variant_access{};
    }
    return *(reinterpret_cast<T*>(var.data));
}

template<size_t N, typename...Args>
constexpr decltype(auto) get(const variant<Args...>& var) {
    if (N != var.current_idx) {
        throw bad_variant_access{};
    }
    using type_t = std::tuple_element<N, std::tuple<Args...>>;
    return *(reinterpret_cast<const type_t*>(var.data));
}

template<typename T, typename...Args>
constexpr decltype(auto) get(const variant<Args...>& var) {
    if (get_index_by_type<0, T, Args...>::value != var.current_idx) {
        throw bad_variant_access{};
    }
    static_assert((std::is_same_v<std::decay_t<T>, std::decay_t<Args>> || ...), "This type can not be used!");
    return *(reinterpret_cast<const T*>(var.data));
}

template<typename T, typename...Args>
constexpr T&& get(variant<Args...>&& var) {
    static_assert((std::is_same_v<std::decay_t<T>, std::decay_t<Args>> || ...), "This type can not be used!");
    if (get_index_by_type<0, T, Args...>::value != var.current_idx) {
        throw bad_variant_access{};
    }
    return std::forward<T>(*(reinterpret_cast<T*>(var.data)));
}

template<size_t N, typename...Args>
constexpr decltype(auto) get(variant<Args...>&& var) {
    if (N != var.current_idx) {
        throw bad_variant_access{};
    }
    using type_t = std::tuple_element<N, std::tuple<Args...>>;
    return std::forward<type_t>(*(reinterpret_cast<type_t*>(var.data)));
}

template<size_t N, typename...Args>
constexpr decltype(auto) get_if(variant<Args...>* var) noexcept {
    static_assert(N < sizeof...(Args), "Index out of bounds");
    using type_t = std::tuple_element<N, std::tuple<Args...>>;
    if (N == var->current_idx) {
        return reinterpret_cast<type_t*>(var->data);
    } else {
        return std::add_pointer_t<type_t>{nullptr};
    }
}

template<size_t N, typename...Args>
constexpr decltype(auto) get_if(const variant<Args...>* var) noexcept {
    static_assert(N < sizeof...(Args), "Index out of bounds");
    using type_t = std::tuple_element<N, std::tuple<Args...>>;
    if (N == var->current_idx) {
        return reinterpret_cast<const type_t*>(var->data);
    }
    else {
        return std::add_pointer_t<const type_t>{nullptr};
    }
}

template<typename T, typename...Args>
constexpr decltype(auto) get_if(variant<Args...>* var) noexcept {
    static_assert((std::is_same_v<std::decay_t<T>, std::decay_t<Args>> || ...), "This type can not be used!");
    if (get_index_by_type<0, T, Args...>::value == var->current_idx) {
        return reinterpret_cast<T*>(var->data);
    }
    else {
        return std::add_pointer_t<T>{nullptr};
    }
}
template<typename T, typename...Args>
constexpr decltype(auto) get_if(const variant<Args...>* var) noexcept {
    static_assert((std::is_same_v<std::decay_t<T>, std::decay_t<Args>> || ...), "This type can not be used!");
    if (get_index_by_type<0, T, Args...>::value == var->current_idx) {
        return reinterpret_cast<const T*>(var->data);
    }
    else {
        return std::add_pointer_t<const T>{nullptr};
    }
}

int main() {
    variant<int> i = 123;
    std::cout << get<0>(i);
    return 0;
}

