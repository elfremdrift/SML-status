#pragma once
#include <array>
#include <stdint.h>
#include <cstring>

#ifndef SML_DBG_LEVEL
#define SML_DBG_LEVEL 0
#endif

#if SML_DBG_LEVEL > 0
#include <iostream>
#include <iomanip>
#endif

#if SML_DBG_LEVEL >= 3
#define SML_DBG_TRACE(stuff) std::cout << std::dec << std::setw((level-stack)*2) << "" << stuff << "\n";
#else
#define SML_DBG_TRACE(stuff)
#endif

#if SML_DBG_LEVEL >= 2
#define SML_DBG_INFO_BEGIN std::cout << std::dec << std::setw((level-stack)*2) << "";
#define SML_DBG_INFO_STUFF(stuff) std::cout << stuff;
#define SML_DBG_INFO_END std::cout << "\n";
#define SML_DBG_INFO(stuff) SML_DBG_INFO_BEGIN SML_DBG_INFO_STUFF(stuff) SML_DBG_INFO_END
#else
#define SML_DBG_INFO_BEGIN()
#define SML_DBG_INFO_STUFF(stuff)
#define SML_DBG_INFO_END()
#define SML_DBG_INFO(stuff)
#endif

#if SML_DBG_LEVEL >= 1
#define SML_DBG_ERROR(stuff) std::cerr << std::dec << std::setw((level-stack)*2) << "" << stuff << "\n";
#else
#define SML_DBG_ERROR(stuff)
#endif

#ifndef byte
#define byte uint8_t
#endif

#define SML_STACK_SIZE 10
#define SML_STRING_MAX_LEN 64

#ifdef PROGMEM
#define SML_PROGMEM PROGMEM
#define SML_PROGMEM_READ_BYTE(ptr) pgm_read_byte(ptr);
#define SML_PROGMEM_READ_WORD(ptr) pgm_read_word(ptr);
#else
#define SML_PROGMEM
#define SML_PROGMEM_READ_BYTE(ptr) (*(ptr))
#define SML_PROGMEM_READ_WORD(ptr) (*(ptr))
#endif

enum class SMLParserType : byte {
	  tbd = 255
	, string = 0
	, boolean = 4
	, integer = 5
	, uinteger = 6
	, array = 7
	, eol = 254
};

union SMLParserValue {
	uint64_t	uinteger;
	int64_t		integer;
	bool		boolean;
	byte		string[SML_STRING_MAX_LEN];
};

template<class SMLEventHandler>
class SMLParser : public SMLEventHandler
{
public:
	using Type = SMLParserType;
	using Value = SMLParserValue;

	// Default constructor - pass everything to event handler class if needed
	template<typename... ARGS>
	SMLParser(ARGS&&... args) : SMLEventHandler(std::forward<ARGS>(args)...)
	{
	}

	void reset()
	{
		SML_DBG_ERROR("Calling reset at level " << getLevel())
		toOutside();
		SMLEventHandler::eventEnd(false);
	}

	void parse(byte ch)  //!< Parse next byte
	{
		// SML_DBG_TRACE("Parsing level " << getLevel() << ": ch = 0x" << std::hex << int{ch})
		switch (state) {
		case State::outside:	outside(ch);	break;
		case State::inside:		inside(ch);		break;
		case State::sum:		sum(ch);		break;
		}
	}

private:
	enum class State : byte {
	      outside = 0
		, inside = 1
		, sum = 2
	} state = State::outside;
	struct StackEntry {
		Type type;
		uint16_t length;
		uint16_t current;
		void init()
		{
			type = Type::tbd;
			length = 0;
			current = 0;
		}
	};
	Value value;
	union {
		byte* next;
		byte protoBytes;
	};
	bool lengthFollows;
	StackEntry stack[SML_STACK_SIZE];
	StackEntry* level;
	byte n1b;
	byte n01;

	uint16_t actSum, expSum;

	void toOutside()
	{
		state = State::outside;
		n1b = 0;
		n01 = 0;
		actSum = 0;
	}

	void outside(const byte ch) //!< Look for start sequence
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

	void toInside()  //!< Change to and initialize inside state
	{
		level = stack;
		level->init();
		if (state != State::inside) {
			SMLEventHandler::eventStart();
			SML_DBG_INFO("Message begins");
		}
		state = State::inside;
		lengthFollows = false;
	}

	void inside(const byte ch) //!< Parse SML structure
	{
		crc16(ch);
		SML_DBG_TRACE("Byte at level " << getLevel() << ": " << std::hex << int{ch})
		bool repeat = true;
		while (repeat) {
			if (lengthFollows) {
				level->length = (level->length << 4) | (ch & 0x0f);
				lengthFollows = (ch & 0x80) != 0;
				++protoBytes;
				if (!lengthFollows && level->type != Type::array) level->length -= protoBytes;  // As type and length bytes are counted as well
				if (!lengthFollows) {
					SML_DBG_TRACE("Entry type " << static_cast<int>(level->type) << " size " << level->length << " encountered at level " << getLevel())
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

			case Type::eol:
				// Won't happen on input
				repeat = false;
				break;
			}
		}
	}

	bool tbd(const byte ch)  //!< Parse type byte to determine entry type
	{
		// Look at ch to determine what this entry is
		if (ch == 0) {
			SML_DBG_INFO("Eol encountered at level " << getLevel())
			SMLEventHandler::eventValue(level-stack, Type::eol, (level > stack) ? (level-1)->current : 0, 0, nullptr);
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
			if (level->length != 1) {
				SML_DBG_ERROR("Illegal boolean length (not 1) encountered at level " << getLevel())
				reset();
				return false;
			}
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
				SML_DBG_TRACE("Entry type " << static_cast<int>(level->type) << " size " << level->length << " encountered at level " << getLevel())
			}
			if (level->length == 0) {
				// Already done
				done();
			}
			break;

		default:
			SML_DBG_ERROR("Unknown entry type byte (" << std::hex << int{ch} << ")");
			reset();
		}
		return false;
	}

