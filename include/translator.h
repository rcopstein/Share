#pragma once

// Loads the path tree from a file
uint8_t translator_load(const char* source_file);

// Find a path in the path tree
char* translator_find(char* path);
