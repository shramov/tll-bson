#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

static constexpr std::string_view scheme_string = R"(yamls+gz://eJztVT1vwjAQ3fsrvFmqQMIORYitrZA6dWn3yjQHWM2XYqcCofz3nuskBOIQoYDEwOQ4uXvvxed7NySRCGFG6BsIH1L6QAhEWahm+EAIneMzozOy09sEo2Skp4P/AHxFn/HDaEDoC64M11dceZ5j5lJC4BcYQ7IrKKQ/ogNikShCeZzmjRDWFaLVIcpkjNs40TKOlFGVgoqDzGxRDs0UfqUm2uy0DOEriTGL5k1q3q3OO6ZuqmNXUQe1n7ZFOYAtIUxpHMmsR7LSqYxWNfrFVgObuCFscBsIOwTxeBfIsLyd841OhaKtV+sHtl3F+xVBBs3yVRSfqZCB7QA3RwhKiVUNotR5fNpWaxX2WIivcX3IMAnAUEkfe2fUyrm2XVlhFV16SzVygfB+SpanO7xMX8oN+FNH+ukW7Ezn/dIbHnFWuj0T7xJVGPergi4aogIpO6R+kbNFe8Mc+73DLm/n4vrwLUMR7EGKF4xPO6eUcw6cM6VOjIHqqN9BafBLz+B3z7h7xu15hkI/2A8+4w5umqdLaJ1czd/+AIQ3WKo=)";

struct Header
{
	static constexpr size_t meta_size() { return 86; }
	static constexpr std::string_view meta_name() { return "Header"; }

