#pragma once
#include <array>
#include <stdint.h>

#ifndef byte
#define byte uint8_t
#endif

#define SML_STACK_SIZE 10
#define SML_STRING_MAX_LEN 64

class SMLParser {
public:
	void reset();
	void parse(byte ch);

private:
	enum class State : byte {
	      outside = 0
		, inside = 1
		, sum = 2
	} state = State::outside;
	struct StackEntry {
		enum class Type : byte {
			  tbd = 255
			, string = 0
			, boolean = 4
			, integer = 5
			, uinteger = 6
			, array = 7
		} type;
		uint16_t length;
		uint16_t current;
		void init();
	};
	union {
		uint64_t	uinteger;
		int64_t		integer;
		bool		boolean;
		byte		string[SML_STRING_MAX_LEN];
	} value;
	union {
		byte* next;
		byte protoBytes;
	};
	bool lengthFollows = false;
	using Type = StackEntry::Type;
	StackEntry stack[SML_STACK_SIZE];
	StackEntry* level = stack;  //!< Current stack level
	byte n1b = 0;
	byte n01 = 0;

	uint16_t actSum, expSum;

	void toOutside();  //! Change to and initialize outside state
	void outside(const byte ch); //!< Look for start sequence
	void toInside();  //! Change to and initialize inside state
	void inside(const byte ch); //!< Parse SML structure
	void toSum();  //! Change to and initialize sum state
	void sum(const byte ch); //!< Gather and verify sum

	bool tbd(const byte ch);  //!< Parse type byte to determine entry type
	bool array(const byte ch);  //!< Parse array
	bool boolean(const byte ch);  //!< Parse array
	bool integer(const byte ch, const bool hasSign);  //!< Parse integer
	bool string(const byte ch);  //!< Parse integer
	void done();

	void crc16(const byte ch);  // Calculate crc16 on the fly
	void x25crc16(const byte ch);  // Calculate crc16 on the fly

	void enter();  //!< Enter next stack level
	void leave();  //!< Leave stack level

#if defined(DBG_LEVEL) && DBG_LEVEL > 0
	int getLevel() const;
#endif
};
