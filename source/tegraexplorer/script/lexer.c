#include "lexer.h"
#include "parser.h"
#include "args.h"
#include "../utils/utils.h"
#include "../../mem/heap.h"
#include <string.h>

static inline int isValidWord(char c) {
	char r = c | 0x20;
	return (r >= 'a' && r <= 'z');
}

static inline int isValidVar(char c) {
	char r = c | 0x20;
	return ((r >= 'a' && r <= 'z') || (r >= '0' && r <= '9'));
}

static inline int isValidNum(char c) {
	return (c >= '0' && c <= '9');
}

static inline int isValidHexNum(char c) {
	char r = c | 0x20;
	return (isValidNum(r) || (r >= 'a' && r <= 'f'));
}

#define makeLexarToken(token, var) (lexarToken_t) {token, {var}}
#define makeLexarIntToken(intVar) (lexarToken_t) {IntLit, .val = intVar}

typedef struct {
	u8 tokenC;
	u16 tokenN;
} lexarTranslation_t;

lexarTranslation_t lexarTranslations[] = {
	{'(', LBracket},
	{')', RBracket},
	{'{', LCBracket},
	{'}', RCBracket},
	{',', Seperator},
	{'+', Plus},
	{'-', Minus},
	{'*', Multiply},
	{'/', Division},
	{'%', Mod},
	{'<', Smaller},
	{'>', Bigger},
	{'=', Equal},
	{'!', Not},
	{'[', LSBracket},
	{']', RSBracket},
	{'\0', 0},
};

char lexarDebugGetTokenC(u16 tokenN) {
	for (int i = 0; lexarTranslations[i].tokenC; i++) {
		if (lexarTranslations[i].tokenN == tokenN) {
			return lexarTranslations[i].tokenC;
		}
	}

	return '?';
}

lexarVector_t lexarVectorInit(int startSize) {
	lexarVector_t l = { 0 };
	l.tokens = calloc(startSize, sizeof(lexarVector_t));
	l.len = startSize;
	l.stored = 0;
	return l;
}

void lexarVectorAdd(lexarVector_t* vec, lexarToken_t token) {
	if (vec->stored >= vec->len) {
		vec->len *= 2;
		//vec->tokens = realloc(vec->tokens, vec->len * sizeof(lexarToken_t));
		void *temp = calloc(vec->len, sizeof(lexarToken_t));
		memcpy(temp, vec->tokens, vec->len / 2 * sizeof(lexarToken_t));
		free(vec->tokens);
		vec->tokens = temp;
	}

	vec->tokens[vec->stored++] = token;
}

void lexarVectorClear(lexarVector_t* vec) {
	for (int i = 0; i < vec->stored; i++) {
		if (vec->tokens[i].token == Variable || vec->tokens[i].token == StrLit)
			if (vec->tokens[i].text != NULL)
				free(vec->tokens[i].text);
	}
	free(vec->tokens);
}

#define ELIFC(c) else if (*in == c)

lexarVector_t lexarGo(const char* in) {
	lexarVector_t vec = lexarVectorInit(16);

	while (*in) {
		if (isValidWord(*in)) {
			char* startWord = in;
			in++;
			while (isValidVar(*in))
				in++;

			lexarVectorAdd(&vec, makeLexarToken(Variable, utils_copyStringSize(startWord, in - startWord)));
			continue;
		}
		else if (isValidNum(*in) || (*in == '-' && isValidNum(in[1]))) {
			int parse = 0;
			u8 negative = (*in == '-');
			if (negative)
				in++;

			if (*in == '0' && (in[1] | 0x20) == 'x') {
				in += 2;
				while (isValidHexNum(*in)) {
					parse = parse * 16 + (*in & 0x0F) + (*in >= 'A' ? 9 : 0);
					in++;
				}
			}
			else while (isValidNum(*in)) {
				parse = parse * 10 + *in++ - '0';
			}

			if (negative)
				parse *= -1;

			lexarVectorAdd(&vec, makeLexarIntToken(parse));
			continue;
		}
		ELIFC('"') {
			char* startStr = ++in;
			while (*in != '"')
				in++;

			lexarVectorAdd(&vec, makeLexarToken(StrLit, utils_copyStringSize(startStr, in - startStr)));
			in++;
			continue;
		}
		ELIFC('#') {
			while (*in != '\n')
				in++;
			
			in++;
			continue;
		}
		ELIFC('&') {
			if (in[1] == '&') {
				lexarVectorAdd(&vec, makeLexarToken(LogicAND, 0));
				in++;
			}
			else {
				lexarVectorAdd(&vec, makeLexarToken(AND, 0));
			}

			in++;
			continue;
		}
		ELIFC('|') {
			if (in[1] == '|') {
				lexarVectorAdd(&vec, makeLexarToken(LogicOR, 0));
				in++;
			}
			else {
				lexarVectorAdd(&vec, makeLexarToken(OR, 0));
			}

			in++;
			continue;
		}

		int val = 0;

		for (int i = 0; lexarTranslations[i].tokenC; i++) {
			if (lexarTranslations[i].tokenC == *in) {
				val = lexarTranslations[i].tokenN;
				break;
			}
		}

		in++;

		if (*in == '=' && val >= Smaller && val <= Not) {
			val++;
			in++;
		}

		if (val != Invalid)
			lexarVectorAdd(&vec, makeLexarToken(val, 0));
	}

	return vec;
}