// SPDX-License-Identifier: MIT

#include "bench-scheme.h"

#include <tll/channel.h>
#include <tll/channel/base.h>
#include <tll/channel/module.h>

#include <tll/util/bench.h>
#include <tll/util/time.h>

extern "C" tll_channel_module_t * tll_channel_module();

static constexpr auto count = 100000u;

using namespace std::chrono;

class Echo : public tll::channel::Base<Echo>
{
 public:
	static constexpr std::string_view channel_protocol() { return "echo"; }
	static constexpr auto process_policy() { return tll::channel::Base<Echo>::ProcessPolicy::Never; }

	int _post(const tll_msg_t *msg, int flags) { return _callback_data(msg); }
};

TLL_DEFINE_IMPL(Echo);

template <typename Buf>
void fill_simple(tll_msg_t &msg, Buf &buf)
{
	auto simple = Simple::bind(buf);
	simple.view().resize(simple.meta_size());

	auto header = simple.get_header();
	header.set_id0(0);
	header.set_id1(100);
	header.set_id2(200);
	header.set_id3(300);
	header.set_ts0(time_point_cast<duration<int64_t, std::micro>>(tll::time::now()));
	header.set_ts1(time_point_cast<duration<int64_t, std::micro>>(tll::time::now()));
	header.set_string0("short");
	header.set_string1("longer string");
	header.set_e0(Header::Enum1::A);
	header.set_e1(Header::Enum1::B);

	simple.set_string0("body0");
	simple.set_string1("longer body1 string");
	simple.set_string2("body2");
	simple.set_string3("longer body3 string");
	simple.set_string4("body4");

	simple.set_f0(tll::util::FixedPoint<int64_t, 8>(123.123));
	simple.set_f1(tll::util::FixedPoint<int64_t, 8>(123.123));
	simple.set_f2(tll::util::FixedPoint<int64_t, 8>(123.123));
	simple.set_f3(tll::util::FixedPoint<int64_t, 8>(123.123));

	auto trailer = simple.get_trailer();
	trailer.set_message("end of simple message");

	msg.data = simple.view().data();
	msg.size = simple.view().size();
	msg.msgid = simple.meta_id();
}

void bench(tll::channel::Context &ctx, std::string_view proto, std::string_view encoder = "")
{
	std::vector<char> buf;
	tll_msg_t msg = {};
	fill_simple(msg, buf);

	tll::Channel::Url url;
	url.proto(proto);
	url.set("scheme", scheme_string);
	url.set("name", "codec");
	if (encoder.size())
		url.set("encoder", encoder);

	auto c = ctx.channel(url);
	if (!c)
		return;
	c->open();
	if (c->state() != tll::state::Active)
		return;

	c->post(&msg);
	tll::bench::timeit(count, proto, tll_channel_post, c.get(), &msg, 0);
}

int main()
{
	tll::Logger::set("tll", tll::Logger::Warning, true);

	tll::Config ctxcfg;
	auto ctx = tll::channel::Context(ctxcfg);
	ctx.reg(&Echo::impl);

	auto m = tll_channel_module();
	if (m->init)
		m->init(m, ctx, nullptr);
	if (m->impl) {
		for (auto i = m->impl; *i; i++)
			ctx.reg(*i);
	}

	tll::bench::prewarm(100ms);
	bench(ctx, "null");
	bench(ctx, "echo");
	bench(ctx, "bson+null", "libbson");
	bench(ctx, "bson+null", "cppbson");
	bench(ctx, "json+null");
	bench(ctx, "bson+echo", "libbson");
	bench(ctx, "bson+echo", "cppbson");
	bench(ctx, "json+echo");
}
