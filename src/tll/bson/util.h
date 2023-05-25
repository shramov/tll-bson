// SPDX-License-Identifier: MIT

#ifndef _TLL_UTIL_BSON_UTIL_H
#define _TLL_UTIL_BSON_UTIL_H

namespace tll::bson::util {

struct Settings
{
	std::string type_key;
	std::string seq_key;
	enum class Mode {
		Flat, // {seq: 100, type: name, fields...}
		Nested, // {seq: 100, name: {fields...}}
	} mode = Mode::Flat;
};

template <typename I, typename Buf>
std::string_view uint_to_string(I v, Buf &buf)
{
	auto end = ((char *) buf.data()) + buf.size();
	auto ptr = end;
	do {
		I r = v % 10;
		v /= 10;
		*--ptr = '0' + r;
	} while (v);
	return std::string_view(ptr, end - ptr);
}

} // namespace tll::bson::util

#endif//_TLL_UTIL_BSON_UTIL_H
