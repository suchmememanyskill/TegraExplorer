#include "model.h"
#include "compat.h"
#include "compat.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>

#include "intClass.h"
#include "StringClass.h"
#include "unsolvedArrayClass.h"
#include "functionClass.h"

#include "scriptError.h"
#include "standardLibrary.h"

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

char* getTokenText(u8 token) {
	for (u32 i = 0; i < tokenConvertionCount; i++) {
		if (tokenConvertions[i].token == token)
			return tokenConvertions[i].strToken;
	}

	return NULL;
}

char* utils_copyStringSize(const char* in, int size) {
	if (size > strlen(in) || size < 0)
		size = strlen(in);

	char* out = calloc(size + 1, 1);
	//strncpy(out, in, size);
	if (size)
		memcpy(out, in, size);
	return out;
}

#define ELIFC(c) else if (*in == c)

Vector_t script;
s64 lineNumber;

enum TokenType {
	Token_Variable = 0,
	Token_String,
	Token_Int,
	Token_Token,
	Token_Err,
	Token_Fatal_Err,
};

typedef enum {
	History_Function = 0,
	History_Bracket,
	History_Array,
} StackHistory_t;

u8 nextToken(char** inPtr, void** val) {
	char* in = *inPtr;
	u8 ret = Token_Err;
	while (ret == Token_Err) {
		if (*in == '#') {
			if (!memcmp(in + 1, "REQUIRE ", 8)) {
				if (!memcmp(in + 9, "VER ", 4)) {
					u8 vers[3] = { 0 };
					char* verStart = in + 13;
					for (u8 i = 0; i < 3; i++) {
						while (isValidNum(*verStart)) {
							vers[i] = vers[i] * 10 + *verStart++ - '0';
						}
						verStart++;
					}

					u8 outdated = 0;
					if (vers[0] > LP_VER_MJ)
						outdated = 1;
					else if (vers[0] == LP_VER_MJ) {
						if (vers[1] > LP_VER_MN)
							outdated = 1;
						else if (vers[1] == LP_VER_MN) {
							if (vers[2] > LP_VER_BF)
								outdated = 1;
						}
					}

					if (outdated) {
						printScriptError(SCRIPT_FATAL, "Script requires TegraExplorer %d.%d.%d or up!", vers[0], vers[1], vers[2]);
						return Token_Fatal_Err;
					}
				}
				else if (!memcmp(in + 9, "MINERVA", 7)) {
					u8 minervaEnabled = 0; // TODO: Change this to the actual value
					if (!minervaEnabled) {
						printScriptError(SCRIPT_FATAL, "Extended memory required.\nPut the bootloader folder from hekate on your sd!");
						return Token_Fatal_Err;
					}
				}
			}

			while (*in && *in != '\n')
				in++;
		}
		else if (isValidWord(*in)) {
			char* startWord = in;
			in++;
			while (isValidVar(*in))
				in++;

			char* str = utils_copyStringSize(startWord, in - startWord);

			//gfx_printf("Variable: '%s'\n", str);
			ret = Token_Variable;
			*val = str;
			break;
		}
		else if (isValidNum(*in) || (*in == '-' && isValidNum(in[1]))) {
			s64 parse = 0;
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

			//gfx_printf("Integer: '%d'\n", parse);
			ret = Token_Int;

			s64* parsePersistent = malloc(sizeof(s64));
			*parsePersistent = parse;

			*val = parsePersistent;
			break;
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

			//gfx_printf("String: '%s'\n", storage);
			ret = Token_String;
			*val = storage;
		}
		else if (*in == '\0') {
			*inPtr = in;
			return ret;
		}
		else if (*in == '\n'){
			lineNumber++;
			scriptCurrentLine = lineNumber;
		}
		else {
			for (u32 i = 0; i < tokenConvertionCount; i++) {
				TokenConvertion_t t = tokenConvertions[i];
				if (!memcmp(t.strToken, in, (t.strToken[1] == '\0') ? 1 : 2)) {
					//gfx_printf("Token: '%s'\n", t.strToken);
					ret = Token_Token;
					*val = t.token;

					if (t.strToken[1] != '\0')
						in++;

					break;
				}
			}
		}
		in++;
	}

	*inPtr = in;
	return ret;
}

#define CreateVariableReferenceStatic(var) VariableReference_t reference = { .staticVariable = var, .staticVariableSet = 1, .staticVariableRef = 1 }
#define CreateVariableReferenceStr(str) VariableReference_t reference = { .name = str }

