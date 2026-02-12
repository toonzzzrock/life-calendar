#include "config.hpp"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
// toml++ removed as we move to NixOS options/env vars

namespace fs = std::filesystem;

std::string expand_home(const std::string &path) {
  if (path.empty() || path[0] != '~')
    return path;
  const char *home = std::getenv("HOME");
  if (!home)
    return path;
  return std::string(home) + path.substr(1);
}

bool parse_date(std::string_view s, int &y, int &m, int &d) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-')
    return false;

  auto parse_part = [](std::string_view part, int &out) {
    auto res = std::from_chars(part.data(), part.data() + part.size(), out);
    return res.ec == std::errc{} && res.ptr == part.data() + part.size();
  };

  if (!parse_part(s.substr(0, 4), y))
    return false;
  if (!parse_part(s.substr(5, 2), m))
    return false;
  if (!parse_part(s.substr(8, 2), d))
    return false;

  return m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

static bool is_leap(int y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m) {
  static const int dm[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && is_leap(y))
    return 29;
  return dm[m];
}

int days_from_epoch(int y, int m, int d) {
  // Days from 0000-01-01 (a simple calculation for comparison purposes)
  int total = 0;
  for (int i = 1; i < y; ++i) {
    total += is_leap(i) ? 366 : 365;
  }
  for (int i = 1; i < m; ++i) {
    total += days_in_month(y, i);
  }
  total += d;
  return total;
}

void get_today(int &y, int &m, int &d) {
  using namespace std::chrono;
  auto today = floor<days>(system_clock::now());
  year_month_day ymd{today};
  y = int(ymd.year());
  m = unsigned(ymd.month());
  d = unsigned(ymd.day());
}

Config load_config(const std::string &explicit_path) {
  std::vector<std::string> search_paths;

  if (!explicit_path.empty()) {
    search_paths.push_back(explicit_path);
  }

  search_paths.push_back("./config.toml");

  const char *xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg) {
    search_paths.push_back(std::string(xdg) + "/life-calendar/config.toml");
  }

  const char *home = std::getenv("HOME");
  if (home) {
    search_paths.push_back(std::string(home) +
                           "/.config/life-calendar/config.toml");
  }

  std::string found_path;
  for (auto &p : search_paths) {
    if (fs::exists(p)) {
      found_path = p;
      break;
    }
  }

  if (found_path.empty()) {
    // No config file found, we'll rely on environment variables and defaults.
  } else {
    // If we wanted to keep TOML support, we could still parse it here, 
    // but the user wants to move to NixOS options.
  }

  Config cfg;
  
  auto get_env = [](const char* name, const std::string& def) {
    const char* val = std::getenv(name);
    return val ? std::string(val) : def;
  };

  cfg.birth_date_str = get_env("LIFE_CALENDAR_BIRTH_DATE", "2000-01-01");
  cfg.death_date_str = get_env("LIFE_CALENDAR_DEATH_DATE", "2080-01-01");
  cfg.editor = get_env("LIFE_CALENDAR_EDITOR", get_env("EDITOR", "vi"));
  cfg.diary_dir = get_env("LIFE_CALENDAR_DIARY_DIR", "~/.life-calendar/diary");
  cfg.diary_template = get_env("LIFE_CALENDAR_DIARY_TEMPLATE", "~/.life-calendar/template.md");

  // Expand ~ in paths
  cfg.diary_dir = expand_home(cfg.diary_dir);
  cfg.diary_template = expand_home(cfg.diary_template);

  // If the user's template doesn't exist, try to create it from the Nix store fallback or a default header
  if (!fs::exists(cfg.diary_template)) {
    try {
      fs::create_directories(fs::path(cfg.diary_template).parent_path());
      
      std::string fallback_template = get_env("LIFE_CALENDAR_DIARY_TEMPLATE_FALLBACK", "");
      if (!fallback_template.empty() && fs::exists(fallback_template)) {
        fs::copy_file(fallback_template, cfg.diary_template);
      } else {
        std::ofstream ofs(cfg.diary_template);
        ofs << "# Diary Entry: {date}\n\n## What happened today?\n- \n\n## Mood / Energy\n- \n\n## Reflection\n- \n";
      }
    } catch (...) {
      // Ignore errors in template creation, we'll handle missing template in open_diary
    }
  }

  // Parse dates
  if (!parse_date(cfg.birth_date_str, cfg.birth_year, cfg.birth_month,
                  cfg.birth_day)) {
    throw std::runtime_error("Invalid birth_date format: " +
                             cfg.birth_date_str);
  }
  if (!parse_date(cfg.death_date_str, cfg.death_year, cfg.death_month,
                  cfg.death_day)) {
    throw std::runtime_error("Invalid death_date format: " +
                             cfg.death_date_str);
  }

  return cfg;
}
