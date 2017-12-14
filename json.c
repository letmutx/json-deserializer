#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define NIL 0
#define NUM 1
#define ARR 2
#define STR 3
#define BOL 4
#define OBJ 5

struct array {
	int len;			/* len of the array */
	int *type;			/* types of each element in elements */
	struct value **elements;	/* pointer to elements in the array */
};

struct value {
	int type;			/* type of the value, types are defined above */
	union {
		struct object *obj;	/* object */
		struct array *arr;	/* array */
		char *string;		/* string */
		double *number;		/* all numbers are double, no distinction between ints, longs */
		int *boolean;		/* true = 1, false = 0 */
	} element;
};

struct member {
	char *key;			/* key */
	struct value *val;		/* value present in this member */
	struct member *next;		/* for use in collision in hashtable */
};

struct object {
	struct member *members[128]; 	/* object is hash table of members */
};

struct array *get_array(char **buffer);
struct object *get_object(char **buffer);
void *get_null(char **buffer);
double *get_number(char **buffer);
int *get_boolean(char **buffer);
char *get_string(char **buffer);


int insert(struct object *obj, struct member *mem);
int hash(char *key);

struct object *construct_json_object(char *buffer);
struct object **construct_json_objects(FILE *data, int *len);

void queries(struct object **objs, int len);
	

int main(int argc, char *argv[]) {
	int c, len;
	long size, i = 0;
	char *buffer, in = 0;
	char *name;
	if (argc < 2) {
		puts("Usage: ./program json file");
		exit(1);
	}
	name = argv[1];
	FILE *data = fopen(name, "r");

	if (!data) {
		puts("File not found!");
		exit(1);
	}

	struct object **objs = construct_json_objects(data, &len);

	fclose(data);
	
	// queries only work for the given object structure
	queries(objs, len);

	return 0;
}

// simple hash function for insertion into bucket
int hash(char *key) {
	int h = 0;
	while (*key) {
		h += *key;
		key++;
	}
	return h % 128;
}

void error(char *estring, char *cur) {
	puts(estring);
	if (cur) {
		puts(cur);
	}
	exit(1);
}

void *get_null(char **buffer) {
	char *cur = *buffer;
	static char null[] = "null";
	if (!strncmp(cur, null, sizeof(null) - 1)) {
		cur += sizeof(null) - 1;
	}
	else {
		error("Error! - not null value", cur);
	}
	*buffer = cur;
	return NULL;
}

double *get_number(char **buffer) {
	double *d = malloc(sizeof(*d));
	char *cur = *buffer;
	char *endptr;
	*d = strtod(cur, &endptr);
	if (!endptr) {
		error("Error! - number not found", cur);
	}
	*buffer = endptr;
	return d;
}

int *get_boolean(char **buffer) {
	int *val = malloc(sizeof(*val));
	char *cur = *buffer;
	static char truth[] = "true", fals[] = "false";
	if (!strncmp(cur, truth, sizeof(truth) - 1)) {
		*val = 1;
		cur += sizeof(truth) - 1;
	}
	else if (!strncmp(cur, fals, sizeof(fals) - 1)) {
		*val = 0;
		cur += sizeof(fals) - 1;
	}
	else {
		error("Error! not true or false", cur);
	}
	*buffer = cur;
	return val;
}

char *get_string(char **buffer) {
	int i = 0;
	int size = 100;
	char *cur = *buffer;
	void *temp;
	cur++;
	char *string = malloc(100);
	if (!string) {
		puts("malloc returned null in get_string");
		exit(2);
	}
	while (*cur && *cur != '"') {
		if (i == size) {
			size *= 2;
			temp = realloc(string, size);
			if (!temp) {
				puts("realloc returned null in get_string");
				exit(2);
			}
			else {
				string = temp;
			}
		}
		if (*cur == '\\') {
			cur++;
			continue;
		}
		string[i++] = *cur++;
	}

	if (!*cur) {
		error("JSON string cannot end with a string", cur);
	}

	string[i++] = 0;
	*buffer = cur + 1;

	temp = realloc(string, i);

	if (!temp) {
		puts("realloc returned null in get_string");
		exit(2);
	}
	else {
		string = temp;
	}

	return string;
}

