#include "lexer.h"
#include "types.h"
#include "args.h"
#include <mem/heap.h>

static inline int isValidWord(char c) {
	char r = c | 0x20;
	return ((r >= 'a' && r <= 'z') || c == '_');
}

static inline int isValidNum(char c) {
	return (c >= '0' && c <= '9');
}

static inline int isValidVar(char c) {
	return (isValidWord(c) || isValidNum(c));
}

static inline int isValidHexNum(char c) {
	char r = c | 0x20;
	return (isValidNum(r) || (r >= 'a' && r <= 'f'));
}

#define makeLexarToken(token, var) ((lexarToken_t) {token, var})

typedef struct {
	u8 tokenC;
	u8 tokenN;
} lexarTranslation_t;

lexarTranslation_t lexarTranslations[] = {
	{'}', RCBracket},
	{',', Seperator},
	{'+', Plus},
	{'-', Minus},
	{'*', Multiply},
	{'/', Division},
	{'%', Mod},
	{'<', Smaller},
	{'>', Bigger},
	{'!', Not},
	{':', Selector},
	{')', RBracket},
	{']', RSBracket},
	{'(', LBracket},
	{'{', LCBracket},
	{'=', Equal},
	{'[', LSBracket},
	{'\0', 0},
};

/*
	Should we make vars with next char being '(' a function and vars with an equals (or [x] wait how are we gonna spot that) after it to be an assignmentVar
*/

char lexarDebugGetTokenC(u8 tokenN) {
	for (int i = 0; lexarTranslations[i].tokenC; i++) {
		if (lexarTranslations[i].tokenN == tokenN) {
			return lexarTranslations[i].tokenC;
		}
	}

	if (tokenN == EquationSeperator)
		return ';';

	return '?';
}

/*
* !! we need to remake this
void lexarVectorClear(lexarVector_t* vec) {
	for (int i = 0; i < vec->stored; i++) {
		if (vec->tokens[i].token == Variable || vec->tokens[i].token == StrLit)
			if (vec->tokens[i].text != NULL)
				free(vec->tokens[i].text);
	}
	free(vec->tokens);
}
*/

void lexarVectorClear(Vector_t *v){
	vecPDefArray(lexarToken_t*, entries, v);

	for (int i = 0; i < v->count; i++){
		if (entries[i].token != IntLit && entries[i].text != NULL){
			free(entries[i].text);
		}
	}

	vecFreePtr(v);
}

#define ELIFC(c) else if (*in == c)

