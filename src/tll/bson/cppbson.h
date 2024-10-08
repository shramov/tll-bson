// SPDX-License-Identifier: MIT

#ifndef _TLL_UTIL_BSON_CPPBSON_H
#define _TLL_UTIL_BSON_CPPBSON_H

#include <array>
#include <cstdint>
#include <string_view>

namespace tll::bson::cppbson {

struct Memory
{
	const void * data = nullptr;
	size_t size = 0;
};

enum class Type : uint8_t
{
	Double = 0x01,
	UTF8 = 0x02,
	Document = 0x03,
	Array = 0x04,
	Binary = 0x05,
	Int32 = 0x10,
	Int64 = 0x12,
	Decimal128 = 0x13,
};

using Decimal128 = std::array<uint8_t, 16>;

template <typename T>
struct data_type {};

template <> struct data_type<int32_t> { static constexpr auto value = Type::Int32; };
template <> struct data_type<int64_t> { static constexpr auto value = Type::Int64; };
template <> struct data_type<double> { static constexpr auto value = Type::Double; };

template <typename T>
static constexpr auto data_type_v = data_type<T>::value;

template <typename View>
struct Document
{
	View view;
	unsigned offset = 4;

	Document(View v) : view(v) {}

	void ensure_size(size_t size)
	{
		if (view.size() < offset + size)
			view.resize(offset + size);
	}

	void append_raw(const void * data, size_t size)
	{
		ensure_size(size);
		append_raw_nocheck(data, size);
	}

	void append_raw_nocheck(const void * data, size_t size)
	{
		memcpy(view.view(offset).data(), data, size);
		offset += size;
	}

	void append_key(Type type, std::string_view key)
	{
		ensure_size(1 + key.size() + 1);
		return append_key_nocheck(type, key);
	}

	void append_key_nocheck(Type type, std::string_view key)
	{
		auto v = view.view(offset);
		*v.template dataT<Type>() = type;
		memcpy(v.view(1).data(), key.data(), key.size());
		*v.view(1 + key.size()).template dataT<char>() = 0;
		offset += 1 + key.size() + 1;
	}

	void append(Type type, std::string_view key, const void * data, size_t size)
	{
		ensure_size(1 + key.size() + 1 + size);
		append_nocheck(type, key, data, size);
	}

	void append_nocheck(Type type, std::string_view key, const void * data, size_t size)
	{
		append_key_nocheck(type, key);
		append_raw_nocheck(data, size);
	}

	template <typename T>
	void append_scalar(std::string_view key, T value)
	{
		ensure_size(1 + key.size() + 1 + sizeof(value));
		append_scalar_nocheck<T>(key, value);
	}

	template <typename T>
	void append_scalar_nocheck(std::string_view key, T value)
	{
		append_key(data_type_v<T>, key);
		ensure_size(sizeof(value));
		*view.view(offset).template dataT<T>() = value;
		offset += sizeof(value);
	}

	void append_int32(std::string_view key, int32_t value) { append_scalar(key, value); }
	void append_int64(std::string_view key, int64_t value) { append_scalar(key, value); }
	void append_double(std::string_view key, double value) { append_scalar(key, value); }

	void append_decimal128(std::string_view key, const Decimal128 * value)
	{
		append(Type::Decimal128, key, value, sizeof(*value));
	}

	void append_utf8(std::string_view key, std::string_view value)
	{
		ensure_size(1 + key.size() + 1 + 4 + value.size() + 1);
		append_utf8_nocheck(key, value);
	}

	void append_utf8_nocheck(std::string_view key, std::string_view value)
	{
		append_key_nocheck(Type::UTF8, key);
		*view.view(offset).template dataT<int32_t>() = value.size() + 1;
		offset += 4;
		append_raw_nocheck(value.data(), value.size());
		*view.view(offset).template dataT<char>() = 0;
		offset++;
	}

	void append_binary(std::string_view key, const Memory & value)
	{
		ensure_size(1 + key.size() + 1 + 4 + value.size);
		append_binary_nocheck(key, value);
	}

	void append_binary_nocheck(std::string_view key, const Memory & value)
	{
		append_key_nocheck(Type::Binary, key);
		*view.view(offset).template dataT<int32_t>() = value.size;
		offset += 4;
		*view.view(offset++).template dataT<char>() = 0;
		append_raw_nocheck(value.data, value.size);
	}

	Document append_document(std::string_view key)
	{
		append_key(Type::Document, key);
		return Document(view.view(offset));
	}

	Document append_array(std::string_view key)
	{
		append_key(Type::Array, key);
		return Document(view.view(offset));
	}

	void finish_document(Document &child)
	{
		child.finish_standalone();
		offset += child.offset;
	}

	void finish_standalone()
	{
		*view.view(offset).template dataT<int8_t>() = 0;
		*view.template dataT<int32_t>() = ++offset;
	}
};

struct ArrayIndex
{
	std::array<char, 16> idxbuf;
	unsigned idx = 0;

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

	std::string_view next() { return uint_to_string(idx++, idxbuf); }
};

} // namespace tll::bson::cppbson

#endif//_TLL_UTIL_BSON_CPPBSON_H
