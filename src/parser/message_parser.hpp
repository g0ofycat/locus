#ifndef MESSAGE_PARSER_HPP
#define MESSAGE_PARSER_HPP

#define STRIKETHROUGH_SEQUENCE "\033[9m"
#define BOLD_SEQUENCE "\033[1m"
#define UNDERLINE_SEQUENCE "\033[4m"
#define ITALIC_SEQUENCE "\033[3m"

#define RESET_SEQUENCE "\033[m"

//--================
// -- message_parser
//--================

#ifdef __cplusplus
extern "C" {
#endif
	/// @brief Parse a message (* = italic; ** = bold; __ = underline, ~~ = strikethrough)
	/// @param input
	/// @return char*
	char* parse_message_c(const char* input);

#ifdef __cplusplus
}
#endif

#endif