void setNextActionOperator(Vector_t *opHolder, ActionType_t action, ActionExtraType_t actionExtra, void *extra) {
	Operator_t* ops = opHolder->data;
	Operator_t* lastOp = &ops[opHolder->count - 1];

	if (lastOp->token == CallArgs) {
		CallArgs_t* last = &lastOp->callArgs;
		for (; last->next != NULL; last = last->next);
		last->next = calloc(sizeof(CallArgs_t), 1);
		last->next->action = action;
		last->next->extra = extra;
		last->next->extraAction = actionExtra;
	}
	else {
		Operator_t newOp = { .token = CallArgs };
		newOp.callArgs.action = action;
		newOp.callArgs.extra = extra;
		newOp.callArgs.extraAction = actionExtra;
		vecAdd(opHolder, newOp);
	}
}

CallArgs_t* getLastRef(CallArgs_t* ref) {
	for (; ref->next != NULL; ref = ref->next);
	return ref;
}

int isLastVarSet(Operator_t* opHolder) {
	return (opHolder->token == CallArgs && getLastRef(&opHolder->callArgs)->action == ActionSet);
}

int isLastVarCall(Operator_t* opHolder) {
	return (opHolder->token == CallArgs && getLastRef(&opHolder->callArgs)->action == ActionCall);
}

