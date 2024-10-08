// SPDX-License-Identifier: MIT

#include <tll/channel/codec.h>
#include <tll/channel/module.h>

#include <tll/scheme/util.h>
#include <tll/util/memoryview.h>

#include <bson/bson.h>

#include "tll/bson/util.h"
#include "tll/bson/libbson.h"
#include "tll/bson/encoder.h"

using namespace tll::bson;

constexpr auto format_as(bson_type_t v) noexcept { return static_cast<int>(v); }

class BSON : public tll::channel::Codec<BSON>
{
	using Base = tll::channel::Codec<BSON>;

	util::Settings _settings;

	enum class Encoder { Lib, CPP } _enc_type = Encoder::Lib;

	bson_t _bson_dec = BSON_INITIALIZER;
	cppbson::Encoder _enc_cpp;
	libbson::Encoder _enc_lib;
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

	const tll::scheme::Message * _bson_decode_meta(bson_iter_t *iter, tll_msg_t * out);
};

int BSON::_init(const tll::Channel::Url &url, tll::Channel *parent)
{
	using Mode = util::Settings::Mode;

	auto reader = channel_props_reader(url);

	_enc_type = reader.getT("encoder", Encoder::Lib, {{"libbson", Encoder::Lib}, {"cppbson", Encoder::CPP}});
	_settings.type_key = reader.getT<std::string>("type-key", "_tll_name");
	_settings.seq_key = reader.getT<std::string>("seq-key", "_tll_seq");
	_settings.mode = reader.getT("compose", Mode::Flat, {{"flat", Mode::Flat}, {"nested", Mode::Nested}});
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_enc_type == Encoder::Lib)
		_enc_lib.init();
	else
		_enc_cpp.init();

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

	if (_enc_type == Encoder::Lib) {
		_enc_lib.error_clear();
		if (auto r = _enc_lib.encode(_settings, message, msg); r)
			return r;
		return _log.fail(std::nullopt, "Failed to encode message {} at {}: {}", message->name, _enc_lib.format_stack(), _enc_lib.error);
	} else {
		_enc_cpp.error_clear();
		if (auto r = _enc_cpp.encode(_settings, message, msg); r)
			return r;
		return _log.fail(std::nullopt, "Failed to encode message {} at {}: {}", message->name, _enc_cpp.format_stack(), _enc_cpp.error);
	}
}

std::optional<tll::const_memory> BSON::_bson_decode(const tll_msg_t *msg, tll_msg_t * out)
{
	if (!bson_init_static(&_bson_dec, (const uint8_t *) msg->data, msg->size))
		return _log.fail(std::nullopt, "Failed to bind BSON buffer");
	if (!bson_iter_init(&_bson_iter, &_bson_dec))
		return _log.fail(std::nullopt, "Failed to bind BSON iterator");
	const tll::scheme::Message * message = nullptr;
	switch (_settings.mode) {
	case util::Settings::Mode::Flat: {
		bool reset = false, seq = _settings.seq_key.empty();
		while (bson_iter_next(&_bson_iter)) {
			std::string_view key = { bson_iter_key_unsafe(&_bson_iter), bson_iter_key_len(&_bson_iter) };
			if (key == _settings.type_key) {
				if (message)
					return _log.fail(std::nullopt, "Duplicate key {}", key);
				if (auto name = _dec.decode_string(&_bson_iter); name) {
					message = _scheme->lookup(*name);
					if (!message)
						return _log.fail(std::nullopt, "Message '{}' not found", *name);
				} else
					return _log.fail(std::nullopt, "Non-string type key {}", key);
				out->msgid = message->msgid;
				if (seq)
					break;
			} else if (_settings.seq_key.size() && key == _settings.seq_key) {
				if (auto r = _dec.decode_int(&_bson_iter); r)
					out->seq = *r;
				else
					return _log.fail(std::nullopt, "Non-integer seq key {}: {}", key, bson_iter_type(&_bson_iter));
				seq = true;
				if (message)
					break;
			} else
				reset = true;
		}
		if (!message)
			return _log.fail(std::nullopt, "No type key {} in BSON", _settings.type_key);
		if (reset && bson_iter_init(&_bson_iter, &_bson_dec))
			return _log.fail(std::nullopt, "Failed to bind BSON iterator");
		_buffer_dec.resize(0);
		_buffer_dec.resize(message->size);
		_dec.error_clear();
		if (!_dec.decode(&_bson_iter, message, tll::make_view(_buffer_dec), _settings))
			return _log.fail(std::nullopt, "Failed to decode BSON message at {}: {}", _dec.format_stack(), _dec.error);
	}
	case util::Settings::Mode::Nested: {
		while (bson_iter_next(&_bson_iter)) {
			std::string_view key = { bson_iter_key_unsafe(&_bson_iter), bson_iter_key_len(&_bson_iter) };
			if (_settings.seq_key.size() && key == _settings.seq_key) {
				if (auto r = _dec.decode_int(&_bson_iter); r)
					out->seq = *r;
				else
					return _log.fail(std::nullopt, "Non-integer seq key {}: {}", key, bson_iter_type(&_bson_iter));
				if (message)
					break;
			} else if (message)
				continue;
			auto m = _scheme->lookup(key);
			if (!m)
				continue;
			if (m->msgid == 0)
				continue;
			out->msgid = m->msgid;
			message = m;

			if (bson_iter_type(&_bson_iter) != BSON_TYPE_DOCUMENT)
				return _log.fail(std::nullopt, "Non-document message '{}' key: {}", key, bson_iter_type(&_bson_iter));

			_buffer_dec.resize(0);
			_buffer_dec.resize(message->size);
			_dec.error_clear();

			const uint8_t * array;
			uint32_t len;
			bson_iter_document(&_bson_iter, &len, &array);
			bson_iter_t child;
			if (!bson_iter_init_from_data(&child, array, len))
				return _log.fail(std::nullopt, "Failed to init BSON document iterator");
			if (!bson_iter_next (&child))
				continue;

			if (!_dec.decode(&child, message, tll::make_view(_buffer_dec)))
				return _log.fail(std::nullopt, "Failed to decode BSON message at {}: {}", _dec.format_stack(), _dec.error);
		}
		if (!message)
			return _log.fail(std::nullopt, "No known type in BSON");
	}
	}
	return tll::const_memory { _buffer_dec.data(), _buffer_dec.size() };
}

TLL_DEFINE_IMPL(BSON);

TLL_DEFINE_MODULE(BSON);
