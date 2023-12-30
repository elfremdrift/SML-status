#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>

#define DBG_LEVEL 2

#if DBG_LEVEL >= 3
#define DBG_TRACE(stuff) std::cout << std::dec << std::setw((level-stack)*2) << "" << stuff << "\n";
#else
#define DBG_TRACE(stuff)
#endif

#if DBG_LEVEL >= 2
#define DBG_INFO_BEGIN std::cout << std::dec << std::setw((level-stack)*2) << "";
#define DBG_INFO_STUFF(stuff) std::cout << stuff;
#define DBG_INFO_END std::cout << "\n";
#else
#define DBG_INFO_BEGIN()
#define DBG_INFO_STUFF(stuff)
#define DBG_INFO_END()
#endif
#define DBG_INFO(stuff) DBG_INFO_BEGIN DBG_INFO_STUFF(stuff) DBG_INFO_END

#if DBG_LEVEL >= 1
#define DBG_ERROR(stuff) std::cout << std::dec << std::setw((level-stack)*2) << "" << stuff << "\n";
#else
#define DBG_ERROR(stuff)
#endif

#include "parser.hpp"

void SMLParser::reset()
{
	DBG_ERROR("Calling reset at level " << getLevel())
	toOutside();
}

#if DBG_LEVEL > 0
int SMLParser::getLevel() const
{
	return level-stack;
}
#endif

void SMLParser::toOutside()
{
	state = State::outside;
	n1b = 0;
	n01 = 0;
	actSum = 0;
}

void SMLParser::outside(const byte ch)
{
	switch (ch) {
	case 0x1b:
		if (n1b < 4) {
			crc16(ch);
			++n1b;
		}
		break;

	case 0x01:
		if (n1b == 4) {
			crc16(ch);
			++n01;
			if (n01 == 4) {
				toInside();
			}
		}
		else {
			toOutside();
		}
		break;

	default:
		toOutside();
		break;
	}
}

void SMLParser::enter()
{
	++level;
	if (level-stack >= SML_STACK_SIZE) {
		std::cerr << "Stack level too high (" << level-stack << ")\n";
		return reset();
	}
	level->init();
}

void SMLParser::leave()
{
	if (level == stack) {
		level->init();
	}
	else if (level > stack) {
		--level;
		while (level->type == Type::array && level->current >= level->length) {
			if (level > stack) {
				--level;
			}
			else {
				level->init();
			}
		}
	}
	DBG_TRACE("Leave - now at level " << getLevel())
}

void SMLParser::toInside()
{
	level = stack;
	level->init();
	if (state != State::inside) {
		DBG_INFO("Message begins");
	}
	state = State::inside;
}

void SMLParser::inside(const byte ch)
{
	crc16(ch);
	DBG_TRACE("Byte at level " << getLevel() << ": " << std::hex << int{ch})
	bool repeat = true;
	while (repeat) {
		if (lengthFollows) {
			level->length = (level->length << 4) | (ch & 0x0f);
			lengthFollows = (ch & 0x80) != 0;
			++protoBytes;
			if (!lengthFollows && level->type != Type::array) level->length -= protoBytes;  // As type and length bytes are counted as well
			if (!lengthFollows) {
				DBG_TRACE("Entry type " << static_cast<int>(level->type) << " size " << level->length << " encountered at level " << getLevel())
			}
			return;
		}
		switch (level->type) {
		case Type::tbd:
			if (ch == 0x1b) {
				++n1b;
				if (n1b == 4) {
					return toSum();
				}
				return;
			}
			else {
				n1b = 0;
			}
			repeat = tbd(ch);
			break;

		case Type::array:
			repeat = array(ch);
			break;

		case Type::boolean:
			repeat = boolean(ch);
			break;

		case Type::integer:
			repeat = integer(ch, true);
			break;

		case Type::uinteger:
			repeat = integer(ch, false);
			break;
			
		case Type::string:
			repeat = string(ch);
			break;
		}
	}
}

void SMLParser::done()
{
	switch (level->type) {
	case Type::integer:
		DBG_INFO("Got signed integer length " << int{level->length} << ": " << value.integer <<
			"/0x" << std::hex << value.integer)
		break;

	case Type::uinteger:
		DBG_INFO("Got unsigned integer length " << int{level->length} << ": " << value.uinteger <<
			"/0x" << std::hex << value.uinteger)
		break;
		
	case Type::string:
		DBG_INFO_BEGIN
		DBG_INFO_STUFF("Got string length " << level->length << ": " << std::hex)
		for (byte i = 0; i < level->length && i < sizeof(value.string); ++i) {
			DBG_INFO_STUFF(int{value.string[i]} << " ")
		}
		DBG_INFO_END
		break;

	default:
		break;
	}
	leave();
}