ParserRet_t parseScript(char* in) {
	Vector_t functionStack; // Function_t
	Vector_t stackHistoryHolder; // StaticHistory_t
	Vector_t staticVariableHolder; // Variable_t

	functionStack = newVec(sizeof(Function_t), 0);
	Function_t firstFunction = createEmptyFunction();
	vecAdd(&functionStack, firstFunction);

	staticVariableHolder = newVec(sizeof(Variable_t), 0);

	stackHistoryHolder = newVec(sizeof(StackHistory_t), 1);
	StackHistory_t firstHistory = History_Function;
	vecAdd(&stackHistoryHolder, firstHistory);
	u8 notNext = 0;
	lineNumber = 1;
	scriptCurrentLine = 1;

	while (*in) {
		Function_t* lastFunc = getStackEntry(&functionStack);
		StackHistory_t* lastHistory = getStackEntry(&stackHistoryHolder);

		Operator_t* lastOp = NULL;

		if (lastFunc) {
			lastOp = getStackEntry(&lastFunc->operations);
		}

		void* var = NULL;
		u8 tokenType = nextToken(&in, &var);

		if (tokenType == Token_Err)
			break;

		if (tokenType == Token_Fatal_Err)
			return (ParserRet_t) { 0 };

		Operator_t op = { .token = Variable };

		if (tokenType >= Token_Variable && tokenType <= Token_Int && lastOp) {
			if (lastOp->token == Variable || lastOp->token == BetweenBrackets || (lastOp->token == CallArgs && !isLastVarSet(lastOp))) {
				op.token = EquationSeperator;
				op.lineNumber = lineNumber;
				vecAdd(&lastFunc->operations, op);
				op.token = Variable;
			}
		}

		if (tokenType == Token_Variable) {

			ClassFunctionTableEntry_t* cfte = searchStdLib(var);

			if (cfte == NULL) {
				CreateVariableReferenceStr(var);
				op.variable = reference;
			}
			else {
				VariableReference_t reference = { .staticVariableType = 3, .staticFunction = cfte };
				op.variable = reference;
			}
		}
		else if (tokenType == Token_Int) {
			/*
			Variable_t a = newIntVariable(*((s64*)var));
			a.gcDoNotFree = 1;
			free(var);
			vecAdd(&staticVariableHolder, a);
			CreateVariableReferenceStatic((Variable_t*)(staticVariableHolder.count - 1));
			*/

			VariableReference_t reference = { .staticVariableType = 1, .integerType = *((s64*)var) };
			op.variable = reference;
			free(var);
		}
		else if (tokenType == Token_String) {
			/*
			Variable_t a = newStringVariable(var, 1, 1);
			a.gcDoNotFree = 1;
			vecAdd(&staticVariableHolder, a);
			CreateVariableReferenceStatic((Variable_t*)(staticVariableHolder.count - 1));
			*/

			VariableReference_t reference = { .staticVariableType = 2, .stringType = var };
			op.variable = reference;
		}
		else if (tokenType == Token_Token) {
			u8 token = (u8)var;

			if (token == Equals && lastOp) {
				if (lastOp->token == Variable) {
					if (lastOp->variable.staticVariableSet) {
						SCRIPT_PARSER_ERR("Trying to assign to a static variable");
					}
					else {
						setNextActionOperator(&lastFunc->operations, ActionSet, ActionExtraNone, NULL);
						continue;
					}
				}
				else if (lastOp->token == CallArgs) {
					CallArgs_t* last = getLastRef(&lastOp->callArgs);
					last->action = ActionSet;
					continue;
				}
				else {
					SCRIPT_PARSER_ERR("Trying to assign to non-object");
				}
			}
			else if (token == LeftCurlyBracket) {
				Function_t templateFunction = createEmptyFunction();
				vecAdd(&functionStack, templateFunction);

				StackHistory_t functionHistory = History_Function;
				vecAdd(&stackHistoryHolder, functionHistory);
				continue;
			}
			else if (token == RightCurlyBracket) {
				if (stackHistoryHolder.count != 1 && *lastHistory == History_Function) {
					Function_t *popFunc = popStackEntry(&functionStack);
					popStackEntry(&stackHistoryHolder);

					lastFunc = getStackEntry(&functionStack);

					if (lastFunc) { // TODO: Add check for null deref
						lastOp = getStackEntry(&lastFunc->operations);
					}

					if (lastOp && (lastOp->token == Variable || (lastOp->token == CallArgs && !isLastVarSet(lastOp)))) {
						if (lastOp->token == Variable) {
							SCRIPT_PARSER_ERR("GET variable before {}");
							continue;
						}
						
						CallArgs_t* lastCall = getLastRef(&lastOp->callArgs);
						if (lastCall->extraAction == ActionExtraCallArgs) {
							Function_t* funcArgs = lastCall->extra;

							op.token = EquationSeperator;
							op.lineNumber = lineNumber;
							vecAdd(&funcArgs->operations, op);
							op.token = Variable;

							Variable_t a = newFunctionVariable(createFunctionClass(*popFunc, NULL));
							vecAdd(&staticVariableHolder, a);
							CreateVariableReferenceStatic((Variable_t*)(staticVariableHolder.count - 1));
							op.variable = reference;
							vecAdd(&funcArgs->operations, op);
							continue;
						}
					}



					Variable_t a = newFunctionVariable(createFunctionClass(*popFunc, NULL));
					vecAdd(&staticVariableHolder, a);
					CreateVariableReferenceStatic((Variable_t*)(staticVariableHolder.count - 1));
					op.variable = reference;
					vecAdd(&lastFunc->operations, op);

					op.token = EquationSeperator;
					op.lineNumber = lineNumber;
					vecAdd(&lastFunc->operations, op);
				}
				else {
					SCRIPT_PARSER_ERR("Stack count is 1 or state is not a function");
				}
			}
			else if (token == Dot) {
				if (lastOp && (lastOp->token == Variable || lastOp->token == BetweenBrackets || (lastOp->token == CallArgs && !isLastVarSet(lastOp)))) {
					tokenType = nextToken(&in, &var);
					if (tokenType != Token_Variable) {
						SCRIPT_PARSER_ERR("Acessing member with non-dynamic token");
					}
					else {
						setNextActionOperator(&lastFunc->operations, ActionGet, ActionExtraMemberName, var);
						continue;
					}
				}
				else {
					SCRIPT_PARSER_ERR("Member access on non-variable");
				}
			}
			else if (token == LeftBracket) {
				Function_t templateFunction = createEmptyFunction();
				vecAdd(&functionStack, templateFunction);

				StackHistory_t functionHistory = History_Bracket;
				vecAdd(&stackHistoryHolder, functionHistory);
				continue;
			}
			else if (token == RightBracket) { 
				if (*lastHistory == History_Bracket) {
					Function_t* bstack = popStackEntry(&functionStack);
					popStackEntry(&stackHistoryHolder);
					lastFunc = getStackEntry(&functionStack);

					if (lastFunc) { // TODO: Add check for null deref
						lastOp = getStackEntry(&lastFunc->operations);
					}

					if (lastOp && (lastOp->token == Variable || lastOp->token == BetweenBrackets || (lastOp->token == CallArgs && !isLastVarSet(lastOp) && !isLastVarCall(lastOp)))) {
						Function_t* newBStack = malloc(sizeof(Function_t));
						*newBStack = *bstack;
						setNextActionOperator(&lastFunc->operations, ActionCall, ActionExtraCallArgs, newBStack); // Maybe pass NULL if array is empty?
						continue;
					}
					else {
						if (lastOp && isLastVarCall(lastOp)) {
							op.token = EquationSeperator;
							vecAdd(&lastFunc->operations, op);
						}

						if (!countTokens(bstack, EquationSeperator)) {
							op.variable.betweenBrackets.data = bstack->operations.data;
							op.variable.betweenBrackets.len = bstack->operations.count;
							op.token = BetweenBrackets;
						}
						else {
							SCRIPT_PARSER_ERR("Priority brackets can only contain 1 argument");
 						}

					}
				}
				else {
					SCRIPT_PARSER_ERR(") without (");
				}
			}
			else if (token == LeftSquareBracket) {
				Function_t templateFunction = createEmptyFunction();
				vecAdd(&functionStack, templateFunction);

				StackHistory_t functionHistory = History_Array;
				vecAdd(&stackHistoryHolder, functionHistory);
				continue;
			}
			else if (token == RightSquareBracket) {
				if (*lastHistory == History_Array) {
					Function_t* astack = popStackEntry(&functionStack);
					popStackEntry(&stackHistoryHolder);
					lastFunc = getStackEntry(&functionStack);

					if (lastFunc) { // TODO: Add check for null deref
						lastOp = getStackEntry(&lastFunc->operations);
					}

					if (lastOp && (lastOp->token == Variable || (lastOp->token == CallArgs && !isLastVarSet(lastOp)))) {
						if (!countTokens(astack, EquationSeperator)) {
							Function_t* newAStack = malloc(sizeof(Function_t));
							*newAStack = *astack;
							setNextActionOperator(&lastFunc->operations, ActionGet, ActionExtraArrayIndex, newAStack);
							continue;
						}
						else {
							// We're just assuming that it's a new array lol

							op.token = EquationSeperator;
							vecAdd(&lastFunc->operations, op);
							op.token = Variable;


							Variable_t a = createUnsolvedArrayVariable(astack);
							vecAdd(&staticVariableHolder, a);
							CreateVariableReferenceStatic((Variable_t*)(staticVariableHolder.count - 1));
							op.variable = reference;

							//gfx_printf("[FATAL] indexes cannot contain mutiple arguments");
						}
					}
					else {
						// TODO: optimize output to a typed array, if possible
						Variable_t a = createUnsolvedArrayVariable(astack);
						vecAdd(&staticVariableHolder, a);
						CreateVariableReferenceStatic((Variable_t*)(staticVariableHolder.count - 1));
						op.variable = reference;
					}
				}
				else {
					SCRIPT_PARSER_ERR("] without [");
				}
			}
			else if (token == Not) {
				notNext = !notNext;
				continue;
			}
			else {
				op.token = token;

				for (u32 i = 0; i < tokenConvertionCount; i++) {
					if (token == tokenConvertions[i].token) {
						op.tokenStr = tokenConvertions[i].strToken;
						break;
					}
				}
			}
		}

		if (notNext) {
			op.not = 1;
			notNext = 0;
		}

		vecAdd(&lastFunc->operations, op);
	}

	if (functionStack.count != 1 || stackHistoryHolder.count != 1) {
		SCRIPT_PARSER_ERR("There seems to be an open bracket somewhere. EOF reached");
	}
		

	ParserRet_t parse = { .main = (*(Function_t*)getStackEntry(&functionStack)), .staticVarHolder = staticVariableHolder, .valid = 1 };

	vecFree(functionStack);
	vecFree(stackHistoryHolder);
	scriptCurrentLine = 1;
	return parse;
}


