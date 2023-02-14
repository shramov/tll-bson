#include <tll/channel/codec.h>
#include <tll/channel/module.h>

#include <tll/scheme/util.h>
#include <tll/util/memoryview.h>

#include <bson/bson.h>

struct bson_delete { void operator ()(bson_t *ptr) const { bson_destroy(ptr); } };

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

class BSON : public tll::channel::Codec<BSON>
{
	using Base = tll::channel::Codec<BSON>;

	std::string _type_key;
	std::string _seq_key;

	bson_t _bson_enc = BSON_INITIALIZER;
	bson_t _bson_dec = BSON_INITIALIZER;
	bson_iter_t _bson_iter;

 public:
	static constexpr std::string_view channel_protocol() { return "bson+"; }

	int _init(const tll::Channel::Url &, tll::Channel *parent);

	void _free()
	{
		bson_destroy(&_bson_enc);
		bson_destroy(&_bson_dec);
	}

	const tll_msg_t * _encode(const tll_msg_t *msg)
	{
		tll_msg_copy_info(&_msg_enc, msg);
		auto r = _bson_encode(msg, &_msg_enc);
		if (!r)
			return _log.fail(nullptr, "Failed to encode BSON");
		_msg_enc.data = r->data;
		_msg_enc.size = r->size;
		return &_msg_enc;
	}

	const tll_msg_t * _decode(const tll_msg_t *msg)
	{
		tll_msg_copy_info(&_msg_dec, msg);
		auto r = _bson_decode(msg, &_msg_dec);
		if (!r)
			return _log.fail(nullptr, "Failed to decode BSON");
		_msg_dec.data = r->data;
		_msg_dec.size = r->size;
		return &_msg_dec;
	}

	int _on_active()
	{
		if (_init_scheme(_child->scheme()))
			return _log.fail(EINVAL, "Failed to initialize scheme");
		return Base::_on_active();
	}

	int _init_scheme(const tll::scheme::Scheme *s);
	std::optional<tll::const_memory> _bson_encode(const tll_msg_t *msg, tll_msg_t * out);
	std::optional<tll::const_memory> _bson_decode(const tll_msg_t *msg, tll_msg_t * out);

	template <typename Buf>
	bool _encode(bson_t * bson, const tll::scheme::Message * message, const Buf & buf);

	template <typename Buf>
	bool _encode(bson_t * bson, const tll::scheme::Field * field, std::string_view key, const Buf & buf);

	template <typename Buf>
	bool _encode_list(bson_t * bson, const tll::scheme::Field * field, std::string_view key, size_t size, size_t entity, const Buf & buf);

	template <typename Buf>
	bool _decode(bson_iter_t * iter, const tll::scheme::Message * message, Buf buf, bool root = false);

	template <typename Buf>
	bool _decode(bson_iter_t * iter, const tll::scheme::Field * field, Buf buf);

	template <typename Buf>
	bool _decode_list(bson_iter_t * iter, const tll::scheme::Field * field, size_t entity, Buf buf);

	template <typename T, typename Buf>
	bool _decode_scalar(bson_iter_t * iter, const tll::scheme::Field * field, Buf & buf);
};

int BSON::_init(const tll::Channel::Url &url, tll::Channel *parent)
{
	auto reader = channel_props_reader(url);

	_type_key = reader.getT<std::string>("type-key", "_tll_name");
	_seq_key = reader.getT<std::string>("seq-key", "_tll_seq");
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	return Base::_init(url, parent);
}

int BSON::_init_scheme(const tll::scheme::Scheme *s)
{
	if (!s)
		return _log.fail(EINVAL, "BSON codec need scheme");
	_scheme.reset(tll_scheme_ref(s));
	return 0;
}