	enum class Enum1: int8_t
	{
		A = 0,
		B = 1,
		C = 2,
	};

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Header::meta_size(); }
		static constexpr auto meta_name() { return Header::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_id0 = int32_t;
		type_id0 get_id0() const { return this->template _get_scalar<type_id0>(0); }
		void set_id0(type_id0 v) { return this->template _set_scalar<type_id0>(0, v); }

		using type_id1 = int32_t;
		type_id1 get_id1() const { return this->template _get_scalar<type_id1>(4); }
		void set_id1(type_id1 v) { return this->template _set_scalar<type_id1>(4, v); }

		using type_ts0 = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<int64_t, std::micro>>;
		type_ts0 get_ts0() const { return this->template _get_scalar<type_ts0>(8); }
		void set_ts0(type_ts0 v) { return this->template _set_scalar<type_ts0>(8, v); }

		using type_id2 = int32_t;
		type_id2 get_id2() const { return this->template _get_scalar<type_id2>(16); }
		void set_id2(type_id2 v) { return this->template _set_scalar<type_id2>(16, v); }

		using type_id3 = int64_t;
		type_id3 get_id3() const { return this->template _get_scalar<type_id3>(20); }
		void set_id3(type_id3 v) { return this->template _set_scalar<type_id3>(20, v); }

		using type_ts1 = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<int64_t, std::micro>>;
		type_ts1 get_ts1() const { return this->template _get_scalar<type_ts1>(28); }
		void set_ts1(type_ts1 v) { return this->template _set_scalar<type_ts1>(28, v); }

		using type_e0 = Enum1;
		type_e0 get_e0() const { return this->template _get_scalar<type_e0>(36); }
		void set_e0(type_e0 v) { return this->template _set_scalar<type_e0>(36, v); }

		using type_e1 = Enum1;
		type_e1 get_e1() const { return this->template _get_scalar<type_e1>(37); }
		void set_e1(type_e1 v) { return this->template _set_scalar<type_e1>(37, v); }

		std::string_view get_string0() const { return this->template _get_bytestring<16>(38); }
		void set_string0(std::string_view v) { return this->template _set_bytestring<16>(38, v); }

		std::string_view get_string1() const { return this->template _get_bytestring<32>(54); }
		void set_string1(std::string_view v) { return this->template _set_bytestring<32>(54, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Extras
{
	static constexpr size_t meta_size() { return 12; }
	static constexpr std::string_view meta_name() { return "Extras"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Extras::meta_size(); }
		static constexpr auto meta_name() { return Extras::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_key = int32_t;
		type_key get_key() const { return this->template _get_scalar<type_key>(0); }
		void set_key(type_key v) { return this->template _set_scalar<type_key>(0, v); }

		using type_value = int64_t;
		type_value get_value() const { return this->template _get_scalar<type_value>(4); }
		void set_value(type_value v) { return this->template _set_scalar<type_value>(4, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Trailer
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Trailer"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Trailer::meta_size(); }
		static constexpr auto meta_name() { return Trailer::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_message() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_message(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		using type_extras = tll::scheme::binder::List<Buf, Extras::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_extras get_extras() const { return this->template _get_binder<type_extras>(8); }
		type_extras get_extras() { return this->template _get_binder<type_extras>(8); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Simple
{
	static constexpr size_t meta_size() { return 246; }
	static constexpr std::string_view meta_name() { return "Simple"; }
	static constexpr int meta_id() { return 10; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Simple::meta_size(); }
		static constexpr auto meta_name() { return Simple::meta_name(); }
		static constexpr auto meta_id() { return Simple::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_header = Header::binder_type<Buf>;
		const type_header get_header() const { return this->template _get_binder<type_header>(0); }
		type_header get_header() { return this->template _get_binder<type_header>(0); }

		std::string_view get_string0() const { return this->template _get_bytestring<16>(86); }
		void set_string0(std::string_view v) { return this->template _set_bytestring<16>(86, v); }

		std::string_view get_string1() const { return this->template _get_bytestring<32>(102); }
		void set_string1(std::string_view v) { return this->template _set_bytestring<32>(102, v); }

		std::string_view get_string2() const { return this->template _get_bytestring<16>(134); }
		void set_string2(std::string_view v) { return this->template _set_bytestring<16>(134, v); }

		using type_f0 = tll::scheme::FixedPoint<int64_t, 8>;
		type_f0 get_f0() const { return this->template _get_scalar<type_f0>(150); }
		void set_f0(type_f0 v) { return this->template _set_scalar<type_f0>(150, v); }

		using type_f1 = tll::scheme::FixedPoint<int64_t, 8>;
		type_f1 get_f1() const { return this->template _get_scalar<type_f1>(158); }
		void set_f1(type_f1 v) { return this->template _set_scalar<type_f1>(158, v); }

		using type_f2 = tll::scheme::FixedPoint<int64_t, 8>;
		type_f2 get_f2() const { return this->template _get_scalar<type_f2>(166); }
		void set_f2(type_f2 v) { return this->template _set_scalar<type_f2>(166, v); }

		using type_f3 = tll::scheme::FixedPoint<int64_t, 8>;
		type_f3 get_f3() const { return this->template _get_scalar<type_f3>(174); }
		void set_f3(type_f3 v) { return this->template _set_scalar<type_f3>(174, v); }

		std::string_view get_string3() const { return this->template _get_bytestring<32>(182); }
		void set_string3(std::string_view v) { return this->template _set_bytestring<32>(182, v); }

		std::string_view get_string4() const { return this->template _get_bytestring<16>(214); }
		void set_string4(std::string_view v) { return this->template _set_bytestring<16>(214, v); }

		using type_trailer = Trailer::binder_type<Buf>;
		const type_trailer get_trailer() const { return this->template _get_binder<type_trailer>(230); }
		type_trailer get_trailer() { return this->template _get_binder<type_trailer>(230); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Sub
{
	static constexpr size_t meta_size() { return 88; }
	static constexpr std::string_view meta_name() { return "Sub"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Sub::meta_size(); }
		static constexpr auto meta_name() { return Sub::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_id0 = int64_t;
		type_id0 get_id0() const { return this->template _get_scalar<type_id0>(0); }
		void set_id0(type_id0 v) { return this->template _set_scalar<type_id0>(0, v); }

		std::string_view get_string0() const { return this->template _get_bytestring<16>(8); }
		void set_string0(std::string_view v) { return this->template _set_bytestring<16>(8, v); }

		std::string_view get_string1() const { return this->template _get_bytestring<32>(24); }
		void set_string1(std::string_view v) { return this->template _set_bytestring<32>(24, v); }

		using type_decimal = tll::scheme::Decimal128;
		type_decimal get_decimal() const { return this->template _get_scalar<type_decimal>(56); }
		void set_decimal(type_decimal v) { return this->template _set_scalar<type_decimal>(56, v); }

		using type_id1 = int64_t;
		type_id1 get_id1() const { return this->template _get_scalar<type_id1>(72); }
		void set_id1(type_id1 v) { return this->template _set_scalar<type_id1>(72, v); }

		using type_ts0 = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<int64_t, std::ratio<1>>>;
		type_ts0 get_ts0() const { return this->template _get_scalar<type_ts0>(80); }
		void set_ts0(type_ts0 v) { return this->template _set_scalar<type_ts0>(80, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Nested
{
	static constexpr size_t meta_size() { return 302; }
	static constexpr std::string_view meta_name() { return "Nested"; }
	static constexpr int meta_id() { return 20; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Nested::meta_size(); }
		static constexpr auto meta_name() { return Nested::meta_name(); }
		static constexpr auto meta_id() { return Nested::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_header = Header::binder_type<Buf>;
		const type_header get_header() const { return this->template _get_binder<type_header>(0); }
		type_header get_header() { return this->template _get_binder<type_header>(0); }

		std::string_view get_string0() const { return this->template _get_bytestring<16>(86); }
		void set_string0(std::string_view v) { return this->template _set_bytestring<16>(86, v); }

		std::string_view get_string1() const { return this->template _get_bytestring<32>(102); }
		void set_string1(std::string_view v) { return this->template _set_bytestring<32>(102, v); }

		std::string_view get_string2() const { return this->template _get_bytestring<16>(134); }
		void set_string2(std::string_view v) { return this->template _set_bytestring<16>(134, v); }

		using type_f0 = tll::scheme::FixedPoint<int64_t, 8>;
		type_f0 get_f0() const { return this->template _get_scalar<type_f0>(150); }
		void set_f0(type_f0 v) { return this->template _set_scalar<type_f0>(150, v); }

		using type_f1 = tll::scheme::FixedPoint<int64_t, 8>;
		type_f1 get_f1() const { return this->template _get_scalar<type_f1>(158); }
		void set_f1(type_f1 v) { return this->template _set_scalar<type_f1>(158, v); }

		using type_f2 = tll::scheme::FixedPoint<int64_t, 8>;
		type_f2 get_f2() const { return this->template _get_scalar<type_f2>(166); }
		void set_f2(type_f2 v) { return this->template _set_scalar<type_f2>(166, v); }

		using type_f3 = tll::scheme::FixedPoint<int64_t, 8>;
		type_f3 get_f3() const { return this->template _get_scalar<type_f3>(174); }
		void set_f3(type_f3 v) { return this->template _set_scalar<type_f3>(174, v); }

		std::string_view get_string3() const { return this->template _get_bytestring<32>(182); }
		void set_string3(std::string_view v) { return this->template _set_bytestring<32>(182, v); }

		std::string_view get_string4() const { return this->template _get_bytestring<16>(214); }
		void set_string4(std::string_view v) { return this->template _set_bytestring<16>(214, v); }

		using type_sub = tll::scheme::binder::List<Buf, Sub::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_sub get_sub() const { return this->template _get_binder<type_sub>(230); }
		type_sub get_sub() { return this->template _get_binder<type_sub>(230); }

		std::string_view get_string5() const { return this->template _get_bytestring<32>(238); }
		void set_string5(std::string_view v) { return this->template _set_bytestring<32>(238, v); }

		std::string_view get_string6() const { return this->template _get_bytestring<16>(270); }
		void set_string6(std::string_view v) { return this->template _set_bytestring<16>(270, v); }

		using type_trailer = Trailer::binder_type<Buf>;
		const type_trailer get_trailer() const { return this->template _get_binder<type_trailer>(286); }
		type_trailer get_trailer() { return this->template _get_binder<type_trailer>(286); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

template <>
struct tll::conv::dump<Header::Enum1> : public to_string_from_string_buf<Header::Enum1>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const Header::Enum1 &v, Buf &buf)
	{
		switch (v) {
		case Header::Enum1::A: return "A";
		case Header::Enum1::B: return "B";
		case Header::Enum1::C: return "C";
		default: break;
		}
		return tll::conv::to_string_buf<int8_t, Buf>((int8_t) v, buf);
	}
};