void exitFunction(Operator_t* start, u32 len) {
	for (u32 i = 0; i < len; i++) {
		if (start[i].token == Variable) {
			if (start[i].variable.staticVariableOptionsUnion == 0) {
				FREE(start[i].variable.name);
			}

			if (start[i].variable.staticVariableType == 2) {
				FREE(start[i].variable.stringType);
			}
		}
		else if (start[i].token == BetweenBrackets) {
			exitFunction(start[i].variable.betweenBrackets.data, start[i].variable.betweenBrackets.len);
			FREE(start[i].variable.betweenBrackets.data);
		}
		else if (start[i].token == CallArgs) {
			CallArgs_t* call = &start[i].callArgs;

			while (call != NULL) {
				if (call->extraAction == ActionExtraArrayIndex) {
					Function_t* f = call->extra;
					exitFunction(f->operations.data, f->operations.count);
					vecFree(f->operations);
					FREE(f);
				}
				else if (call->extraAction == ActionExtraMemberName) {
					FREE(call->extra);
				}
				else if (call->extraAction == ActionExtraCallArgs) {
					Function_t* f = call->extra;
					exitFunction(f->operations.data, f->operations.count);
					vecFree(f->operations);
					FREE(f);
				}

				CallArgs_t* nextCall = call->next;
				if (call != &start[i].callArgs) {
					FREE(call);
				}
					
				call = nextCall;
			}
		}
	}
}

void exitStaticVars(Vector_t* v) {
	vecForEach(Variable_t*, staticVar, v) {
		if (staticVar->variableType == FunctionClass) {
			if (!staticVar->function.builtIn) {
				exitFunction(staticVar->function.function.operations.data, staticVar->function.function.operations.count);
				vecFree(staticVar->function.function.operations);
			}
		}
		else {
			freeVariableInternal(staticVar);
		}
	}
}