struct value *get_value(char **buffer) {
	struct value *val = malloc(sizeof(*val));
	char *cur = *buffer;
	switch (*cur) {
		case '\\':
			cur++;
		case '"':
			val->type = STR;
			val->element.string = get_string(&cur);
			break;
		case '-':
		case '0'...'9':
			val->type = NUM;
			val->element.number = get_number(&cur);
			break;
		case 't':
			val->type = BOL;
			val->element.boolean = get_boolean(&cur);
			break;
		case 'f':
			val->type = BOL;
			val->element.boolean = get_boolean(&cur);
			break;
		case 'n':
			val->type = NIL;
		case '[':
			val->type = ARR;
			val->element.arr = get_array(&cur);
			break;
		case '{':
			val->type = OBJ;
			val->element.obj = get_object(&cur);
			break;
		default:
			error("Error! - no value found", cur);
	}
	*buffer = cur;
	return val;
}


/* inserts mem in to bucket with index hsh in obj, 
 * if bucket is not empty, uses a linked list and inserts
 * new element always at the beginning
 */
int insert(struct object *obj, struct member *mem) {
	char *key = mem->key;
	// the index of bucket to insert this member into
	int hsh = hash(key), c;

	if (!obj->members[hsh]) {
		obj->members[hsh] = mem;
	}
	else {
		// TODO: change to sorted order
		// same keys will be a problem
		mem->next = obj->members[hsh];
		obj->members[hsh] = mem->next;
	}
	return 1;
}

struct object *get_object(char **buffer) {
	struct object *obj;
	struct member *mem;
	char *cur = *buffer;
	if (*cur != '{') {
		error("JSON string doesn't start with '{'", cur);
	}
	cur++;
	obj = malloc(sizeof(*obj));
	if (!obj) {
		puts("malloc returned null in get_object");
		exit(2);
	}
	while (*cur && *cur != '}') {
		mem = malloc(sizeof(*mem));
		mem->key = get_string(&cur);
		mem->next = NULL;
		if (*cur != ':') {
			error("Error! no ':' found", cur);
		}
		else {
			cur++;
		}
		mem->val = get_value(&cur);
		insert(obj, mem);
		if (*cur != ',' && *cur != '}') {
			error("Error! ',' or '}' not found", cur);
		}
		if (*cur == ',') {
			cur++;
		}
	}
	// if not end of the json string, skip the '}'
	*buffer = *cur ? cur + 1 : cur;
	return obj;
}

struct array *get_array(char **buffer) {
	struct array *arr;
	char *cur = *buffer;
	struct value *element;
	void *temp;
	int size = 16;
	if (*cur != '[') {
		return NULL;
	}
	cur++;
	arr = malloc(sizeof(*arr));
	arr->len = 0;
	arr->type = malloc(sizeof(*(arr->type)) * size);
	if (!arr->type) {
		puts("malloc returned null in get_array");
		exit(2);
	}
	arr->elements = malloc(sizeof(*(arr->elements)) * size);
	if (!arr->elements) {
		puts("malloc retured null in get_array");
		exit(2);
	}
	while (*cur && *cur != ']') {
		// resize the array elements dynamically
		if (arr->len == size) {
			size *= 2;
			temp = realloc(arr->type, size * sizeof(*(arr->type)));
			if (!temp) {
				puts("realloc returned null in get_array");
				exit(2);
			}
			else {
				arr->type = temp;
			}
			temp = realloc(arr->elements, size * sizeof(*(arr->elements)));
			if (!temp) {
				puts("realloc returned null in get_array");
				exit(2);
			}
			else {
				arr->elements = temp;
			}
		}
		element = get_value(&cur);
		arr->type[arr->len] = element->type;
		arr->elements[arr->len] = element;
		arr->len++;
		if (*cur == ']')
			continue;
		else if (*cur != ',') {
			error("Error! invalid character", cur);
		}
		// skip the ','
		else
			cur++;
	}
	if (!*cur) {
		error("JSON string ended with array", cur);
	}
	temp = realloc(arr->type, arr->len * sizeof(*(arr->type)));
	if (arr->len > 0 && !temp) {
		puts("realloc returned null in get_array");
		exit(2);
	}
	else {
		arr->type = temp;
	}
	temp = realloc(arr->elements, arr->len * sizeof(*(arr->elements)));
	if (arr->len > 0 && !temp) {
		puts("realloc returned null in get_array");
		exit(2);
	}
	else {
		arr->elements = temp;
	}
	// fix the dangling pointers
	if (arr->len == 0) {
		arr->type = NULL;
		arr->elements = NULL;
	}
	*buffer = cur + 1;
	return arr;
}

