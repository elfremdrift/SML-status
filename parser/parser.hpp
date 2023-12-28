#pragma once
#include <array>
#include <stdint.h>

#ifndef byte
#define byte uint8_t
#endif

class SMLParser {
public:
	void reset();
	void parse(byte ch);

private:
	bool isInside = false;
	byte level = 0;  //!< Stack level
	struct StackEntry {
		enum class Type : byte {
			  tbd = 255
			, string = 0
			, boolean = 4
			, integer = 5
			, uinteger = 6
			, array = 7
		} type = Type::tbd;
		uint16_t length = 0;
		uint16_t current = 0;
		bool lengthFollows = false;
		void init();
		void tbd(SMLParser& parser, const byte ch);  //!< Parse type byte to determine entry type
		void array(SMLParser& parser, const byte ch);  //!< Parse array
		void boolean(SMLParser& parser, const byte ch);  //!< Parse array
		void integer(SMLParser& parser, const byte ch, const bool hasSign);  //!< Parse integer
		void string(SMLParser& parser, const byte ch);  //!< Parse integer
		void done(SMLParser& parser);
		union {
			uint64_t	uinteger;
			int64_t		integer;
			bool		boolean;
			byte		string[sizeof(uint64_t)];
		} value = { 0 };
		union {
			byte* next;
			byte protoBytes;
		};
	};
	using Type = StackEntry::Type;
	std::array<StackEntry, 6> stack;
	byte n1b = 0;
	byte n01 = 0;

	void outside(const byte ch); //!< Look for start sequence
	void inside(const byte ch); //!< Parse SML structure
	void enter();  //!< Enter next stack level
	void leave();  //!< Leave stack level
};
