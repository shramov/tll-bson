#include "bench-scheme.h"

#include <tll/channel.h>
#include <tll/channel/base.h>
#include <tll/channel/module.h>

#include <tll/util/bench.h>
#include <tll/util/time.h>

extern "C" tll_channel_module_t * channel_module();

using namespace std::chrono;

class Echo : public tll::channel::Base<Echo>
{
 public:
	static constexpr std::string_view channel_protocol() { return "echo"; }
	static constexpr auto process_policy() { return tll::channel::Base<Echo>::ProcessPolicy::Never; }

	int _post(const tll_msg_t *msg, int flags) { return _callback_data(msg); }
};

TLL_DEFINE_IMPL(Echo);

int main()
{
	tll::Logger::set("tll", tll::Logger::Warning, true);

	tll::Config ctxcfg;
	auto ctx = tll::channel::Context(ctxcfg);
	ctx.reg(&Echo::impl);

	auto m = channel_module();
	if (m->init)
		m->init(m, ctx);
	if (m->impl) {
		for (auto i = m->impl; *i; i++)
			ctx.reg(*i);
	}

	auto bson = ctx.channel("bson+echo://;scheme=yaml://src/bench.yaml;name=bson");
	if (!bson)
		return 1;
	bson->open();

	auto json = ctx.channel("json+echo://;scheme=yaml://src/bench.yaml;name=json");
	if (!json)
		return 1;
	json->open();

	std::vector<char> buf_simple, buf_nested;

	auto simple = Simple::bind(buf_simple);
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

	tll_msg_t smsg = {};
	smsg.data = simple.view().data();
	smsg.size = simple.view().size();
	smsg.msgid = simple.meta_id();

	bson->post(&smsg);
	json->post(&smsg);

	static constexpr auto count = 100000u;

	tll::bench::prewarm(100ms);
	tll::bench::timeit(count, "bson simple", tll_channel_post, bson.get(), &smsg, 0);
	tll::bench::timeit(count, "json simple", tll_channel_post, json.get(), &smsg, 0);
}