std::optional<tll::const_memory> BSON::_bson_encode(const tll_msg_t *msg, tll_msg_t * out)
{
	auto message = _scheme->lookup(msg->msgid);
	if (!message)
		return _log.fail(std::nullopt, "Message {} not found", msg->msgid);
	bson_reinit(&_bson_enc);
	bson_append_utf8(&_bson_enc, _type_key.data(), _type_key.size(), message->name, strlen(message->name));
	if (_seq_key.size())
		bson_append_int64(&_bson_enc, _seq_key.data(), _seq_key.size(), msg->seq);
	if (!_encode(&_bson_enc, message, tll::make_view(*msg)))
		return _log.fail(std::nullopt, "Failed to encode message {}", message->name);
	return tll::const_memory { bson_get_data(&_bson_enc), _bson_enc.len };
}

std::optional<tll::const_memory> BSON::_bson_decode(const tll_msg_t *msg, tll_msg_t * out)
{
	if (!bson_init_static(&_bson_dec, (const uint8_t *) msg->data, msg->size))
		return _log.fail(std::nullopt, "Failed to bind BSON buffer");
	if (!bson_iter_init(&_bson_iter, &_bson_dec))
		return _log.fail(std::nullopt, "Failed to bind BSON iterator");
	bool reset = false, seq = _seq_key.size() == 0;
	const tll::scheme::Message * message = nullptr;
	while (bson_iter_next(&_bson_iter)) {
		std::string_view key = { bson_iter_key_unsafe(&_bson_iter), bson_iter_key_len(&_bson_iter) };
		if (key == _type_key) {
			if (message)
				return _log.fail(std::nullopt, "Duplicate key {}", key);
			uint32_t len;
			auto ptr = bson_iter_utf8(&_bson_iter, &len);
			if (!ptr)
				return _log.fail(std::nullopt, "Non-string type key {}", key);
			std::string name = { ptr, len };
			message = _scheme->lookup(name);
			if (!message)
				return _log.fail(std::nullopt, "Message '{}' not found", name);
			out->msgid = message->msgid;
			if (seq)
				break;
		} else if (_seq_key.size() && key == _seq_key) {
			switch (bson_iter_type(&_bson_iter)) {
			case BSON_TYPE_INT32:
				out->seq = bson_iter_int32_unsafe(&_bson_iter);
				break;
			case BSON_TYPE_INT64:
				out->seq = bson_iter_int64_unsafe(&_bson_iter);
				break;
			default:
				return _log.fail(std::nullopt, "Non-integer seq key {}: {}", key, bson_iter_type(&_bson_iter));
			}
			seq = true;
			if (message)
				break;
		} else
			reset = true;
	}
	if (reset && bson_iter_init(&_bson_iter, &_bson_dec))
		return _log.fail(std::nullopt, "Failed to bind BSON iterator");
	_buffer_dec.resize(0);
	_buffer_dec.resize(message->size);
	if (!_decode(&_bson_iter, message, tll::make_view(_buffer_dec), true))
		return _log.fail(std::nullopt, "Failed to decode BSON message");
	return tll::const_memory { _buffer_dec.data(), _buffer_dec.size() };
}

template <typename Buf>
bool BSON::_encode(bson_t * bson, const tll::scheme::Message * message, const Buf & buf)
{
	for (auto f = message->fields; f; f = f->next) {
		if (!_encode(bson, f, f->name, buf.view(f->offset)))
			return _log.fail(false, "Failed to encode field {}", f->name);
	}
	return true;
}

