#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>
#include "parser.hpp"

#define DBG(level) std::cout << std::setw(level*2) << ""

void SMLParser::reset()
{
	std::cerr << "Calling reset at level " << int{level} << "\n";
	isInside = false;
	level = 0;
	n1b = 0;
	n01 = 0;
}

void SMLParser::outside(const byte ch)
{
	switch (ch) {
	case 0x1b:
		if (n1b < 4) ++n1b;
		break;

	case 0x01:
		if (n1b == 4) {
			++n01;
			if (n01 == 4) isInside = true;
		}
		else {
			n1b = 0;
		}
		break;

	default:
		n1b = 0;
		n01 = 0;
		break;
	}
}

void SMLParser::enter()
{
	++level;
	if (level >= stack.size()) {
		std::cerr << "Stack level too high (" << int{level} << ")\n";
		return reset();
	}
	stack[level].init();
}

void SMLParser::leave()
{
	if (level == 0) {
		stack[level].init();
	}
	else if (level > 0) {
		--level;
		while (stack[level].type == Type::array && stack[level].current >= stack[level].length) {
			if (level > 0) {
				--level;
			}
			else {
				stack[level].init();
			}
		}
	}
	DBG(level) << "Leave - now at level " << int{level} << "\n";
}

void SMLParser::inside(const byte ch)
{
	// DBG(level) << "Byte at level " << int{level} << ": " << std::hex << int{ch} << "\n";
	auto& entry = stack[level];
	if (entry.lengthFollows) {
		entry.length = (entry.length << 4) | (ch & 0x0f);
		entry.lengthFollows = (ch & 0x80) != 0;
		++entry.protoBytes;
		if (!entry.lengthFollows && entry.type != Type::array) entry.length -= entry.protoBytes;  // As type and length bytes are counted as well
		if (entry.lengthFollows) return;
	}
	switch (entry.type) {
	case Type::tbd:
		entry.tbd(*this, ch);
		break;

	case Type::array:
		entry.array(*this, ch);
		break;

	case Type::boolean:
		entry.boolean(*this, ch);
		break;

	case Type::integer:
		entry.integer(*this, ch, true);
		break;

	case Type::uinteger:
		entry.integer(*this, ch, false);
		break;
		
	case Type::string:
		entry.string(*this, ch);
		break;
	}
}

void SMLParser::StackEntry::done(SMLParser& parser)
{
	switch (type) {
	case Type::integer:
		DBG(parser.level) << "Got signed integer length " << int{length} << ": " << value.integer << "\n";
		break;

	case Type::uinteger:
		DBG(parser.level) << "Got unsigned integer length " << int{length} << ": " << value.uinteger << "\n";
		break;
		
	case Type::string:
		DBG(parser.level) << "Got string length " << int{length} << ": ";
		for (byte i = 0; i < length && i < sizeof(value.string); ++i) {
			std::cout << std::hex << int{value.string[i]} << " ";
		}
		std::cout << "\n";
		break;

	default:
		break;
	}
	parser.leave();
}

void SMLParser::StackEntry::tbd(SMLParser& parser, const byte ch)
{
	// Look at ch to determine what this entry is
	if (ch == 0) {
		DBG(parser.level) << "Eol encountered at level " << int{parser.level} << "\n";
		return done(parser);
	}

	auto entType = static_cast<Type>((ch & 0x70) >> 4);
	switch (entType) {
	case Type::string:
	case Type::boolean:
	case Type::integer:
	case Type::uinteger:
	case Type::array:
		type = entType;
		length = ch & 0x0f;
		lengthFollows = (ch & 0x80) != 0;
		protoBytes = 1;
		if (!lengthFollows && type != Type::array) length -= protoBytes;  // As type and length bytes are counted as well
		if (length == 0) {
			// Already done
			done(parser);
		}
		DBG(parser.level) << "Entry type " << static_cast<int>(type) << " encountered at level " << int{parser.level} << "\n";
		break;

	default:
		std::cerr << "Unknown entry type byte (" << std::hex << int{ch} << ")\n";
		return parser.reset();
	}
}

void SMLParser::StackEntry::array(SMLParser& parser, const byte ch)
{
	DBG(parser.level) << "Parse array row " << int{current} << " at level " << int{parser.level} << "\n";
	if (current < length) {
		++current;
		parser.enter();  // Read next array entry
		parser.parse(ch);  // Recurse to parse ch at new level
	}
}

void SMLParser::StackEntry::init()
{
	*this = StackEntry{};
}

void SMLParser::StackEntry::boolean(SMLParser& parser, const byte ch)
{
	DBG(parser.level) << "Parse boolean byte " << int{current} << " at level " << int{parser.level} << "\n";
	if (current < length) {
		++current;
	}
	if (current >= length) {
		done(parser);
	}
}

void SMLParser::StackEntry::string(SMLParser& parser, const byte ch)
{
	// DBG(parser.level) << "Parse string byte " << int{current} << " at level " << int{parser.level} << "\n";
	if (current < length) {
		if (current < sizeof(value.string)) value.string[current] = ch;
		++current;
	}
	if (current >= length) {
		done(parser);
	}
}

void SMLParser::StackEntry::integer(SMLParser& parser, const byte ch, const bool hasSign)
{
	// DBG(parser.level) << "Parse integer byte " << int{current} << " at level " << int{parser.level} << "\n";

	if (current == 0) {
		value.uinteger = 0;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		next = &value.string[sizeof(uint64_t)-1];
#endif
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		next = &value.string;
#endif
		if (sizeof(uint64_t) > length) {
			for (byte i = sizeof(uint64_t)-length; i>0; --i) {
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


	if (current < length) {
		if (current < sizeof(uint64_t)) {
			*next = ch;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			--next;
#endif
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			++next;
#endif
		}
		++current;
	}
	// DBG(parser.level) << std::hex << "So far: " << value.uinteger << "\n";

	if (current >= length) {
		done(parser);
	}
}

void SMLParser::parse(byte ch)
{
//	DBG(parser.level) << "Parsing level " << int{level} << ": " << std::hex << int{ch} << "\n";
	if (!isInside) {
		outside(ch);
	}
	else {
		inside(ch);
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
