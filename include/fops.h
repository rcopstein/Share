#pragma once

#include <memory.h>

int fops_make_dir(const char* dirname);

int fops_exists_dir(const char* dirname);

int fops_remove_dir(const char* dirname);

int fops_append_line(const char* filename, const char* line);

int fops_remove_line(const char* filename, const char* prefix);

int fops_read_line(const char* filename, const char* prefix, char* buffer, size_t size);

int fops_update_line(const char* filename, const char* prefix, char* (*funct)(char *line));
