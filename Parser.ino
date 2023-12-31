#include "Parser.hpp"

// Some value names to look for:
#define smlListResponse 0x701
static const byte smlListNameValues[6] SML_PROGMEM = { 0x01, 0x00, 0x62, 0x0a, 0xff, 0xff };
static const byte smlNameEnergyValue[6] SML_PROGMEM = { 0x01, 0x00, 0x01, 0x08, 0x00, 0xff };
static const byte smlNamePowerValue[6] SML_PROGMEM = { 0x01, 0x00, 0x10, 0x07, 0x00, 0xff };

void Parser::eventStart()
{
	inListResponse = false;
	inValueList = false;
	inEnergyEntry = false;
	inPowerEntry = false;
	gotEnergyEntry = false;
	gotPowerEntry = false;
	energy = 0;
	power = 0;
}

void Parser::eventEnd(bool valid)
{
	if (valid && gotPowerEntry) {
	}
}

void Parser::eventEnterArray(byte level, uint16_t length)
{
}

void Parser::eventLeaveArray(byte level)
{
	if (level < 2) {
		inListResponse = false;
	}

	if (level < 3) {
		inValueList = false;
	}

	if (level < 5) {
		inEnergyEntry = false;
		inPowerEntry = false;
	}
}

void Parser::eventValue(byte level, SMLParserType type, uint16_t pos, uint16_t length, SMLParserValue* value)
{
	switch (type) {
	case SMLParserType::integer:
		if (inEnergyEntry && level == 5 && pos == 6) {
			energy = value->integer;
			gotEnergyEntry = true;
		}

		if (inPowerEntry && level == 5 && pos == 6) {
			power = value->integer;
			gotPowerEntry = true;
		}
		break;

	case SMLParserType::uinteger:
		if (level == 2 && pos == 1 && value->uinteger == smlListResponse) {
			inListResponse = true;
		}
		break;

	case SMLParserType::string:
		if (inListResponse && level == 3 && pos == 3 && length == sizeof(smlListNameValues) &&
			!memcmp_P(value->string, smlListNameValues, sizeof(smlListNameValues)))
		{
			inValueList = true;
		}

		if (inValueList && level == 5 && pos == 1 && length == sizeof(smlNameEnergyValue) &&
			!memcmp_P(value->string, smlNameEnergyValue, sizeof(smlNameEnergyValue)))
		{
			inEnergyEntry = true;
		}

		if (inValueList && level == 5 && pos == 1 && length == sizeof(smlNamePowerValue) &&
			!memcmp_P(value->string, smlNamePowerValue, sizeof(smlNamePowerValue)))
		{
			inPowerEntry = true;
		}

		break;

	default:
		break;
	}
}

SMLParser<Parser> parser1;
SMLParser<Parser> parser2;
