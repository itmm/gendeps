#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(BUFFER_SIZE)
	#define BUFFER_SIZE 64
#endif

typedef struct _node_t {
	struct _node_t *link;
	const char *path_begin;
	const char *path_end;
} node_t;

static node_t *last_node = NULL;

#define ERR(FORMAT, ...) \
	fprintf(stderr, "ERROR (%s:%d): " FORMAT "\n", __FILE__, __LINE__, ## __VA_ARGS__)

#define WARN(FORMAT, ...) \
	fprintf(stderr, "warn (%s:%d): " FORMAT "\n", __FILE__, __LINE__, ## __VA_ARGS__)

static inline node_t *add_path(const char *path) {
	assert(path);

	node_t *node = malloc(sizeof(node_t));
	if (! node) { ERR("no memory for allocating node for \"%s\"", path); return NULL; }

	node->link = last_node;
	last_node = node;
	node->path_begin = path;
	node->path_end = path + strlen(path);
	return node;
}

static inline bool has_suffix(
	const char *str_begin,
	const char *str_end,
	const char *pattern_begin,
	const char *pattern_end
) {
	assert(str_begin);
	assert(str_end >= str_begin);
	assert(pattern_begin);
	assert(pattern_end >= pattern_begin);

	size_t pattern_len = pattern_end - pattern_begin;
	const char *suffix_start = str_end - pattern_len;

	if (suffix_start == str_begin || (suffix_start > str_begin && suffix_start[-1] == '/')) {
		return memcmp(suffix_start, pattern_begin, pattern_len) == 0;
	}
	
	return false;
}

static inline bool parse_file(node_t *node) {
	assert(node);

	printf("%s:", node->path_begin);

	FILE *in = fopen(node->path_begin, "r");
	int ch = getc(in);
	while (ch != EOF) {
		while (ch != EOF && ch <= ' ') { ch = getc(in); } // whitespace at begin of line

		const char *include_pattern = "#include";
		while (*include_pattern && ch == *include_pattern) { ch = getc(in); ++include_pattern; }
		int spaces_counted = 0;
		if (! *include_pattern) {
			while (ch != EOF && ch <= ' ') { ++spaces_counted; ch = getc(in); }
		}
		if (spaces_counted >= 1 && ch == '"') {
			ch = getc(in);

			char path[BUFFER_SIZE];
			char *cur = path;
			const char *end = path + sizeof(path);

			while (ch != EOF && ch != '"' && ch > ' ') {
				if (cur == end) { ERR("\nname longer than %d bytes", BUFFER_SIZE); return false; }
				*cur++ = ch;
				ch = getc(in);
			}
			if (ch != '"') { ERR("invalid include statement"); return false; }

			ch = getc(in);
			int count = 0;
			for (node_t *node = last_node; node; node = node->link) {
				if (has_suffix(node->path_begin, node->path_end, path, cur)) {
					printf(" %s", node->path_begin);
					if (++count == 2) {
						WARN("multiple sources found for \"%*s\"", (int) (cur - path), path);
					}
				}
			}
			if (! count) { 
				WARN("no file for \"%*s\" found", (int) (cur - path), path); 
			}
		}
		while (ch != EOF && ch != '\n' && ch != '\r') { ch = getc(in); } // eat rest of line
	}
	putchar('\n');
	return true;
}

static void print_help() {
	puts(
		"Syntax: gendeps [-h|--help|<file>...]\n\n"
		"generate make compatible dependencies for local #include directives in the passed\n"
		"files."
	);
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		print_help();
	} else {
		for (int i = 1; i < argc; ++i) {
			if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
				print_help();
				exit(0);
			} else if (! add_path(argv[i])) {
				ERR("can't create node for \"%s\"", argv[i]);
				exit(10);
			}
		}
		for (node_t *cur = last_node; cur; cur = cur->link) {
			if (!parse_file(cur)) { 
				ERR("errors while parsing \"%s\"", cur->path_begin);
				exit(10);
			}
		}
	}	
}
