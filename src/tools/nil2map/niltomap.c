/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: niltomap.cpp
Version: 0.6.7

StaRev is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#include "niltomap.h"
#include "tool_common.h"
#include "tool_help.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 */
enum { 
	NIL_CLI_PATH_COUNT = 2, 
	NIL_COPY_BUFFER_SIZE = 8192 
};

/*
 */
typedef struct {
	double scale;
	double thickness;
	int classic;
} CliOptions;

/*
 */
static int positive_number(const char *const text, double *const value) {
	char *end = NULL;
	errno	  = 0;
	*value	  = strtod(text, &end);
	return end != text && *end == '\0' && !errno && isfinite(*value) && *value > 0 && *value <= FLT_MAX;
}

/*
 */
static void usage(FILE *const output) {
	nod_tools_print_help(output, "NILtoMAP",
		"--scale N : Position scale (default 1).\n"
		"--thickness N : Brush depth in MAP units (default 1).\n"
		"--classic : Use legacy alignment; loses native UVs.\n"
		"--help : Show help.\n"
		"-- : End options.\n"
		"input.nil : Source NIL level geometry.\n"
		"output.map : Destination Quake III MAP file.\n",
		"NILtoMAP --scale 32 --thickness 2 input.nil output.map\n");
}

/*
 */
typedef enum { 
	ARGUMENT_FAILURE, 
	ARGUMENT_READY, 
	ARGUMENT_HELP 
} ArgumentResult;

/*
 */
static ArgumentResult parse_arguments(const int argument_count, char *const arguments[],
									  const char *paths[NIL_CLI_PATH_COUNT], CliOptions *const options) {
	int path_count			 = 0;
	int should_parse_options = 1;
	for(int argument_index = 1; argument_index < argument_count; ++argument_index) {
		const char *const argument = arguments[argument_index];
		if(should_parse_options && strcmp(argument, "--") == 0) {
			should_parse_options = 0;
		} else if(should_parse_options && strcmp(argument, "--help") == 0) {
			usage(stdout);
			return ARGUMENT_HELP;
		} else if(should_parse_options && strcmp(argument, "--classic") == 0) {
			options->classic = 1;
		} else if(should_parse_options && (strcmp(argument, "--scale") == 0 || strcmp(argument, "--thickness") == 0)) {
			double *const value = strcmp(argument, "--scale") == 0 ? &options->scale : &options->thickness;
			if(argument_index + 1 == argument_count || !positive_number(arguments[argument_index + 1], value)) {
				fprintf(stderr, "NILtoMAP: %s requires a finite positive number within float range\n", argument);
				return ARGUMENT_FAILURE;
			}
			++argument_index;
		} else if((should_parse_options && argument[0] == '-') || path_count == NIL_CLI_PATH_COUNT) {
			fprintf(stderr, "NILtoMAP: unexpected argument: %s\n", argument);
			usage(stderr);
			return ARGUMENT_FAILURE;
		} else {
			paths[path_count++] = argument;
		}
	}
	if(path_count != NIL_CLI_PATH_COUNT) {
		usage(stderr);
		return ARGUMENT_FAILURE;
	}
	if(strcmp(paths[0], paths[1]) == 0) {
		fprintf(stderr, "NILtoMAP: input and output paths must differ\n");
		return ARGUMENT_FAILURE;
	}
	return ARGUMENT_READY;
}

/*
 */
// The caller owns any returned allocation and open stream, including on failure.
static int read_input_file(FILE **const input, unsigned char **const data, size_t *const data_size,
						   const char *const path) {
	*input = fopen(path, "rb");
	if(!*input) {
		fprintf(stderr, "NILtoMAP: cannot open input '%s': %s\n", path, strerror(errno));
		return 0;
	}
	long file_size = 0;
	if(fseek(*input, 0, SEEK_END) || (file_size = ftell(*input)) < 0 || fseek(*input, 0, SEEK_SET)) {
		fprintf(stderr, "NILtoMAP: cannot determine input size: %s\n", strerror(errno));
		return 0;
	}
	if((uintmax_t)file_size > SIZE_MAX) {
		fprintf(stderr, "NILtoMAP: input is too large for this platform\n");
		return 0;
	}
	*data_size = (size_t)file_size;
	*data	   = malloc(file_size ? (size_t)file_size : 1);
	if(!*data) {
		fprintf(stderr, "NILtoMAP: cannot allocate %ld bytes for input\n", file_size);
		return 0;
	}
	if(fread(*data, 1, (size_t)file_size, *input) != (size_t)file_size) {
		fprintf(stderr, "NILtoMAP: cannot read complete input\n");
		return 0;
	}
	if(fclose(*input)) {
		*input = NULL;
		fprintf(stderr, "NILtoMAP: cannot close input: %s\n", strerror(errno));
		return 0;
	}
	*input = NULL;
	return 1;
}