struct object **construct_json_objects(FILE *data, int *len) {
	int i = 0, in = 0, j = 0, c;
	char *buffer = malloc(100);
	*len = 8;
	struct object **obj = malloc(sizeof(*obj) * (*len));
	if (!obj) {
		error("Error! malloc returned null in construct json objects", NULL);
	}
	
	while ((c = fgetc(data)) != EOF) {
		if (!in && isspace(c)) {
			if (c == '\n') {
				j = buffer[j] = 0;
				obj[i++] = construct_json_object(buffer);
			}
			continue;
		}
		if (c == '"') {
			in = !in;
		}
		buffer[j++] = c;
	}
	return obj;
}

struct object *construct_json_object(char *buffer) {
	return get_object(&buffer);
}

void *get(struct object *obj, char *key) {
	int hsh = hash(key);
	struct member *val = obj->members[hsh];
	double *d = malloc(sizeof(*d) * 2);
	while (val != NULL) {
		if (!strcmp(val->key, "firstName") || !strcmp(val->key, "lastName")) {
			return val->val->element.string;
		}
		else if (!strcmp(val->key, "phones")) {
			d[0] = *(val->val->element.arr->elements[0]->element.number);
			d[1] = *(val->val->element.arr->elements[1]->element.number);
			return d;
		}
		else if (!strcmp(val->key, "age")) {
			d[0] = *(val->val->element.number);
			return d;
		}
		val = val->next;
	}
	return NULL;
}

void print_answer(struct object **objs, int len, char *key, void *val) {
	void *ans = NULL;
	char *v;
	double *dv, *a;
	if (!strcmp(key, "firstName")) {
		v = val;
		for (int i = 0; i < len; i++) {
			ans = get(objs[i], key);
			if (ans && !strcmp(ans, v)) {
				ans = get(objs[i], "firstName");
				if (ans) {
					printf("Answer = %s\n", (char *)ans);
				}
				ans = NULL;
			}
		}
	}
	else if (!strcmp(key, "lastName")) {
		v = val;
		for (int i = 0; i < len; i++) {
			ans = get(objs[i], key);
			if (!strcmp(ans, v)) {
				ans = get(objs[i], "firstName");
				if (ans) {
					printf("Answer = %s\n", (char *)ans);
				}
				ans = NULL;
			}
		}
	}
	else if (!strcmp(key, "phones")) {
		dv = val;
		for (int i = 0; i < len; i++) {
			a = get(objs[i], "phones");
			if (a && a[0] == *dv || a[1] == *dv) {
				ans = get(objs[i], "firstName");
				if (ans) {
					printf("Answer = %s\n", (char *)ans);
				}
			}
		}
	}
	else if (!strcmp(key, "age")) {
		dv = val;
		for (int i = 0; i < len; i++) {
			ans = get(objs[i], "age");
			if (!ans) {
				continue;
			}
			if (*(double *)ans == *dv) {
				ans = get(objs[i], "firstName");
				if (ans) {
					printf("Answer = %s\n", (char *)ans);
				}
			}
			ans = NULL;
		}
	}
	return;
}

void queries(struct object **objs, int len) {
	int choice;
	double value;
	char searchstring[100];

	for (;;) {
		printf("Query on: 1) firstName 2) lastName 3)phones 4) age 5) Exit\n");
		do {
			printf("Enter your choice: ");
			scanf("%d", &choice);
		} while (choice > 5 || choice < 1);

		switch(choice) {
			case 1:
				printf("Enter string: ");
				scanf("%s", searchstring);
				print_answer(objs, len, "firstName", searchstring);
				break;
			case 2:
				printf("Enter string: ");
				scanf("%s", searchstring);
				print_answer(objs, len, "lastName", searchstring);
				break;
			case 3:
				printf("Enter value: ");
				scanf("%lf", &value);
				print_answer(objs, len, "phones", &value);
				break;
			case 4:
				printf("Enter value: ");
				scanf("%lf", &value);
				print_answer(objs, len, "age", &value);
				break;
			case 5:
				return;
		}
	}
}