template <typename Buf>
bool BSON::_encode(bson_t * bson, const tll::scheme::Field * field, std::string_view key, const Buf & data)
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
		return _log.fail(false, "uint64 fields are not supported");
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
			return _log.fail(false, "Negative count for field {}: {}", field->name, size);

		auto af = field->type_array;
		if (!_encode_list(bson, af, key, size, af->size, data.view(af->offset)))
			return _log.fail(false, "Failed to encode array {}", field->name);
		return true;
	}
	case Field::Pointer: {
		auto ptr = tll::scheme::read_pointer(field, data);
		if (!ptr)
			return _log.fail(false, "Invalid offset ptr version for {}: {}", field->name, field->offset_ptr_version);
		if (data.size() < ptr->offset)
			return _log.fail(false, "Offset pointer {} out of bounds: +{} < {}", field->name, ptr->offset, data.size());
		if (field->sub_type == Field::ByteString) {
			if (ptr->size == 0)
				return bson_append_utf8(bson, key.data(), key.size(), "", 0);
			return bson_append_utf8(bson, key.data(), key.size(), data.view(ptr->offset).template dataT<const char>(), ptr->size - 1);
		}
		if (!_encode_list(bson, field->type_ptr, key, ptr->size, ptr->entity, data.view(ptr->offset)))
			return _log.fail(false, "Failed to encode array {}", field->name);
		return true;
	}
	case Field::Message: {
		bson_t child;
		if (!bson_append_document_begin(bson, key.data(), key.size(), &child))
			return _log.fail(false, "Failed to init map for {}", field->name);
		if (!_encode(&child, field->type_msg, data))
			return _log.fail(false, "Failed to encode submessage {}", field->name);
		if (!bson_append_document_end(bson, &child))
			return _log.fail(false, "Failed to init map for {}", field->name);
		return true;
	}
	case Field::Union: {
		auto type = tll::scheme::read_size(field->type_union->type_ptr, data.view(field->type_union->type_ptr->offset));
		if (type < 0 || (size_t) type > field->type_union->fields_size)
			return _log.fail(false, "Union type out of bounds for {}: {}", field->type, type);
		auto uf = field->type_union->fields + type;

		bson_t child;
		if (!bson_append_document_begin(bson, key.data(), key.size(), &child))
			return _log.fail(false, "Failed to init map for {}", field->name);

		if (!_encode(&child, uf, uf->name, data.view(uf->offset)))
			return _log.fail(false, "Failed to encode union {} field {}", field->name, uf->name);
		if (!bson_append_document_end(bson, &child))
			return _log.fail(false, "Failed to init map for {}", field->name);
		return true;
	}
	}
	return false;
}

template <typename Buf>
bool BSON::_encode_list(bson_t * bson, const tll::scheme::Field * field, std::string_view key, size_t size, size_t entity, const Buf & data)
{
	bson_t child;

	if (!bson_append_array_begin(bson, key.data(), key.size(), &child))
		return _log.fail(false, "Failed to init array for {}", field->name);

	std::array<char, 10> idxbuf;
	for (auto i = 0u; i < size; i++) {
		if (!_encode(&child, field, uint_to_string(i, idxbuf), data.view(entity * i)))
			return _log.fail(false, "Failed to encode array element {}", i);
	}
	if (!bson_append_array_end(bson, &child))
		return _log.fail(false, "Failed to finalize array for {}", field->name);
	return true;
}

template <typename Buf>
bool BSON::_decode(bson_iter_t * iter, const tll::scheme::Message * message, Buf buf, bool root)
{
	auto field = message->fields;
	do {
		std::string_view key = { bson_iter_key_unsafe(iter), bson_iter_key_len(iter) };
		if (root) {
			if (key == _type_key)
				continue;
			else if (_seq_key.size() && key == _seq_key)
				continue;
		}
		if (field->name != key) {
			auto f = message->fields;
			for (; f; f = f->next) {
				if (f->name == key)
					break;
			}
			if (!f)
				continue;
			field = f;
		}
		if (!_decode(iter, field, buf.view(field->offset)))
			return _log.fail(false, "Failed to decode field {}", field->name);
	} while (bson_iter_next(iter));
	return true;
}