Vector_t runLexar(const char* in, u32 len) {
	const char *start = in;
	Vector_t vec = newVec(sizeof(lexarToken_t), 16);
	// store last var for re-assignment
	// var -> func if next obj is '('
	// var -> assignment if next obj is '='
	// var -> arrassignment if next obj is '[' and before '=' is ']'
	// maybe measure len between ( ) and [ ], so this doesn't have to be done during runtime?
		// We also have to support (()). maybe if '(' set indent level, then if ')' minus indent level, set len. indent level contains {u8 level, u16 token, u16 startoffset}

	u32 lastAssignment = 0;

	while ((in - start) < len) {
		lexarToken_t* lx = vecGetArray(lexarToken_t*, vec);

		if ((lx[vec.count - 2].token == StrLit || lx[vec.count - 2].token == IntLit || lx[vec.count - 2].token == Variable || lx[vec.count - 2].token == RSBracket || lx[vec.count - 2].token == RBracket)
			&& (lx[vec.count - 1].token == Variable || lx[vec.count - 1].token == LCBracket || lx[vec.count - 1].token == RCBracket)) {
			if (!(lx[lastAssignment].token == ArrayVariableAssignment && lx[vec.count - 1].token == Variable && lx[vec.count - 2].token == RSBracket)) {
				lexarToken_t holder = lx[vec.count - 1];
				lx[vec.count - 1] = makeLexarToken(EquationSeperator, 0);
				vecAddElement(&vec, holder);
				lx = vecGetArray(lexarToken_t*, vec);
			}
		}

		if (isValidWord(*in)) {
			char* startWord = in;
			in++;
			while (isValidVar(*in))
				in++;

			vecAddElement(&vec, (makeLexarToken(Variable, utils_copyStringSize(startWord, in - startWord))));
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

			vecAddElement(&vec, makeLexarToken(IntLit, parse));
			continue;
		}
		ELIFC('(') {
			if (lx[vec.count - 1].token == Variable)
				lx[vec.count - 1].token = Function;

			vecAddElement(&vec, makeLexarToken(LBracket, 0));
		}
		ELIFC('[') {
			if (lx[vec.count - 1].token == Variable)
				lx[vec.count - 1].token = ArrayVariable;

			vecAddElement(&vec, makeLexarToken(LSBracket, 0));
		}
		ELIFC('=') { // Do we need to keep = if the vars are assignments anyway?
			if (lx[vec.count - 1].token == Variable)
				lx[vec.count - 1].token = VariableAssignment;

			else if (lx[vec.count - 1].token == RSBracket) {
				int back = 1;
				while (lx[vec.count - back].token != ArrayVariable) {
					back++;
					if (vec.count - back < 0)
						break; // major error
				}
				if (lx[vec.count - back].token == ArrayVariable) {
					lx[vec.count - back].token = ArrayVariableAssignment;
					lastAssignment = vec.count - back;
					in++;
					continue;
				}
			}
			lastAssignment = 0;
		}
		ELIFC('{') {
			if (lx[vec.count - 1].token == VariableAssignment) {
				lx[vec.count - 1].token = FunctionAssignment;
			}
			vecAddElement(&vec, makeLexarToken(LCBracket, 0));
		}
		ELIFC('"') {
			char* startStr = ++in;
			int len = 0;
			while (*in != '"') {
				in++;
			}
			len = in - startStr;

			char* storage = malloc(len + 1);

			int pos = 0;
			for (int i = 0; i < len; i++) {
				if (startStr[i] == '\\') {
					if (startStr[i + 1] == 'n') {
						storage[pos++] = '\n';
						i++;
						continue;
					}
					
					if (startStr[i + 1] == 'r') {
						storage[pos++] = '\r';
						i++;
						continue;
					}
				}
				storage[pos++] = startStr[i];
			}
			storage[pos] = '\0';

			vecAddElement(&vec, makeLexarToken(StrLit, storage));
		}
		ELIFC('#') {
			while (*in != '\n')
				in++;
		}
		ELIFC('&') {
			if (in[1] == '&') {
				vecAddElement(&vec, makeLexarToken(LogicAND, 0));
				in++;
			}
			else {
				vecAddElement(&vec, makeLexarToken(AND, 0));
			}
		}
		ELIFC('|') {
			if (in[1] == '|') {
				vecAddElement(&vec, makeLexarToken(LogicOR, 0));
				in++;
			}
			else {
				vecAddElement(&vec, makeLexarToken(OR, 0));
			}
		}
		else {
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
				vecAddElement(&vec, makeLexarToken(val, 0));

			continue;
		}
		in++;
	}

	lexarToken_t* lx = vecGetArray(lexarToken_t*, vec);
	if ((lx[vec.count - 2].token == StrLit || lx[vec.count - 2].token == IntLit || lx[vec.count - 2].token == Variable || lx[vec.count - 2].token == RSBracket || lx[vec.count - 2].token == RBracket)
		&& (lx[vec.count - 1].token == Variable || lx[vec.count - 1].token == LCBracket || lx[vec.count - 1].token == RCBracket)) {
		lexarToken_t holder = lx[vec.count - 1];
		lx[vec.count - 1] = makeLexarToken(EquationSeperator, 0);
		vecAddElement(&vec, holder);
	}

	vecAddElement(&vec, makeLexarToken(EquationSeperator, 0));
	return vec;
}