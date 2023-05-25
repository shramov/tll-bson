// SPDX-License-Identifier: MIT

#ifndef _TLL_UTIL_BSON_LIBBSON_H
#define _TLL_UTIL_BSON_LIBBSON_H

#include <bson/bson.h>

#include <tll/scheme.h>
#include <tll/util/memoryview.h>

#include "tll/bson/error-stack.h"
#include "tll/bson/util.h"

namespace tll::bson::libbson {

using Settings = util::Settings;

struct Encoder : public ErrorStack
{
	bson_t _bson = BSON_INITIALIZER;

	~Encoder() { bson_destroy(&_bson); }

	void init() {}

	std::optional<tll::const_memory> encode(const Settings &settings, const tll::scheme::Message * message, const tll_msg_t * msg)
	{
		bson_reinit(&_bson);
		if (settings.seq_key.size())
			bson_append_int64(&_bson, settings.seq_key.data(), settings.seq_key.size(), msg->seq);
		if (settings.mode == Settings::Mode::Flat) {
			bson_append_utf8(&_bson, settings.type_key.data(), settings.type_key.size(), message->name, strlen(message->name));
			if (!encode(&_bson, message, tll::make_view(*msg)))
				return std::nullopt;
		} else {
			bson_t child;
			if (!bson_append_document_begin(&_bson, message->name, strlen(message->name), &child))
				return fail(std::nullopt, "Failed to init nested document");
			if (!encode(&child, message, tll::make_view(*msg)))
				return std::nullopt;
			if (!bson_append_document_end(&_bson, &child))
				return fail(std::nullopt, "Failed to finish nested document");
		}
		return tll::const_memory { bson_get_data(&_bson), _bson.len };
	}

	template <typename Buf>
	bool encode(bson_t * bson, const tll::scheme::Message * message, const Buf & buf);

	template <typename Buf>
	bool encode(bson_t * bson, const tll::scheme::Field * field, std::string_view key, const Buf & buf);

