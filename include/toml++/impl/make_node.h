//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once

#include "forward_declarations.h"
#include "header_start.h"

/// \cond
TOML_IMPL_NAMESPACE_START
{
	template <typename T>
	TOML_NODISCARD
	TOML_ATTR(returns_nonnull)
	auto* make_node_specialized(T && val, [[maybe_unused]] value_flags flags)
	{
		using unwrapped_type = unwrap_node<remove_cvref<T>>;
		static_assert(!std::is_same_v<unwrapped_type, node>);
		static_assert(!is_node_view<unwrapped_type>);

		// arrays + tables - invoke copy/move ctor
		if constexpr (is_one_of<unwrapped_type, array, table>)
		{
			return new unwrapped_type{ static_cast<T&&>(val) };
		}

		// values
		else
		{
			using native_type = native_type_of<unwrapped_type>;
			using value_type  = value<native_type>;

			value_type* out;

			// copy/move ctor
			if constexpr (std::is_same_v<remove_cvref<T>, value_type>)
			{
				out = new value_type{ static_cast<T&&>(val) };

				// only override the flags if the new ones are nonzero
				// (so the copy/move ctor does the right thing in the general case)
				if (flags != value_flags::none)
					out->flags(flags);
			}

			// creating from raw value
			else
			{
				static_assert(!is_wide_string<T> || TOML_WINDOWS_COMPAT,
							  "Instantiating values from wide-character strings is only "
							  "supported on Windows with TOML_WINDOWS_COMPAT enabled.");
				static_assert(is_native<unwrapped_type> || is_losslessly_convertible_to_native<unwrapped_type>,
							  "Value initializers must be (or be promotable to) one of the TOML value types");

				if constexpr (is_wide_string<T>)
				{
#if TOML_WINDOWS_COMPAT
					out = new value_type{ narrow(static_cast<T&&>(val)) };
#else
					static_assert(dependent_false<T>, "Evaluated unreachable branch!");
#endif
				}
				else
					out = new value_type{ static_cast<T&&>(val) };

				out->flags(flags);
			}

			return out;
		}
	}

	template <typename T>
	TOML_NODISCARD
	auto* make_node(T && val, value_flags flags = value_flags::none)
	{
		using type = unwrap_node<remove_cvref<T>>;
		if constexpr (std::is_same_v<type, node> || is_node_view<type>)
		{
			if constexpr (is_node_view<type>)
			{
				if (!val)
					return static_cast<toml::node*>(nullptr);
			}

			return static_cast<T&&>(val).visit(
				[flags](auto&& concrete) {
					return static_cast<toml::node*>(
						make_node_specialized(static_cast<decltype(concrete)&&>(concrete), flags));
				});
		}
		else
			return make_node_specialized(static_cast<T&&>(val), flags);
	}

	template <typename T>
	TOML_NODISCARD
	auto* make_node(inserter<T> && val, value_flags flags = value_flags::none)
	{
		return make_node(static_cast<T&&>(val.value), flags);
	}

	template <typename T, bool = (is_node<T> || is_node_view<T> || is_value<T> || can_partially_represent_native<T>)>
	struct inserted_type_of_
	{
		using type = std::remove_pointer_t<decltype(make_node(std::declval<T>()))>;
	};

	template <typename T>
	struct inserted_type_of_<inserter<T>, false>
	{
		using type = typename inserted_type_of_<T>::type;
	};

	template <typename T>
	struct inserted_type_of_<T, false>
	{
		using type = void;
	};
}
TOML_IMPL_NAMESPACE_END;
/// \endcond

TOML_NAMESPACE_START
{
	/// \brief	Metafunction for determining which node type would be constructed
	///			if an object of this type was inserted into a toml::table or toml::array.
	///
	/// \detail \cpp
	/// static_assert(std::is_same_v<toml::inserted_type_of<const char*>, toml::value<std::string>);
	/// static_assert(std::is_same_v<toml::inserted_type_of<int>,         toml::value<int64_t>);
	/// static_assert(std::is_same_v<toml::inserted_type_of<float>,       toml::value<double>);
	/// static_assert(std::is_same_v<toml::inserted_type_of<bool>,        toml::value<bool>);
	/// \ecpp
	///
	/// \note	This will return toml::node for nodes and node_views, even though a more specific node subclass
	///			would actually be inserted. There is no way around this in a compile-time metafunction.
	template <typename T>
	using inserted_type_of = POXY_IMPLEMENTATION_DETAIL(typename impl::inserted_type_of_<impl::remove_cvref<T>>::type);
}
TOML_NAMESPACE_END;

#include "header_end.h"
