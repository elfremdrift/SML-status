#include "parser.hpp"

#ifdef SML_TEST

#include <iostream>
#include <iomanip>

class MyHandlers {
public:
	void eventStart()
	{
		std::cout << "* Message start event\n";
	}

	void eventEnd(bool valid)
	{
		std::cout << "* Message end event (" << (valid ? "" : "in") << "valid)\n";
	}

	void eventLevel(byte level)
	{
		std::cout << std::setw(2+level*2) << std::left << "*" << "Event level now " << int{level} << "\n";
	}

	void eventValue(byte level, SMLParserType type, uint16_t length, SMLParserValue* value)
	{
		std::cout << std::setw(2+level*2) << std::left << "*" << "Got value type " << int{static_cast<byte>(type)} << " at level " << int{level} << ": ";
		switch (type) {
		case SMLParserType::integer:
			std::cout << value->integer << " (0x" << std::hex << value->integer << std::dec << ")\n";
			break;
		case SMLParserType::uinteger:
			std::cout << value->uinteger << " (0x" << std::hex << value->uinteger << std::dec << ")\n";
			break;
		case SMLParserType::string:
			std::cout << std::hex;
			for (uint16_t i = 0; i<length; ++i) {
				std::cout << int{value->string[i]} << " ";
			}
			std::cout << std::dec << "\n";
			break;
		case SMLParserType::eol:
			std::cout << "EOL\n";
			break;
		default:
			std::cout << "(unknown)\n";
		}
	}
};

int main()
{
	FILE* f = fopen("sml.example.bin", "rb");
	if (!f) {
		perror("open file");
		exit(1);
	}
	ssize_t bytes;
	char buffer[128];
	SMLParser<MyHandlers> parser;
	while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
		for (auto i = 0; i < bytes; ++i) {
			parser.parse(buffer[i]);
		}
	}
}

#endif