	template <typename Buf>
	bool encode_list(bson_t * bson, const tll::scheme::Field * field, std::string_view key, size_t size, size_t entity, const Buf & buf);
};

template <typename Buf>
bool Encoder::encode(bson_t * bson, const tll::scheme::Message * message, const Buf & buf)
{
	for (auto f = message->fields; f; f = f->next) {
		if (!encode(bson, f, f->name, buf.view(f->offset)))
			return fail_field(false, f);
	}
	return true;
}

template <typename Buf>
bool Encoder::encode(bson_t * bson, const tll::scheme::Field * field, std::string_view key, const Buf & data)
{
	using Field = tll::scheme::Field;
	switch (field->type) {
	case Field::Int8:
		return bson_append_int32(bson, key.data(), key.size(), *data.template dataT<int8_t>());
	case Field::Int16:
		return bson_append_int32(bson, key.data(), key.size(), *data.template dataT<int16_t>());
	case Field::Int32:
		return bson_append_int32(bson, key.data(), key.size(), *data.template dataT<int32_t>());
	case Field::Int64:
		return bson_append_int64(bson, key.data(), key.size(), *data.template dataT<int64_t>());
	case Field::UInt8:
		return bson_append_int32(bson, key.data(), key.size(), *data.template dataT<uint8_t>());
	case Field::UInt16:
		return bson_append_int32(bson, key.data(), key.size(), *data.template dataT<uint16_t>());
	case Field::UInt32:
		return bson_append_int64(bson, key.data(), key.size(), *data.template dataT<uint32_t>());
	case Field::UInt64:
		return fail(false, "uint64 fields are not supported");
	case Field::Double:
		return bson_append_double(bson, key.data(), key.size(), *data.template dataT<double>());
	case Field::Decimal128:
		return bson_append_decimal128(bson, key.data(), key.size(), data.template dataT<bson_decimal128_t>());

	case Field::Bytes:
		if (field->sub_type == Field::ByteString) {
			auto ptr = data.template dataT<char>();
			return bson_append_utf8(bson, key.data(), key.size(), ptr, strnlen(ptr, field->size));
		}
		return bson_append_binary(bson, key.data(), key.size(), BSON_SUBTYPE_BINARY, data.template dataT<uint8_t>(), field->size);

	case Field::Array: {
		auto size = tll::scheme::read_size(field->count_ptr, data);
		if (size < 0)
			return fail(false, "Negative count: {}", size);

		auto af = field->type_array;
		return encode_list(bson, af, key, size, af->size, data.view(af->offset));
	}
	case Field::Pointer: {
		auto ptr = tll::scheme::read_pointer(field, data);
		if (!ptr)
			return fail(false, "Invalid offset ptr version: {}", field->offset_ptr_version);
		if (data.size() < ptr->offset)
			return fail(false, "Offset pointer out of bounds: +{} < {}", ptr->offset, data.size());
		if (field->sub_type == Field::ByteString) {
			if (ptr->size == 0)
				return bson_append_utf8(bson, key.data(), key.size(), "", 0);
			return bson_append_utf8(bson, key.data(), key.size(), data.view(ptr->offset).template dataT<const char>(), ptr->size - 1);
		}
		return encode_list(bson, field->type_ptr, key, ptr->size, ptr->entity, data.view(ptr->offset));
	}
	case Field::Message: {
		bson_t child;
		if (!bson_append_document_begin(bson, key.data(), key.size(), &child))
			return fail(false, "Failed to init document");
		if (!encode(&child, field->type_msg, data))
			return false;
		if (!bson_append_document_end(bson, &child))
			return fail(false, "Failed to finish document");
		return true;
	}
	case Field::Union: {
		auto type = tll::scheme::read_size(field->type_union->type_ptr, data.view(field->type_union->type_ptr->offset));
		if (type < 0 || (size_t) type > field->type_union->fields_size)
			return fail(false, "Union type out of bounds: {} > max {}", type, field->type_union->fields_size);
		auto uf = field->type_union->fields + type;

		bson_t child;
		if (!bson_append_document_begin(bson, key.data(), key.size(), &child))
			return fail(false, "Failed to init document");

		if (!encode(&child, uf, uf->name, data.view(uf->offset)))
			return fail_field(false, uf);
		if (!bson_append_document_end(bson, &child))
			return fail(false, "Failed to finish document");
		return true;
	}
	}
	return false;
}

template <typename Buf>
bool Encoder::encode_list(bson_t * bson, const tll::scheme::Field * field, std::string_view key, size_t size, size_t entity, const Buf & data)
{
	bson_t child;

	if (!bson_append_array_begin(bson, key.data(), key.size(), &child))
		return fail(false, "Failed to init array");

	std::array<char, 10> idxbuf;
	for (auto i = 0u; i < size; i++) {
		if (!encode(&child, field, util::uint_to_string(i, idxbuf), data.view(entity * i)))
			return fail_index(false, i);
	}
	if (!bson_append_array_end(bson, &child))
		return fail(false, "Failed to finalize array");
	return true;
}

struct Decoder : public ErrorStack
{
	template <typename Buf>
	bool decode(bson_iter_t * iter, const tll::scheme::Message * message, Buf buf, const Settings &settings);

	template <typename Buf>
	bool decode(bson_iter_t * iter, const tll::scheme::Message * message, Buf buf);

	template <typename Buf>
	bool decode(bson_iter_t * iter, const tll::scheme::Field * field, Buf buf);

	template <typename Buf>
	bool decode_list(bson_iter_t * iter, const tll::scheme::Field * field, size_t entity, Buf buf);

	template <typename T, typename Buf>
	bool decode_scalar(bson_iter_t * iter, const tll::scheme::Field * field, Buf & buf);

	std::optional<long long> decode_int(bson_iter_t * iter)
	{
			switch (bson_iter_type(iter)) {
			case BSON_TYPE_INT32:
				return bson_iter_int32_unsafe(iter);
			case BSON_TYPE_INT64:
				return bson_iter_int64_unsafe(iter);
			default:
				return std::nullopt;
			}
	}

	std::optional<std::string_view> decode_string(bson_iter_t * iter)
	{
			uint32_t len;
			auto ptr = bson_iter_utf8(iter, &len);
			if (!ptr)
				return std::nullopt;
			return std::string_view(ptr, len);
	}

