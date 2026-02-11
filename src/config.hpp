#pragma once

#include <string>
#include <string_view>

struct Config {
  std::string birth_date_str;
  std::string death_date_str;
  std::string editor;
  std::string diary_dir;
  std::string diary_template;

  // Parsed dates (days since epoch)
  int birth_year{}, birth_month{}, birth_day{};
  int death_year{}, death_month{}, death_day{};
};

// Load config from file. Searches in order:
// 1. ./config.toml
// 2. $XDG_CONFIG_HOME/life-calendar/config.toml
// 3. ~/.config/life-calendar/config.toml
[[nodiscard]] Config load_config(const std::string &explicit_path = "");

// Expand ~ to home directory
[[nodiscard]] std::string expand_home(const std::string &path);

// Parse "YYYY-MM-DD" into year/month/day
[[nodiscard]] bool parse_date(std::string_view s, int &y, int &m, int &d);

// Convert y/m/d to days since a fixed epoch (0000-01-01)
[[nodiscard]] int days_from_epoch(int y, int m, int d);

// Get today's date components
void get_today(int &y, int &m, int &d);
