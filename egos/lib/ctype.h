#define CTYPE_IS_PUNCT		(1 << 0)
#define CTYPE_IS_LOWER		(1 << 1)
#define CTYPE_IS_UPPER		(1 << 2)
#define CTYPE_IS_DIGIT		(1 << 3)
#define CTYPE_IS_XDIGIT		(1 << 4)
#define CTYPE_IS_BLANK		(1 << 5)
#define CTYPE_IS_SPACE		(1 << 6)
#define CTYPE_IS_CNTRL		(1 << 7)
#define CTYPE_32			(1 << 8)

#define isascii(c)			((unsigned) (c) < 128)

#define islower(c)			(ctype_type[(c) + 1] & CTYPE_IS_LOWER)
#define isupper(c)			(ctype_type[(c) + 1] & CTYPE_IS_UPPER)
#define isalpha(c)			(ctype_type[(c) + 1] & (CTYPE_IS_LOWER | CTYPE_IS_UPPER))
#define isdigit(c)			(ctype_type[(c) + 1] & CTYPE_IS_DIGIT)
#define isxdigit(c)			(ctype_type[(c) + 1] & CTYPE_IS_XDIGIT)
#define isalnum(c)			(ctype_type[(c) + 1] & (CTYPE_IS_LOWER | CTYPE_IS_UPPER | CTYPE_IS_DIGIT))
#define isspace(c)			(ctype_type[(c) + 1] & CTYPE_IS_SPACE)
#define isblank(c)			(ctype_type[(c) + 1] & CTYPE_IS_BLANK)
#define isgraph(c)			(ctype_type[(c) + 1] & (CTYPE_IS_PUNCT | CTYPE_IS_DIGIT | CTYPE_IS_LOWER | CTYPE_IS_UPPER))
#define isprint(c)			(ctype_type[(c) + 1] & (CTYPE_IS_PUNCT | CTYPE_IS_DIGIT | CTYPE_IS_LOWER | CTYPE_IS_UPPER | CTYPE_32))
#define ispunct(c)			(ctype_type[(c) + 1] & CTYPE_IS_PUNCT)
#define iscntrl(c)			(ctype_type[(c) + 1] & CTYPE_IS_CNTRL)

#define toascii(c)			((c) & 0x7F)

extern unsigned short ctype_type[257];

int tolower(int c);
int toupper(int c);
