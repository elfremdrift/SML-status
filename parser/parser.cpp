#include "../SMLParser.hpp"

#include <iostream>
#include <iomanip>

#define smlListResponse 0x701
static byte smlListNameValues[6] SML_PROGMEM = { 0x01, 0x00, 0x62, 0x0a, 0xff, 0xff };
static byte smlNameEnergyValue[6] SML_PROGMEM = { 0x01, 0x00, 0x01, 0x08, 0x00, 0xff };
static byte smlNamePowerValue[6] SML_PROGMEM = { 0x01, 0x00, 0x10, 0x07, 0x00, 0xff };

class MyHandlers {
private:
	bool inListResponse = false;
	bool inValueList = false;
	bool inEnergyEntry = false;
	bool inPowerEntry = false;
	uint64_t	energy;
	uint64_t	consumption;

public:
	void eventStart()
	{
		std::cout << "* Message start event\n";
	}

	void eventEnd(bool valid)
	{
		std::cout << "* Message end event (" << (valid ? "" : "in") << "valid)\n";
	}

	void eventEnterArray(byte level, uint16_t length)
	{
		std::cout << std::setw(2+level*2) << std::left << "*" << "Entering array size " << length << ", level " << int{level} << "\n";
	}

	void eventLeaveArray(byte level )
	{
		if (level < 2) {
			if (inListResponse) {
				std::cout << "Exit list response...\n";
			}
			inListResponse = false;
		}

		if (level < 3) {
			if (inValueList) {
				std::cout << "Exit value list...\n";
			}
			inValueList = false;
		}

		if (level < 5) {
			if (inEnergyEntry) {
				std::cout << "Exit energy entries...\n";
			}
			if (inPowerEntry) {
				std::cout << "Exit power entries...\n";
			}
			inEnergyEntry = false;
			inPowerEntry = false;
		}

		std::cout << std::setw(2+level*2) << std::left << "*" << "Leaving array, level " << int{level} << "\n";
	}

	void eventValue(byte level, SMLParserType type, uint16_t pos, uint16_t length, SMLParserValue* value)
	{
		std::cout << std::setw(2+level*2) << std::left << "*" << "Got value type " << int{static_cast<byte>(type)} << " at level " << int{level} << ": ";
		switch (type) {
		case SMLParserType::integer:
			std::cout << value->integer << " (0x" << std::hex << value->integer << std::dec << ")\n";

			if (inEnergyEntry && level == 5 && pos == 6) {
				std::cout << "Got energy: " << (value->integer/10) << "." << (value->integer%10) << "Wh\n";
			}

			if (inPowerEntry && level == 5 && pos == 6) {
				std::cout << "Got power: " << (value->integer/10) << "." << (value->integer%10) << "W\n";
			}

			break;

		case SMLParserType::uinteger:
			std::cout << value->uinteger << " (0x" << std::hex << value->uinteger << std::dec << ")\n";

			if (level == 2 && pos == 1 && value->uinteger == smlListResponse) {
				std::cout << "Enter list response...\n";
				inListResponse = true;
			}

			break;
		case SMLParserType::string:
			std::cout << std::hex;
			for (uint16_t i = 0; i<length; ++i) {
				std::cout << int{value->string[i]} << " ";
			}
			std::cout << std::dec << "\n";

			if (inListResponse && level == 3 && pos == 3 && length == sizeof(smlListNameValues) &&
				!memcmp(value->string, smlListNameValues, sizeof(smlListNameValues)))
			{
				std::cout << "Enter value list...\n";
				inValueList = true;
			}

			if (inValueList && level == 5 && pos == 1 && length == sizeof(smlNameEnergyValue) &&
				!memcmp(value->string, smlNameEnergyValue, sizeof(smlNameEnergyValue)))
			{
				std::cout << "Enter energy entries...\n";
				inEnergyEntry = true;
			}

			if (inValueList && level == 5 && pos == 1 && length == sizeof(smlNamePowerValue) &&
				!memcmp(value->string, smlNamePowerValue, sizeof(smlNamePowerValue)))
			{
				std::cout << "Enter power entries...\n";
				inPowerEntry = true;
			}

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
