#pragma once

#include <string>

// Get the diary file path for a given date
// Format: diary_dir/YYYY/YYYY-MM-DD.md
std::string get_diary_path(int year, int month, int day,
                           const std::string &diary_dir);

// Check if a diary entry exists for the given date
bool diary_exists(int year, int month, int day, const std::string &diary_dir);

// Open the diary file in the configured editor.
// Creates the file and parent directories if they don't exist.
// This function blocks until the editor is closed.
void open_diary(int year, int month, int day, const std::string &editor,
                const std::string &diary_dir,
                const std::string &diary_template);
