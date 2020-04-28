#include <string.h>
#include "../../mem/heap.h"
#include "../gfx/gfxutils.h"
#include "../emmc/emmc.h"
#include "../../utils/types.h"
#include "../../libs/fatfs/ff.h"
#include "../../utils/sprintf.h"
#include "../../utils/btn.h"
#include "../../gfx/gfx.h"
#include "../../utils/util.h"
#include "../../storage/emummc.h"
#include "parser.h"
#include "../common/common.h"
#include "../fs/fsactions.h"
#include "variables.h"
#include "../utils/utils.h"

static dict_str_int *str_int_table = NULL;
static dict_str_str *str_str_table = NULL;
//static dict_str_loc *str_jmp_table = NULL;

int str_int_add(char *key, int value){
	char *key_local;
	dict_str_int *keyvaluepair;

	utils_copystring(key, &key_local);
	
	keyvaluepair = calloc(1, sizeof(dict_str_int));
	keyvaluepair->key = key_local;
	keyvaluepair->value = value;
	keyvaluepair->next = NULL;

	if (str_int_table == NULL){
		str_int_table = keyvaluepair;
	}
	else {
		dict_str_int *temp;
		temp = str_int_table;
		while (temp != NULL){
			if (!strcmp(temp->key, key_local)){
				free(keyvaluepair);
				free(key_local);
				temp->value = value;
				return 0;
			}

			if (temp->next == NULL){
				temp->next = keyvaluepair;
				return 0;
			}

			temp = temp->next;
		}
	}

	return 0;
}

int str_int_find(char *key, int *out){
	dict_str_int *temp;
	temp = str_int_table;
	while (temp != NULL){
		if (!strcmp(temp->key, key)){
			*out = temp->value;
			return 0;
		}
		temp = temp->next;
	}

	return -1;
}

void str_int_clear(){
	dict_str_int *cur, *next;
	cur = str_int_table;

	while (cur != NULL){
		next = cur->next;
		free(cur->key);
		free(cur);
		cur = next;
	}
	str_int_table = NULL;
}

void str_int_printall(){
	dict_str_int *temp;
	temp = str_int_table;
	while (temp != NULL){
		gfx_printf("%s -> %d\n", temp->key, temp->value);
		temp = temp->next;
	}
}
/*
int str_jmp_add(char *key, u64 value){
	char *key_local;
	dict_str_loc *keyvaluepair;

	//gfx_printf("Adding |%s|\n", key_local);

	utils_copystring(key, &key_local);

	keyvaluepair = calloc(1, sizeof(dict_str_loc));
	keyvaluepair->key = key_local;
	keyvaluepair->value = value;
	keyvaluepair->next = NULL;
	
	if (str_jmp_table == NULL){
		str_jmp_table = keyvaluepair;
	}
	else {
		dict_str_loc *temp;
		temp = str_jmp_table;
		while (temp != NULL){
			if (!strcmp(temp->key, key_local)){
				free(keyvaluepair);
				free(key_local);

				temp->value = value;
				return 0;
			}

			if (temp->next == NULL){
				temp->next = keyvaluepair;
				return 0;
			}

			temp = temp->next;
		}
	}

	return 0;
}

int str_jmp_find(char *key, u64 *out){
	dict_str_loc *temp;
	temp = str_jmp_table;
	//gfx_printf("Searching |%s|\n", key);
	while (temp != NULL){
		if (!strcmp(temp->key, key)){
			//gfx_printf("Key found!\n", temp->value);
			*out = temp->value;
			return 0;
		}
		temp = temp->next;
	}

	//gfx_printf("no key!\n");
	return -1;
}

void str_jmp_clear(){
	dict_str_loc *cur, *next;
	cur = str_jmp_table;

	while (cur != NULL){
		next = cur->next;
		free(cur->key);
		free(cur);
		cur = next;
	}
	str_jmp_table = NULL;
}
*/

int str_str_add(char *key, char *value){
	char *key_local, *value_local;
	dict_str_str *keyvaluepair;
	//gfx_printf("Adding |%s|\n", key_local);
	utils_copystring(value, &value_local);
	utils_copystring(key, &key_local);

	keyvaluepair = calloc(1, sizeof(dict_str_str));
	keyvaluepair->key = key_local;
	keyvaluepair->value = value_local;
	keyvaluepair->next = NULL;
	
	if (str_str_table == NULL){
		str_str_table = keyvaluepair;
	}
	else {
		dict_str_str *temp;
		temp = str_str_table;
		while (temp != NULL){
			if (!strcmp(temp->key, key_local)){
				free(keyvaluepair);
				free(key_local);
				
				free(temp->value);
				temp->value = value_local;
				return 0;
			}

			if (temp->next == NULL){
				temp->next = keyvaluepair;
				return 0;
			}

			temp = temp->next;
		}
	}

	return 0;
}

int str_str_find(char *key, char **out){
	dict_str_str *temp;
	temp = str_str_table;

	while (temp != NULL){
		if (!strcmp(temp->key, key)){
			*out = temp->value;
			return 0;
		}
		temp = temp->next;
	}

	return -1;
}

int str_str_index(int index, char **out){
	dict_str_str *temp;
	temp = str_str_table;

	for (int i = 0; i < index; i++){
		if (temp == NULL)
			return -1;
		temp = temp->next;
	}

	if (temp == NULL)
		return -1;

	*out = temp->value;
	return 0;
}

void str_str_clear(){
	dict_str_str *cur, *next;
	cur = str_str_table;

	while (cur != NULL){
		next = cur->next;
		free(cur->key);
		free(cur->value);
		free(cur);
		cur = next;
	}
	str_str_table = NULL;
}