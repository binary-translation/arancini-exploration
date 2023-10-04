//
// Created by simon on 04.10.23.
//

#pragma once

template <typename type, unsigned long start_idx, unsigned long search, type idx, type... idxs> struct find_idx {
	static constexpr unsigned long value
		= (search == static_cast<const unsigned long>(idx)) ? start_idx : find_idx<type, start_idx + 1, search, idxs...>::value;
};
template <typename type, unsigned long start_idx, unsigned long search, type idx> struct find_idx<type, start_idx, search, idx> {
	static constexpr unsigned long value = start_idx;
};
template <typename type, type... idx> struct ordered {
	static constexpr unsigned long value[] = { find_idx<type, 0, static_cast<const unsigned long>(idx), idx...>::value... };
};

template <typename type1, type1 arg, typename type2> struct to_type {
	using type = type2;
};

template <typename type1, type1 arg, typename type2> using to_type_t = typename to_type<type1, arg, type2>::type;