bool SMLParser::tbd(const byte ch)
{
	// Look at ch to determine what this entry is
	if (ch == 0) {
		DBG_INFO("Eol encountered at level " << getLevel())
		done();
		return false;
	}

	auto entType = static_cast<Type>((ch & 0x70) >> 4);
	switch (entType) {
	case Type::string:
		memset(value.string, 0, sizeof(SML_STRING_MAX_LEN));
		break;
	case Type::boolean:
		value.boolean = false;
		break;
	case Type::integer:
		value.integer = 0;
		break;
	case Type::uinteger:
		value.uinteger = 0;
		break;
	default:
		break;
	}
	switch (entType) {
	case Type::string:
	case Type::boolean:
	case Type::integer:
	case Type::uinteger:
	case Type::array:
		level->type = entType;
		level->length = ch & 0x0f;
		lengthFollows = (ch & 0x80) != 0;
		protoBytes = 1;
		if (!lengthFollows && level->type != Type::array) level->length -= protoBytes;  // As type and length bytes are counted as well
		if (!lengthFollows) {
			DBG_TRACE("Entry type " << static_cast<int>(level->type) << " size " << level->length << " encountered at level " << getLevel())
		}
		if (level->length == 0) {
			// Already done
			done();
		}
		break;

	default:
		DBG_ERROR("Unknown entry type byte (" << std::hex << int{ch} << ")");
		reset();
	}
	return false;
}

bool SMLParser::array(const byte ch)
{
	if (level->current == 0) {
		DBG_INFO("Array size " << level->length << " encountered at level " << getLevel())
	}
	DBG_TRACE("Parse array row " << level->current << " at level " << getLevel())
	if (level->current < level->length) {
		++level->current;
		enter();  // Read next array entry
		return true;  // Re-use char on next level
	}
	return false;
}

void SMLParser::StackEntry::init()
{
	type = Type::tbd;
	length = 0;
	current = 0;
}

bool SMLParser::boolean(const byte ch)
{
	DBG_TRACE("Parse boolean byte " << level->current << " at level " << getLevel())
	if (level->current < level->length) {
		++level->current;
	}
	if (level->current >= level->length) {
		done();
	}
	return false;
}

bool SMLParser::string(const byte ch)
{
	DBG_TRACE("Parse string byte " << level->current << " at level " << getLevel())
	if (level->current < level->length) {
		if (level->current < sizeof(value.string)) value.string[level->current] = ch;
		++level->current;
	}
	if (level->current >= level->length) {
		done();
	}
	return false;
}

bool SMLParser::integer(const byte ch, const bool hasSign)
{
	DBG_TRACE("Parse integer byte " << level->current << " at level " << getLevel())

	byte* val = hasSign?reinterpret_cast<byte*>(&value.uinteger):reinterpret_cast<byte*>(&value.integer);

	if (level->current == 0) {
		*reinterpret_cast<uint64_t*>(val) = 0;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		next = val + sizeof(uint64_t) - 1;
#endif
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		next = val;
#endif
		if (level->length < sizeof(uint64_t)) {
			for (byte i = sizeof(uint64_t)-level->length; i>0; --i) {
				if (hasSign && (ch & 0x80)) {
					*next = 0xff;
				}
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
				--next;
#endif
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
				++next;
#endif
			}
		}
	}


	if (level->current < level->length) {
		if (level->current < sizeof(uint64_t)) {
			*next = ch;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			--next;
#endif
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			++next;
#endif
		}
		++level->current;
	}
	DBG_TRACE("Int so far: " << std::hex << value.uinteger)

	if (level->current >= level->length) {
		done();
	}
	return false;
}

void SMLParser::toSum()
{
	state = State::sum;
	expSum = 0;
	n01 = 0;
}

void SMLParser::sum(const byte ch)
{
	if (n01 == 0 && ch != 0x1a) {
		// Not a sum, assume outside and start parsing again
		toOutside();
		DBG_INFO("Message complete");
		parse(ch);
	}
	if (n01 < 2) crc16(ch);
	if (n01 >= 2) {
		expSum |= ch << (8 * (n01 & 1));
	}
	++n01;
	if (n01 == 4) {
		if (expSum != actSum) {
			DBG_ERROR("Wrong sum, expected sum: " << std::hex << expSum << " - actual sum: " << actSum)
		}
		else {
			DBG_INFO("Message complete");
		}
		toOutside();
	}
}

void SMLParser::crc16(const byte ch)
{
	static uint16_t crc16_table[] = {
			0xf78f, 0xe70e, 0xd68d, 0xc60c,
			0xb58b, 0xa50a, 0x9489, 0x8408,
			0x7387, 0x6306, 0x5285, 0x4204,
			0x3183, 0x2102, 0x1081, 0x0000
	};

	actSum = ( 0xf000 | (actSum >> 4) ) ^ crc16_table[(actSum & 0xf) ^ (ch & 0xf)];
	actSum = ( 0xf000 | (actSum >> 4) ) ^ crc16_table[(actSum & 0xf) ^ (ch >> 4)];
}

void SMLParser::parse(byte ch)
{
	DBG_TRACE("Parsing level " << getLevel() << ": ch = 0x" << std::hex << int{ch})
	switch (state) {
	case State::outside:	outside(ch);	break;
	case State::inside:		inside(ch);		break;
	case State::sum:		sum(ch);		break;
	}
}

int main()
{
	FILE* f = fopen("sml.example.bin", "rb");
	if (!f) {
		perror("open file");
		exit(1);
	}
	ssize_t bytes;
	char buffer[128];
	SMLParser parser;
	while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
		for (auto i = 0; i < bytes; ++i) {
			parser.parse(buffer[i]);
		}
	}
}