	bool array(const byte ch)  //!< Parse array
	{
		if (level->current == 0) {
			SMLEventHandler::eventEnterArray(level-stack, level->length);
			SML_DBG_INFO("Array size " << level->length << " encountered at level " << getLevel())
		}
		SML_DBG_TRACE("Parse array row " << level->current << " at level " << getLevel())
		if (level->current < level->length) {
			++level->current;
			enter();  // Read next array entry
			return true;  // Re-use char on next level
		}
		return false;
	}

	bool boolean(const byte ch)  //!< Parse boolean
	{
		SML_DBG_TRACE("Parse boolean byte " << level->current << " at level " << getLevel())
		value.boolean = ch != 0;
		done();
		return false;
	}

	bool integer(const byte ch, const bool hasSign)  //!< Parse integer
	{
		SML_DBG_TRACE("Parse integer byte " << level->current << " at level " << getLevel())

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
		SML_DBG_TRACE("Int so far: " << std::hex << value.uinteger)

		if (level->current >= level->length) {
			done();
		}
		return false;
	}

	bool string(const byte ch)  //!< Parse string
	{
		SML_DBG_TRACE("Parse string byte " << level->current << " at level " << getLevel())
		if (level->current < level->length) {
			if (level->current < sizeof(value.string)) value.string[level->current] = ch;
			++level->current;
		}
		if (level->current >= level->length) {
			done();
		}
		return false;
	}


	void done()  //!< Level done
	{
		switch (level->type) {
		case Type::integer:
		case Type::uinteger:
		case Type::boolean:
		case Type::string:
			SMLEventHandler::eventValue(level-stack, level->type, (level > stack) ? (level-1)->current : 0, level->length, &value);
			break;
		default:
			break;
		}
#if SML_DBG_LEVEL >= 2
		switch (level->type) {
		case Type::integer:
			SML_DBG_INFO("Got signed integer length " << int{level->length} << ": " << value.integer <<
				"/0x" << std::hex << value.integer)
			break;

		case Type::uinteger:
			SML_DBG_INFO("Got unsigned integer length " << int{level->length} << ": " << value.uinteger <<
				"/0x" << std::hex << value.uinteger)
			break;
			
		case Type::string:
			SML_DBG_INFO_BEGIN
			SML_DBG_INFO_STUFF("Got string length " << level->length << ": " << std::hex)
			for (byte i = 0; i < level->length && i < sizeof(value.string); ++i) {
				SML_DBG_INFO_STUFF(int{value.string[i]} << " ")
			}
			SML_DBG_INFO_END
			break;

		default:
			break;
		}
#endif
		leave();
	}

	void enter()  //!< Enter next stack level
	{
		++level;
		if (level-stack >= SML_STACK_SIZE) {
			SML_DBG_ERROR("Stack level too high (" << level-stack << ")")
			return reset();
		}
		level->init();
	}

	void leave()  //!< Leave stack level
	{
		if (level == stack) {
			if (level->type == Type::array) SMLEventHandler::eventLeaveArray(0);
			level->init();
		}
		else if (level > stack) {
			--level;
			while (level->type == Type::array && level->current >= level->length) {
				SMLEventHandler::eventLeaveArray(level-stack);
				if (level > stack) {
					--level;
				}
				else {
					level->init();
				}
			}
		}
		SML_DBG_TRACE("Leave - now at level " << getLevel())
	}

	void toSum()  //! Change to and initialize sum state
	{
		state = State::sum;
		expSum = 0;
		n01 = 0;
	}

	void sum(const byte ch) //!< Gather and verify sum
	{
		if (n01 == 0 && ch != 0x1a) {
			// Not a sum, assume outside and start parsing again
			toOutside();
			SMLEventHandler::eventEnd(true);
			SML_DBG_INFO("Message complete");
			parse(ch);
		}
		if (n01 < 2) crc16(ch);
		if (n01 >= 2) {
			expSum |= ch << (8 * (n01 & 1));
		}
		++n01;
		if (n01 == 4) {
			SMLEventHandler::eventEnd(expSum == actSum);
			if (expSum != actSum) {
				SML_DBG_ERROR("Wrong sum, expected sum: " << std::hex << expSum << " - actual sum: " << actSum)
			}
			else {
				SML_DBG_INFO("Message complete");
			}
			toOutside();
		}
	}

	void crc16(const byte ch)  // Calculate crc16 on the fly
	{
		// TODO: Use PROGMEM on linux
		static uint16_t crc16_table[] SML_PROGMEM = {
				0xf78f, 0xe70e, 0xd68d, 0xc60c,
				0xb58b, 0xa50a, 0x9489, 0x8408,
				0x7387, 0x6306, 0x5285, 0x4204,
				0x3183, 0x2102, 0x1081, 0x0000
		};

		actSum = ( 0xf000 | (actSum >> 4) ) ^ SML_PROGMEM_READ_WORD(crc16_table + ((actSum & 0xf) ^ (ch & 0xf)));
		actSum = ( 0xf000 | (actSum >> 4) ) ^ SML_PROGMEM_READ_WORD(crc16_table + ((actSum & 0xf) ^ (ch >> 4)));
	}


#if defined(SML_DBG_LEVEL) && SML_DBG_LEVEL > 0
	int getLevel() const
	{
		return level-stack+1;
	}
#endif

};
