#include "ctype.h"

unsigned short ctype_type[257] = {
	/*  -1 (EOF) */		CTYPE_IS_CNTRL,
	/*   0 \0 */		CTYPE_IS_CNTRL,
	/*   1  */			CTYPE_IS_CNTRL,
	/*   2  */			CTYPE_IS_CNTRL,
	/*   3  */			CTYPE_IS_CNTRL,
	/*   4  */			CTYPE_IS_CNTRL,
	/*   5  */			CTYPE_IS_CNTRL,
	/*   6  */			CTYPE_IS_CNTRL,
	/*   7  */			CTYPE_IS_CNTRL,
	/*   8  */			CTYPE_IS_CNTRL,
	/*   9 '\t' */		CTYPE_IS_CNTRL | CTYPE_IS_SPACE | CTYPE_IS_BLANK,
	/*  10 '\n' */		CTYPE_IS_CNTRL | CTYPE_IS_SPACE,
	/*  11  */			CTYPE_IS_CNTRL | CTYPE_IS_SPACE,
	/*  12  */			CTYPE_IS_CNTRL | CTYPE_IS_SPACE,
	/*  13  */			CTYPE_IS_SPACE,
	/*  14  */			CTYPE_IS_CNTRL,
	/*  15  */			CTYPE_IS_CNTRL,
	/*  16  */			CTYPE_IS_CNTRL,
	/*  17  */			CTYPE_IS_CNTRL,
	/*  18  */			CTYPE_IS_CNTRL,
	/*  19  */			CTYPE_IS_CNTRL,
	/*  20  */			CTYPE_IS_CNTRL,
	/*  21  */			CTYPE_IS_CNTRL,
	/*  22  */			CTYPE_IS_CNTRL,
	/*  23  */			CTYPE_IS_CNTRL,
	/*  24  */			CTYPE_IS_CNTRL,
	/*  25  */			CTYPE_IS_CNTRL,
	/*  26  */			CTYPE_IS_CNTRL,
	/*  27  */			CTYPE_IS_CNTRL,
	/*  28  */			CTYPE_IS_CNTRL,
	/*  29  */			CTYPE_IS_CNTRL,
	/*  30  */			CTYPE_IS_CNTRL,
	/*  31  */			CTYPE_IS_CNTRL,
	/*  32 ' ' */		CTYPE_IS_SPACE | CTYPE_IS_BLANK | CTYPE_32,
	/*  33 '!' */		CTYPE_IS_PUNCT,
	/*  34 '"' */		CTYPE_IS_PUNCT,
	/*  35 '#' */		CTYPE_IS_PUNCT,
	/*  36 '$' */		CTYPE_IS_PUNCT,
	/*  37 '%' */		CTYPE_IS_PUNCT,
	/*  38 '&' */		CTYPE_IS_PUNCT,
	/*  39 ''' */		CTYPE_IS_PUNCT,
	/*  40 '(' */		CTYPE_IS_PUNCT,
	/*  41 ')' */		CTYPE_IS_PUNCT,
	/*  42 '*' */		CTYPE_IS_PUNCT,
	/*  43 '+' */		CTYPE_IS_PUNCT,
	/*  44 ',' */		CTYPE_IS_PUNCT,
	/*  45 '-' */		CTYPE_IS_PUNCT,
	/*  46 '.' */		CTYPE_IS_PUNCT,
	/*  47 '/' */		CTYPE_IS_PUNCT,
	/*  48 '0' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  49 '1' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  50 '2' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  51 '3' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  52 '4' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  53 '5' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  54 '6' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  55 '7' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  56 '8' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  57 '9' */		CTYPE_IS_DIGIT | CTYPE_IS_XDIGIT,
	/*  58 ':' */		CTYPE_IS_PUNCT,
	/*  59 ';' */		CTYPE_IS_PUNCT,
	/*  60 '<' */		CTYPE_IS_PUNCT,
	/*  61 '=' */		CTYPE_IS_PUNCT,
	/*  62 '>' */		CTYPE_IS_PUNCT,
	/*  63 '?' */		CTYPE_IS_PUNCT,
	/*  64 '@' */		CTYPE_IS_PUNCT,
	/*  65 'A' */		CTYPE_IS_UPPER | CTYPE_IS_XDIGIT,
	/*  66 'B' */		CTYPE_IS_UPPER | CTYPE_IS_XDIGIT,
	/*  67 'C' */		CTYPE_IS_UPPER | CTYPE_IS_XDIGIT,
	/*  68 'D' */		CTYPE_IS_UPPER | CTYPE_IS_XDIGIT,
	/*  69 'E' */		CTYPE_IS_UPPER | CTYPE_IS_XDIGIT,
	/*  70 'F' */		CTYPE_IS_UPPER | CTYPE_IS_XDIGIT,
	/*  71 'G' */		CTYPE_IS_UPPER,
	/*  72 'H' */		CTYPE_IS_UPPER,
	/*  73 'I' */		CTYPE_IS_UPPER,
	/*  74 'J' */		CTYPE_IS_UPPER,
	/*  75 'K' */		CTYPE_IS_UPPER,
	/*  76 'L' */		CTYPE_IS_UPPER,
	/*  77 'M' */		CTYPE_IS_UPPER,
	/*  78 'N' */		CTYPE_IS_UPPER,
	/*  79 'O' */		CTYPE_IS_UPPER,
	/*  80 'P' */		CTYPE_IS_UPPER,
	/*  81 'Q' */		CTYPE_IS_UPPER,
	/*  82 'R' */		CTYPE_IS_UPPER,
	/*  83 'S' */		CTYPE_IS_UPPER,
	/*  84 'T' */		CTYPE_IS_UPPER,
	/*  85 'U' */		CTYPE_IS_UPPER,
	/*  86 'V' */		CTYPE_IS_UPPER,
	/*  87 'W' */		CTYPE_IS_UPPER,
	/*  88 'X' */		CTYPE_IS_UPPER,
	/*  89 'Y' */		CTYPE_IS_UPPER,
	/*  90 'Z' */		CTYPE_IS_UPPER,
	/*  91 '[' */		CTYPE_IS_PUNCT,
	/*  92 '\' */		CTYPE_IS_PUNCT,
	/*  93 ']' */		CTYPE_IS_PUNCT,
	/*  94 '^' */		CTYPE_IS_PUNCT,
	/*  95 '_' */		CTYPE_IS_PUNCT,
	/*  96 '`' */		CTYPE_IS_PUNCT,
	/*  97 'a' */		CTYPE_IS_LOWER | CTYPE_IS_XDIGIT,
	/*  98 'b' */		CTYPE_IS_LOWER | CTYPE_IS_XDIGIT,
	/*  99 'c' */		CTYPE_IS_LOWER | CTYPE_IS_XDIGIT,
	/* 100 'd' */		CTYPE_IS_LOWER | CTYPE_IS_XDIGIT,
	/* 101 'e' */		CTYPE_IS_LOWER | CTYPE_IS_XDIGIT,
	/* 102 'f' */		CTYPE_IS_LOWER | CTYPE_IS_XDIGIT,
	/* 103 'g' */		CTYPE_IS_LOWER,
	/* 104 'h' */		CTYPE_IS_LOWER,
	/* 105 'i' */		CTYPE_IS_LOWER,
	/* 106 'j' */		CTYPE_IS_LOWER,
	/* 107 'k' */		CTYPE_IS_LOWER,
	/* 108 'l' */		CTYPE_IS_LOWER,
	/* 109 'm' */		CTYPE_IS_LOWER,
	/* 110 'n' */		CTYPE_IS_LOWER,
	/* 111 'o' */		CTYPE_IS_LOWER,
	/* 112 'p' */		CTYPE_IS_LOWER,
	/* 113 'q' */		CTYPE_IS_LOWER,
	/* 114 'r' */		CTYPE_IS_LOWER,
	/* 115 's' */		CTYPE_IS_LOWER,
	/* 116 't' */		CTYPE_IS_LOWER,
	/* 117 'u' */		CTYPE_IS_LOWER,
	/* 118 'v' */		CTYPE_IS_LOWER,
	/* 119 'w' */		CTYPE_IS_LOWER,
	/* 120 'x' */		CTYPE_IS_LOWER,
	/* 121 'y' */		CTYPE_IS_LOWER,
	/* 122 'z' */		CTYPE_IS_LOWER,
	/* 123 '{' */		CTYPE_IS_PUNCT,
	/* 124 '|' */		CTYPE_IS_PUNCT,
	/* 125 '}' */		CTYPE_IS_PUNCT,
	/* 126 '~' */		CTYPE_IS_PUNCT,
	/* 127 DEL */		CTYPE_IS_CNTRL
	/* ... */
};

int tolower(int c){
	return isupper(c) ? (c - 'A') + 'a' : c;
}

int toupper(int c){
	return islower(c) ? (c - 'a') + 'A' : c;
}
