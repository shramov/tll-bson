// SPDX-License-Identifier: MIT

#ifndef _TLL_UTIL_BSON_ENCODER_H
#define _TLL_UTIL_BSON_ENCODER_H

#include <tll/channel.h>
#include <tll/scheme.h>
#include <tll/scheme/util.h>
#include <tll/util/memoryview.h>

#include "tll/bson/cppbson.h"
#include "tll/bson/error-stack.h"
#include "tll/bson/util.h"

namespace tll::bson::cppbson {

struct Encoder : public ErrorStack
{
	std::vector<char> buffer;

	void init()
	{
		buffer.resize(64 * 1024);
	}

	std::optional<tll::const_memory> encode(const util::Settings &settings, const tll::scheme::Message * message, const tll_msg_t * msg)
	{
		Document bson(tll::make_view(buffer));
		if (settings.seq_key.size())
			bson.append_int64(settings.seq_key, (int64_t) msg->seq);
		if (settings.mode == util::Settings::Mode::Flat) {
			bson.append_utf8(settings.type_key, message->name);
			if (!encode(bson, message, tll::make_view(*msg)))
				return std::nullopt;
		} else {
			auto child = bson.append_document(message->name);
			if (!encode(child, message, tll::make_view(*msg)))
				return std::nullopt;
			bson.finish_document(child);
		}
		bson.finish_standalone();
		return tll::const_memory { bson.view.data(), bson.offset };
	}

	template <typename View, typename Buf>
	bool encode(Document<View> &bson, const tll::scheme::Message * message, const Buf & buf);

	template <typename View, typename Buf>
	bool encode(Document<View> &bson, const tll::scheme::Field * field, std::string_view key, const Buf & buf);

	template <typename View, typename Buf>
	bool encode_list(Document<View> &bson, const tll::scheme::Field * field, std::string_view key, size_t size, size_t entity, const Buf & buf);
};

template <typename View, typename Buf>
bool Encoder::encode(Document<View> &bson, const tll::scheme::Message * message, const Buf & buf)
{
	for (auto f = message->fields; f; f = f->next) {
		if (!encode(bson, f, f->name, buf.view(f->offset)))
			return fail_field(false, f);
	}
	return true;
}

template <typename View, typename Buf>
bool Encoder::encode(Document<View> &bson, const tll::scheme::Field * field, std::string_view key, const Buf & data)
{
	using Field = tll::scheme::Field;
	switch (field->type) {
	case Field::Int8:
		bson.append_int32(key, *data.template dataT<int8_t>()); return true;
	case Field::Int16:
		bson.append_int32(key, *data.template dataT<int16_t>()); return true;
	case Field::Int32:
		bson.append_int32(key, *data.template dataT<int32_t>()); return true;
	case Field::Int64:
		bson.append_int64(key, *data.template dataT<int64_t>()); return true;
	case Field::UInt8:
		bson.append_int32(key, *data.template dataT<uint8_t>()); return true;
	case Field::UInt16:
		bson.append_int32(key, *data.template dataT<uint16_t>()); return true;
	case Field::UInt32:
		bson.append_int64(key, *data.template dataT<uint32_t>()); return true;
	case Field::UInt64:
		return fail(false, "uint64 fields are not supported");
	case Field::Double:
		bson.append_double(key, *data.template dataT<double>()); return true;
	case Field::Decimal128:
		bson.append_decimal128(key, data.template dataT<Decimal128>()); return true;

	case Field::Bytes:
		if (field->sub_type == Field::ByteString) {
			auto ptr = data.template dataT<char>();
			bson.append_utf8(key, std::string_view(ptr, strnlen(ptr, field->size)));
		} else
			bson.append_binary(key, Memory { data.template dataT<uint8_t>(), field->size });
		return true;

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
				bson.append_utf8(key, "");
			else
				bson.append_utf8(key, std::string_view(data.view(ptr->offset).template dataT<const char>(), ptr->size - 1));
			return true;
		}
		return encode_list(bson, field->type_ptr, key, ptr->size, ptr->entity, data.view(ptr->offset));
	}
	case Field::Message: {
		auto child = bson.append_document(key);
		if (!encode(child, field->type_msg, data))
			return false;
		bson.finish_document(child);
		return true;
	}
	case Field::Union: {
		auto type = tll::scheme::read_size(field->type_union->type_ptr, data.view(field->type_union->type_ptr->offset));
		if (type < 0 || (size_t) type > field->type_union->fields_size)
			return fail(false, "Union type out of bounds: {}", type);
		auto uf = field->type_union->fields + type;

		auto child = bson.append_document(key);
		if (!encode(child, uf, uf->name, data.view(uf->offset)))
			return fail_field(false, uf);
		bson.finish_document(child);
		return true;
	}
	}
	return false;
}

template <typename View, typename Buf>
bool Encoder::encode_list(Document<View> &bson, const tll::scheme::Field * field, std::string_view key, size_t size, size_t entity, const Buf & data)
{
	auto child = bson.append_array(key);

	std::array<char, 10> idxbuf;
	for (auto i = 0u; i < size; i++) {
		if (!encode(child, field, util::uint_to_string(i, idxbuf), data.view(entity * i)))
			return fail_index(false, i);
	}
	bson.finish_document(child);
	return true;
}

} // namespace tll::bson::cppbson

#endif//_TLL_UTIL_BSON_ENCODER_H