/*
 */
// The caller closes the output stream if copying fails before fclose.
static int copy_staged_output(FILE **const output, FILE *const staged, const char *const path) {
	*output = fopen(path, "wb");
	if(!*output) {
		fprintf(stderr, "NILtoMAP: cannot open output '%s': %s\n", path, strerror(errno));
		return 0;
	}
	unsigned char buffer[NIL_COPY_BUFFER_SIZE] = {0};
	size_t bytes_read						   = 0;
	while((bytes_read = fread(buffer, 1, sizeof(buffer), staged)) != 0) {
		if(fwrite(buffer, 1, bytes_read, *output) != bytes_read) {
			fprintf(stderr, "NILtoMAP: cannot write output: %s\n", strerror(errno));
			return 0;
		}
	}
	if(ferror(staged)) {
		fprintf(stderr, "NILtoMAP: cannot read temporary MAP: %s\n", strerror(errno));
		return 0;
	}
	if(fclose(*output)) {
		*output = NULL;
		fprintf(stderr, "NILtoMAP: cannot close output: %s\n", strerror(errno));
		return 0;
	}
	*output = NULL;
	return 1;
}

/*
 */
int main(const int argument_count, char **const arguments) {
	CliOptions options = {.scale = 1.0, .thickness = 1.0};
	const char *paths[NIL_CLI_PATH_COUNT] = {NULL, NULL};
	const ArgumentResult argument_result  = parse_arguments(argument_count, arguments, paths, &options);
	if(argument_result == ARGUMENT_HELP) {
		return EXIT_SUCCESS;
	}
	if(argument_result != ARGUMENT_READY) {
		return EXIT_FAILURE;
	}
	int result	 = EXIT_FAILURE;
	FILE *input	 = NULL;
	FILE *staged = NULL;
	FILE *output = NULL;
	// This function owns the input allocation and all three streams.
	unsigned char *data = NULL;
	size_t data_size	= 0;
	if(!read_input_file(&input, &data, &data_size, paths[0])) {
		goto cleanup;
	}
	staged = tmpfile();
	if(!staged) {
		fprintf(stderr, "NILtoMAP: cannot create temporary MAP: %s\n", strerror(errno));
		goto cleanup;
	}
	size_t brushes = 0;
	size_t degenerate = 0;
	if(!(options.classic ? niltomap_write_stats : niltomap_write_uv_stats)(
			data, data_size, staged, options.scale, options.thickness, NULL,
							 &brushes, &degenerate)) {
		if(ferror(staged)) {
			fprintf(stderr, "NILtoMAP: error writing temporary MAP: %s\n", strerror(errno));
		}
		goto cleanup;
	}
	if(fflush(staged) || fseek(staged, 0, SEEK_SET)) {
		fprintf(stderr, "NILtoMAP: cannot rewind temporary MAP: %s\n", strerror(errno));
		goto cleanup;
	}
	if(!copy_staged_output(&output, staged, paths[1])) {
		goto cleanup;
	}
	printf("NILtoMAP: wrote %zu brushes to '%s'; skipped %zu degenerate triangles\n", brushes, paths[1],
		   degenerate);
	result = EXIT_SUCCESS;
cleanup:
	if(input && fclose(input)) {
		fprintf(stderr, "NILtoMAP: error closing input: %s\n", strerror(errno));
	}
	if(output && fclose(output)) {
		fprintf(stderr, "NILtoMAP: error closing output: %s\n", strerror(errno));
	}
	if(staged && fclose(staged)) {
		fprintf(stderr, "NILtoMAP: error closing temporary MAP: %s\n", strerror(errno));
		result = EXIT_FAILURE;
	}
	free(data);
	return result;
}
