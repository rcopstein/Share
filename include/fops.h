#pragma once

#include <stdio.h>

int fops_make_dir(const char* dirname);

FILE* fops_make_file(const char* filename);

int fops_append_line(const char* filename, const char* line);

int fops_delete_line(const char* filename, const char* line);

int fops_delete_line_starts_with(const char* filename, const char* line);
