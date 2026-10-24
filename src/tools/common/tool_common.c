/*
===========================================================================
Copyright (C) 2002 - 2005, Nihilistic Software, Inc.
Copyright (C) 2002 - 2014, Blizzard Entertainment, Inc.
Copyright (C) 2024 - 2026, OpenNod SDK & StaRev contributors

StaRev - Starcraft Ghost reversed
This file is part of the StaRev source code.

File: tool_common.c
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

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "tool_common.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

/*
 */
enum { 
	PATH_CAPACITY = 2048 
};

/*
 */
static const uint32_t crc32_polynomial = 0xedb88320u;

/*
 */
static int io_error(const char *message) {
	fprintf(stderr, "NODscene: %s\n", message);
	return 0;
}

/*
 */
uint32_t crc32(const Blob *blob) {
	uint32_t checksum = UINT32_MAX;
	for(size_t i = 0; i < blob->size; ++i) {
		checksum ^= blob->data[i];
		for(unsigned j = 0; j < 8; ++j) {
			checksum = (checksum >> 1) ^ (crc32_polynomial & (0u - (checksum & 1u)));
		}
	}

	return ~checksum;
}

/*
 */
int read_stream(FILE *file, Blob *blob) {
	long file_size = 0;
	if(fseek(file, 0, SEEK_END) || (file_size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
		return io_error("cannot seek input stream");
	}

	if((uintmax_t)file_size > SIZE_MAX) {
		return io_error("input exceeds addressable size");
	}

	blob->size = (size_t)file_size;
	blob->data = malloc(blob->size ? blob->size : 1);
	if(!blob->data) {
		return io_error("out of memory reading input");
	}

	if(fread(blob->data, 1, blob->size, file) != blob->size) {
		return io_error("cannot read complete input");
	}

	return 1;
}

/*
 */
int load(const char *path, Blob *blob) {
	FILE *file = fopen(path, "rb");
	if(!file) {
		fprintf(stderr, "NODscene: cannot open '%s': %s\n", path, strerror(errno));
		return 0;
	}

	const int is_successful = read_stream(file, blob);
	if(fclose(file)) {
		return io_error("cannot close input file");
	}

	return is_successful;
}

/*
 */
int save(const char *path, const Blob *blob) {
	FILE *file = fopen(path, "wb");
	if(!file) {
		fprintf(stderr, "NODscene: cannot open output '%s': %s\n", path, strerror(errno));
		return 0;
	}

	int is_successful = fwrite(blob->data, 1, blob->size, file) == blob->size;
	if(fclose(file)) {
		is_successful = 0;
	}

	return is_successful ? 1 : io_error("output write failed; output may be partial");
}

/*
 */
int ci(const char *left, const char *right) {
	while(*left && *right) {
		const int left_character  = tolower((unsigned char)*left++);
		const int right_character = tolower((unsigned char)*right++);
		if(left_character != right_character) {
			return left_character - right_character;
		}
	}

	return (unsigned char)*left - (unsigned char)*right;
}

/*
 */
int path_join(char *output, size_t capacity, const char *base_path, const char *relative_path) {
	const int length = snprintf(output, capacity, "%s/%s", base_path, relative_path);
#ifdef _WIN32
	for(size_t i = 0; length >= 0 && i < capacity && output[i]; ++i) {
		if(output[i] == '/') {
			output[i] = '\\';
		}
	}
#endif
	return length >= 0 && (size_t)length < capacity ? 1 : io_error("path is too long");
}

/*
 */
int save_generated(const char *path, const Blob *data) {
	FILE *file = fopen(path, "rb");
	if(!file) {
		if(errno == ENOENT) {
			return save(path, data);
		}

		fprintf(stderr, "NODscene: cannot inspect preview file '%s': %s\n", path, strerror(errno));
		return 0;
	}

	Blob existing	  = {NULL, 0};
	int is_successful = read_stream(file, &existing);
	if(fclose(file)) {
		is_successful = io_error("cannot close existing preview file");
	}

	if(is_successful && (existing.size != data->size || memcmp(existing.data, data->data, data->size))) {
		fprintf(stderr, "NODscene: refusing to overwrite different preview file '%s'\n", path);
		is_successful = 0;
	}

	free(existing.data);
	return is_successful;
}

/*
 */
#ifdef _WIN32
int visit_files(const char *directory, int recursive, int optional, FileVisitor visitor, void *context) {
	int is_successful	   = 1;
	const DWORD attributes = GetFileAttributesA(directory);
	if(attributes == INVALID_FILE_ATTRIBUTES && optional &&
	   (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND)) {
		return 1;
	}

	if(attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		return io_error("cannot inspect asset directory");
	}

	char pattern[PATH_CAPACITY] = {0};
	if(!path_join(pattern, sizeof(pattern), directory, "*")) {
		return 0;
	}

	WIN32_FIND_DATAA entry = {0};
	const HANDLE search	   = FindFirstFileA(pattern, &entry);
	if(search == INVALID_HANDLE_VALUE) {
		if(GetLastError() == ERROR_FILE_NOT_FOUND) {
			return 1;
		}

		return io_error("cannot enumerate asset directory");
	}

	do {
		if(!strcmp(entry.cFileName, ".") || !strcmp(entry.cFileName, "..")) {
			continue;
		}

		char path[PATH_CAPACITY] = {0};
		if(!path_join(path, sizeof(path), directory, entry.cFileName)) {
			is_successful = 0;
			break;
		}

		if(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if(recursive && (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
				is_successful = io_error("refusing a linked asset directory");
				break;
			}

			if(recursive && !visit_files(path, recursive, 0, visitor, context)) {
				is_successful = 0;
				break;
			}
		} else if(!visitor(context, path, entry.cFileName)) {
			is_successful = 0;
			break;
		}
	} while(FindNextFileA(search, &entry));
	if(is_successful && GetLastError() != ERROR_NO_MORE_FILES) {
		is_successful = io_error("asset directory enumeration failed");
	}

	if(!FindClose(search)) {
		is_successful = io_error("cannot close directory enumeration");
	}

	return is_successful;
}
#else

/*
 */
int visit_files(const char *directory, int recursive, int optional, FileVisitor visitor, void *context) {
	int is_successful	  = 1;
	DIR *directory_stream = opendir(directory);
	if(!directory_stream) {
		if(optional && errno == ENOENT) {
			return 1;
		}

		return io_error("cannot enumerate asset directory");
	}

	struct dirent *entry = NULL;
	errno				 = 0;
	while((entry = readdir(directory_stream)) != NULL) {
		if(!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
			continue;
		}

		char path[PATH_CAPACITY] = {0};
		struct stat status		 = {0};
		if(!path_join(path, sizeof(path), directory, entry->d_name) || lstat(path, &status)) {
			is_successful = io_error("cannot inspect asset catalog entry");
			break;
		}

		if(S_ISLNK(status.st_mode)) {
			is_successful = io_error("refusing a linked asset catalog entry");
			break;
		}

		if(S_ISDIR(status.st_mode)) {
			if(recursive && !visit_files(path, recursive, 0, visitor, context)) {
				is_successful = 0;
				break;
			}
		} else if(S_ISREG(status.st_mode) && !visitor(context, path, entry->d_name)) {
			is_successful = 0;
			break;
		}

		errno = 0;
	}

	if(is_successful && errno) {
		is_successful = io_error("asset directory enumeration failed");
	}

	if(closedir(directory_stream)) {
		is_successful = io_error("cannot close asset directory");
	}

	return is_successful;
}
#endif
