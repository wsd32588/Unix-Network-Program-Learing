#pragma once
#ifndef _WIN32

#include <cerrno>
#include <expected>
#include <system_error>
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>

namespace unp::sys {
	[[nodiscard]] inline std::expected<void, std::error_code> exec_program(
		const char* file,
		char* const arguments[]
		) {
		::execvp(file, arguments);

		return std::unexpected(
			std::error_code(
				errno,std::generic_category())
		);
	}

	[[nodiscard]] inline std::expected<void, std::error_code> exec_program(
		std::string_view file,
		std::span<const char* const> args
	)  {
		std::vector<char*> argv;
		argv.reserve(args.size() + 2);

		std::string file_str(file);
		argv.push_back(file_str.data());

		for (const char* arg : args) {
			argv.push_back(const_cast<char*>(arg));
		}
		argv.push_back(nullptr);

		return exec_program(file_str.c_str(), argv.data());
	}
}

#endif