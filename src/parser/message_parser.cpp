#include <string>
#include <regex>
#include <cstring>

#include "message_parser.hpp"

//--================
// -- message_parser
//--================

/// @brief Parse a message (* = italic; ** = bold; __ = underline, ~~ = strikethrough)
/// @param input
/// @return char*
extern "C" char* parse_message_c(const char* input) {
	std::string text(input);

	std::regex strike("~~([^~]+?)~~");
	std::regex bold("\\*\\*([^*]+?)\\*\\*");
	std::regex underline("__([^_]+?)__");
	std::regex italic("\\*([^*]+?)\\*");

	text = std::regex_replace(text, strike, STRIKETHROUGH_SEQUENCE "$1" RESET_SEQUENCE);
	text = std::regex_replace(text, bold, BOLD_SEQUENCE "$1" RESET_SEQUENCE);
	text = std::regex_replace(text, underline, UNDERLINE_SEQUENCE "$1" RESET_SEQUENCE);
	text = std::regex_replace(text, italic, ITALIC_SEQUENCE "$1" RESET_SEQUENCE);

	char* result = (char*)malloc(text.length() + 1);
	if (result) {
		std::memcpy(result, text.c_str(), text.length() + 1);
	}

	return result;
}
