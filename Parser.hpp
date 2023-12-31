#pragma once

#include "SMLParser.hpp"

class Parser {
private:
	bool inListResponse;
	bool inValueList;
	bool inEnergyEntry;
	bool inPowerEntry;
	bool gotEnergyEntry;
	bool gotPowerEntry;
	int64_t	energy;
	int64_t	power;

public:
	void eventStart();
	void eventEnd(bool valid);
	void eventEnterArray(byte level, uint16_t length);
	void eventLeaveArray(byte level);
	void eventValue(byte level, SMLParserType type, uint16_t pos, uint16_t length, SMLParserValue* value);
};

extern SMLParser<Parser> parser1;
extern SMLParser<Parser> parser2;