	const tll::scheme::Field * lookup(const tll::scheme::Message * message, const tll::scheme::Field * field, std::string_view name)
	{
		if (field && field->name == name)
			return field;
		for (auto f = message->fields; f; f = f->next) {
			if (f->name == name)
				return f;
		}
		return nullptr;
	}
};

template <typename Buf>
bool Decoder::decode(bson_iter_t * iter, const tll::scheme::Message * message, Buf buf)
{
	const tll::scheme::Field * field = message->fields;
	do {
		std::string_view key = { bson_iter_key_unsafe(iter), bson_iter_key_len(iter) };
		auto f = lookup(message, field, key);
		if (!f)
			continue;
		field = f;
		if (!decode(iter, field, buf.view(field->offset)))
			return fail_field(false, field);
		field = field->next;
	} while (bson_iter_next(iter));
	return true;
}

template <typename Buf>
bool Decoder::decode(bson_iter_t * iter, const tll::scheme::Message * message, Buf buf, const Settings &settings)
{
	const tll::scheme::Field * field = message->fields;
	do {
		std::string_view key = { bson_iter_key_unsafe(iter), bson_iter_key_len(iter) };
		if (key == settings.type_key)
			continue;
		else if (settings.seq_key.size() && key == settings.seq_key)
			continue;
		auto f = lookup(message, field, key);
		if (!f)
			continue;
		field = f;
		if (!decode(iter, field, buf.view(field->offset)))
			return fail_field(false, field);
		field = field->next;
	} while (bson_iter_next(iter));
	return true;
}

template <typename T, typename Buf>
bool Decoder::decode_scalar(bson_iter_t * iter, const tll::scheme::Field * field, Buf & data)
{
	auto t = bson_iter_type(iter);
	int64_t v ;
	if (t == BSON_TYPE_INT32) {
		v = bson_iter_int32_unsafe(iter);
	} else if (t == BSON_TYPE_INT64) {
		v = bson_iter_int64_unsafe(iter);
	} else
		return fail(false, "Invalid BSON type for integer: {}", t);
	if constexpr (std::is_same_v<T, uint64_t>) {
		if (v < 0)
			return fail(false, "Negative value for unsigned field: {}", v);
	} else {
		if (v > std::numeric_limits<T>::max()) {
			return fail(false, "Invalid value: {} too large", v);
		} else if (v < std::numeric_limits<T>::min()) {
				return fail(false, "Invalid value: {} too small", v);
		}
	}
	*data.template dataT<T>() = v;
	return true;
}

template <typename Buf>
bool Decoder::decode(bson_iter_t * iter, const tll::scheme::Field * field, Buf data)
{
	auto t = bson_iter_type(iter);
	using Field = tll::scheme::Field;
	switch (field->type) {
	case Field::Int8:
		return decode_scalar<int8_t>(iter, field, data);
	case Field::Int16:
		return decode_scalar<int16_t>(iter, field, data);
	case Field::Int32:
		return decode_scalar<int32_t>(iter, field, data);
	case Field::Int64:
		return decode_scalar<int64_t>(iter, field, data);
	case Field::UInt8:
		return decode_scalar<uint8_t>(iter, field, data);
	case Field::UInt16:
		return decode_scalar<uint16_t>(iter, field, data);
	case Field::UInt32:
		return decode_scalar<uint32_t>(iter, field, data);
	case Field::UInt64:
		return decode_scalar<uint64_t>(iter, field, data);
	case Field::Double: {
		if (t == BSON_TYPE_DOUBLE)
			*data.template dataT<double>() = bson_iter_double_unsafe(iter);
		else if (t == BSON_TYPE_INT32)
			*data.template dataT<double>() = bson_iter_int32_unsafe(iter);
		else if (t == BSON_TYPE_INT64)
			*data.template dataT<double>() = bson_iter_int64_unsafe(iter);
		else
			return fail(false, "Invalid BSON type for double: {}", t);
		return true;
	}
	case Field::Decimal128:
		if (t != BSON_TYPE_DECIMAL128)
			return fail(false, "Invalid BSON type for decimal128: {}", t);
		bson_iter_decimal128_unsafe(iter, data.template dataT<bson_decimal128_t>());
		return true;

	case Field::Bytes:
		if (t == BSON_TYPE_UTF8) {
			size_t len;
			auto str = bson_iter_utf8_unsafe(iter, &len);
			if (len > field->size)
				return fail(false, "String for too long: {} > max {}", len, field->size);
			memcpy(data.data(), str, len);
		} else if (t == BSON_TYPE_BINARY) {
			if (field->sub_type == Field::ByteString)
				return fail(false, "Invalid BSON type for string: {}", t);
			uint32_t len;
			const uint8_t * ptr;
			bson_subtype_t sub;
			bson_iter_binary(iter, &sub, &len, &ptr);
			if (len > field->size)
				return fail(false, "Binary data too long: {} > max {}", len, field->size);
			memcpy(data.data(), ptr, len);
		} else
			return fail(false, "Invalid BSON type for bytes: {}", t);
		return true;

	case Field::Array: {
		if (t != BSON_TYPE_ARRAY)
			return fail(false, "Invalid BSON type for array: {}", t);

		const uint8_t * array;
		uint32_t len;
		bson_iter_array(iter, &len, &array);
		bson_iter_t child;
		if (!bson_iter_init_from_data(&child, array, len))
			return fail(false, "Failed to init BSON array iterator");
		unsigned count = 0;
		while (bson_iter_next (&child)) {
			count++;
		}
		if (count > field->count)
			return fail(false, "Array size too large: {} > max {}", count, field->count);

		tll::scheme::write_size(field->count_ptr, data, count);

		auto af = field->type_array;
		bson_iter_init_from_data(&child, array, len);
		return decode_list(&child, af, af->size, data.view(af->offset));
	}
	case Field::Pointer: {
		tll::scheme::generic_offset_ptr_t ptr = {};
		ptr.offset = data.size();
		if (field->sub_type == Field::ByteString) {
			if (t != BSON_TYPE_UTF8)
				return fail(false, "Invalid BSON type for string: {}", t);
			size_t len;
			auto str = bson_iter_utf8_unsafe(iter, &len);
			ptr.size = len + 1;
			ptr.entity = 1;
			tll::scheme::write_pointer(field, data, ptr);
			auto view = data.view(ptr.offset);
			view.resize(ptr.size);
			memcpy(view.data(), str, len);
			*view.view(len).template dataT<char>() = '\0';
			return true;
		}
		if (t != BSON_TYPE_ARRAY)
			return fail(false, "Invalid BSON type for array: {}", t);
		const uint8_t * array;
		uint32_t len;
		bson_iter_array(iter, &len, &array);
		bson_iter_t child;
		if (!bson_iter_init_from_data(&child, array, len))
			return fail(false, "Failed to init BSON array iterator");
		while (bson_iter_next (&child))
			ptr.size++;

		auto af = field->type_ptr;
		ptr.entity = af->size;

		tll::scheme::write_pointer(field, data, ptr);
		auto view = data.view(ptr.offset);
		view.resize(ptr.size * ptr.entity);

		bson_iter_init_from_data(&child, array, len);
		return decode_list(&child, af, ptr.entity, view);
	}
	case Field::Message: {
		if (t != BSON_TYPE_DOCUMENT)
			return fail(false, "Invalid BSON type for message: {}", t);

		const uint8_t * array;
		uint32_t len;
		bson_iter_document(iter, &len, &array);
		bson_iter_t child;
		if (!bson_iter_init_from_data(&child, array, len))
			return fail(false, "Failed to init BSON document iterator");
		if (!bson_iter_next (&child))
			return true;

		return decode(&child, field->type_msg, data);
	}
	case Field::Union: {
		if (t != BSON_TYPE_DOCUMENT)
			return fail(false, "Invalid BSON type for message: {}", t);

		const uint8_t * array;
		uint32_t len;
		bson_iter_document(iter, &len, &array);
		bson_iter_t child;
		if (!bson_iter_init_from_data(&child, array, len))
			return fail(false, "Failed to init BSON document iterator");
		while (bson_iter_next (&child)) {
			std::string_view key = { bson_iter_key_unsafe(&child), bson_iter_key_len(&child) };
			auto ud = field->type_union;
			for (auto i = 0u; i < ud->fields_size; i++) {
				auto uf = ud->fields + i;
				if (uf->name == key) {
					tll::scheme::write_size(ud->type_ptr, data.view(ud->type_ptr->offset), i);
					if (!decode(&child, uf, data.view(uf->offset)))
						return fail_field(false, uf);
					return true;
				}
			}
		}
		return fail(false, "No known fields in union");
	}
	}
	return false;
}

template <typename Buf>
bool Decoder::decode_list(bson_iter_t * iter, const tll::scheme::Field * field, size_t entity, Buf data)
{
	auto i = 0u;
	auto view = data.view(0);
	while (bson_iter_next(iter)) {
		if (!decode(iter, field, view))
			return fail_index(false, i);
		i++;
		view = view.view(entity);
	}
	return true;
}
} // namespace tll::bson::libbson

#endif//_TLL_UTIL_BSON_LIBBSON_H
