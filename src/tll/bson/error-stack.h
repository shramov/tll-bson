// SPDX-License-Identifier: MIT

#ifndef _TLL_BSON_ERROR_STACK_H
#define _TLL_BSON_ERROR_STACK_H

namespace tll::bson {

struct ErrorStack
{
	/// Error message
	std::string error;
	/// Error stack, field pointer or array index
	std::vector<std::variant<const tll::scheme::Field *, size_t>> error_stack;

	void error_clear()
	{
		error.clear();
		error_stack.clear();
	}

	template <typename R, typename Fmt, typename... Args>
	[[nodiscard]]
	R fail(R err, Fmt format, Args && ... args)
	{
		error = fmt::format(format, std::forward<Args>(args)...);
		error_stack.clear();
		return err;
	}

	template <typename R>
	[[nodiscard]]
	R fail_index(R err, size_t idx)
	{
		error_stack.push_back(idx);
		return err;
	}

	template <typename R>
	[[nodiscard]]
	R fail_field(R err, const tll::scheme::Field * field)
	{
		error_stack.push_back(field);
		return err;
	}

	std::string format_stack() const
	{
		using Field = tll::scheme::Field;
		std::string r;
		for (auto i = error_stack.rbegin(); i != error_stack.rend(); i++) {
			if (std::holds_alternative<size_t>(*i)) {
				r += fmt::format("[{}]", std::get<size_t>(*i));
			} else {
				if (r.size())
					r += ".";
				r += std::get<const Field *>(*i)->name;
			}
		}
		return r;
	}
};

} // namespace tll::bson

#endif//_TLL_BSON_ERROR_STACK_H