template <typename T, typename Buf>
bool BSON::_decode_scalar(bson_iter_t * iter, const tll::scheme::Field * field, Buf & data)
{
	auto t = bson_iter_type(iter);
	int64_t v ;
	if (t == BSON_TYPE_INT32) {
		v = bson_iter_int32_unsafe(iter);
	} else if (t == BSON_TYPE_INT64) {
		v = bson_iter_int64_unsafe(iter);
	} else
		return _log.fail(false, "Invalid BSON type for integer {}: {}", field->name, t);
	if constexpr (std::is_same_v<T, uint64_t>) {
		if (v < 0)
			return _log.fail(false, "Negative value for unsigned field {}: {}", field->name, v);
	} else {
		if (v > std::numeric_limits<T>::max()) {
			return _log.fail(false, "Invalid value for {}: {} too large", field->name, v);
		} else if (v < std::numeric_limits<T>::min()) {
				return _log.fail(false, "Invalid value for {}: {} too small", field->name, v);
		}
	}
	*data.template dataT<T>() = v;
	return true;
}

template <typename Buf>
bool BSON::_decode(bson_iter_t * iter, const tll::scheme::Field * field, Buf data)
{
	auto t = bson_iter_type(iter);
	using Field = tll::scheme::Field;
	switch (field->type) {
	case Field::Int8:
		return _decode_scalar<int8_t>(iter, field, data);
	case Field::Int16:
		return _decode_scalar<int16_t>(iter, field, data);
	case Field::Int32:
		return _decode_scalar<int32_t>(iter, field, data);
	case Field::Int64:
		return _decode_scalar<int64_t>(iter, field, data);
	case Field::UInt8:
		return _decode_scalar<uint8_t>(iter, field, data);
	case Field::UInt16:
		return _decode_scalar<uint16_t>(iter, field, data);
	case Field::UInt32:
		return _decode_scalar<uint32_t>(iter, field, data);
	case Field::UInt64:
		return _decode_scalar<uint64_t>(iter, field, data);
	case Field::Double: {
		if (t == BSON_TYPE_DOUBLE)
			*data.template dataT<double>() = bson_iter_double_unsafe(iter);
		else if (t == BSON_TYPE_INT32)
			*data.template dataT<double>() = bson_iter_int32_unsafe(iter);
		else if (t == BSON_TYPE_INT64)
			*data.template dataT<double>() = bson_iter_int64_unsafe(iter);
		else
			return _log.fail(false, "Invalid BSON type for double {}: {}", field->name, t);
		return true;
	}
	case Field::Decimal128:
		if (t != BSON_TYPE_DECIMAL128)
			return _log.fail(false, "Invalid BSON type for decimal128 {}: {}", field->name, t);
		bson_iter_decimal128_unsafe(iter, data.template dataT<bson_decimal128_t>());
		return true;

	case Field::Bytes:
		if (t == BSON_TYPE_UTF8) {
			size_t len;
			auto str = bson_iter_utf8_unsafe(iter, &len);
			if (len > field->size)
				return _log.fail(false, "String for field {} is too long: {} > max {}", field->name, len, field->size);
			memcpy(data.data(), str, len);
		} else if (t == BSON_TYPE_BINARY) {
			if (field->sub_type == Field::ByteString)
				return _log.fail(false, "Invalid BSON type for string {}: {}", field->name, t);
			uint32_t len;
			const uint8_t * ptr;
			bson_subtype_t sub;
			bson_iter_binary(iter, &sub, &len, &ptr);
			if (len > field->size)
				return _log.fail(false, "Binary data for field {} is too long: {} > max {}", field->name, len, field->size);
			memcpy(data.data(), ptr, len);
		} else
			return _log.fail(false, "Invalid BSON type for bytes {}: {}", field->name, t);
		return true;

	case Field::Array: {
		if (t != BSON_TYPE_ARRAY)
			return _log.fail(false, "Invalid BSON type for array {}: {}", field->name, t);

		const uint8_t * array;
		uint32_t len;
		bson_iter_array(iter, &len, &array);
		bson_iter_t child;
		if (!bson_iter_init_from_data(&child, array, len))
			return _log.fail(false, "Failed to init BSON array iterator for {}", field->name);
		unsigned count = 0;
		while (bson_iter_next (&child)) {
			count++;
		}
		if (count > field->count)
			return _log.fail(false, "Array size for {} is too large: {} > max {}", field->name, count, field->count);

		tll::scheme::write_size(field->count_ptr, data, count);

		auto af = field->type_array;
		bson_iter_init_from_data(&child, array, len);
		if (!_decode_list(&child, af, af->size, data.view(af->offset)))
			return _log.fail(false, "Failed to decode array {}", field->name);
		return true;
	}
	case Field::Pointer: {
		tll::scheme::generic_offset_ptr_t ptr = {};
		ptr.offset = data.size();
		if (field->sub_type == Field::ByteString) {
			if (t != BSON_TYPE_UTF8)
				return _log.fail(false, "Invalid BSON type for string {}: {}", field->name, t);
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
			return _log.fail(false, "Invalid BSON type for array {}: {}", field->name, t);
		const uint8_t * array;
		uint32_t len;
		bson_iter_array(iter, &len, &array);
		bson_iter_t child;
		if (!bson_iter_init_from_data(&child, array, len))
			return _log.fail(false, "Failed to init BSON array iterator for {}", field->name);
		while (bson_iter_next (&child))
			ptr.size++;

		auto af = field->type_ptr;
		ptr.entity = af->size;

		tll::scheme::write_pointer(field, data, ptr);
		auto view = data.view(ptr.offset);
		view.resize(ptr.size * ptr.entity);

		bson_iter_init_from_data(&child, array, len);
		if (!_decode_list(&child, af, ptr.entity, view))
			return _log.fail(false, "Failed to decode array {}", field->name);
		return true;
	}
	case Field::Message: {
		if (t != BSON_TYPE_DOCUMENT)
			return _log.fail(false, "Invalid BSON type for message {}: {}", field->name, t);

		const uint8_t * array;
		uint32_t len;
		bson_iter_document(iter, &len, &array);
		bson_iter_t child;
		if (!bson_iter_init_from_data(&child, array, len))
			return _log.fail(false, "Failed to init BSON document iterator for {}", field->name);
		if (!bson_iter_next (&child))
			return true;

		return _decode(&child, field->type_msg, data);
	}
	case Field::Union: {
		if (t != BSON_TYPE_DOCUMENT)
			return _log.fail(false, "Invalid BSON type for message {}: {}", field->name, t);

		const uint8_t * array;
		uint32_t len;
		bson_iter_document(iter, &len, &array);
		bson_iter_t child;
		if (!bson_iter_init_from_data(&child, array, len))
			return _log.fail(false, "Failed to init BSON document iterator for {}", field->name);
		while (bson_iter_next (&child)) {
			std::string_view key = { bson_iter_key_unsafe(&child), bson_iter_key_len(&child) };
			auto ud = field->type_union;
			for (auto i = 0u; i < ud->fields_size; i++) {
				auto uf = ud->fields + i;
				_log.debug("Check union field {}: {}", uf->name, key);
				if (uf->name == key) {
					tll::scheme::write_size(ud->type_ptr, data.view(ud->type_ptr->offset), i);
					if (!_decode(&child, uf, data.view(uf->offset)))
						return _log.fail(false, "Failed to decode union {} field {}", field->name, uf->name);
					return true;
				}
			}
		}
		return _log.fail(false, "No known fields in union {}", field->name);
	}
	}
	return false;
}

template <typename Buf>
bool BSON::_decode_list(bson_iter_t * iter, const tll::scheme::Field * field, size_t entity, Buf data)
{
	auto i = 0u;
	auto view = data.view(0);
	while (bson_iter_next(iter)) {
		if (!_decode(iter, field, view))
			return _log.fail(false, "Failed to decode array element {}", i);
		view = view.view(entity);
	}
	return true;
}

TLL_DEFINE_IMPL(BSON);

TLL_DEFINE_MODULE(BSON);
