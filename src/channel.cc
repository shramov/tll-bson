// SPDX-License-Identifier: MIT

#include <tll/channel/codec.h>
#include <tll/channel/module.h>

#include <tll/scheme/util.h>
#include <tll/util/memoryview.h>

#include <bson/bson.h>

#include "tll/bson/libbson.h"

using namespace tll::bson;

class BSON : public tll::channel::Codec<BSON>
{
	using Base = tll::channel::Codec<BSON>;

	std::string _type_key;
	std::string _seq_key;

	bson_t _bson_dec = BSON_INITIALIZER;
	libbson::Encoder _enc;
	libbson::Decoder _dec;
	bson_iter_t _bson_iter;

 public:
	static constexpr std::string_view channel_protocol() { return "bson+"; }

	int _init(const tll::Channel::Url &, tll::Channel *parent);

	void _free()
	{
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
};

int BSON::_init(const tll::Channel::Url &url, tll::Channel *parent)
{
	auto reader = channel_props_reader(url);

	_enc.init();
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

	_enc.error_clear();
	if (auto r = _enc.encode(_type_key, _seq_key, message, msg); r)
		return r;
	return _log.fail(std::nullopt, "Failed to encode message {} at {}: {}", message->name, _enc.format_stack(), _enc.error);
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
	if (!message)
		return _log.fail(std::nullopt, "No type key {} in BSON", _type_key);
	if (reset && bson_iter_init(&_bson_iter, &_bson_dec))
		return _log.fail(std::nullopt, "Failed to bind BSON iterator");
	_buffer_dec.resize(0);
	_buffer_dec.resize(message->size);
	if (!_dec.decode(&_bson_iter, message, tll::make_view(_buffer_dec), _type_key, _seq_key, true))
		return _log.fail(std::nullopt, "Failed to decode BSON message at {}: {}", _dec.format_stack(), _dec.error);
	return tll::const_memory { _buffer_dec.data(), _buffer_dec.size() };
}

TLL_DEFINE_IMPL(BSON);

TLL_DEFINE_MODULE(BSON);
