#include "diary.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

std::string get_diary_path(int year, int month, int day,
                           const std::string &diary_dir) {
  std::ostringstream oss;
  oss << diary_dir << "/" << year << "/" << year << "-" << std::setfill('0')
      << std::setw(2) << month << "-" << std::setfill('0') << std::setw(2)
      << day << ".md";
  return oss.str();
}

bool diary_exists(int year, int month, int day, const std::string &diary_dir) {
  return fs::exists(get_diary_path(year, month, day, diary_dir));
}

static std::string read_file(const std::string &path) {
  std::ifstream ifs(path);
  if (!ifs) {
    return "";
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

static void replace_all(std::string &text, const std::string &from,
                        const std::string &to) {
  if (from.empty()) {
    return;
  }
  std::size_t start = 0;
  while ((start = text.find(from, start)) != std::string::npos) {
    text.replace(start, from.size(), to);
    start += to.size();
  }
}

static std::string apply_template(const std::string &templ, int year, int month,
                                  int day) {
  std::ostringstream date_oss;
  date_oss << year << "-" << std::setfill('0') << std::setw(2) << month << "-"
           << std::setfill('0') << std::setw(2) << day;
  std::string out = templ;
  replace_all(out, "{date}", date_oss.str());
  replace_all(out, "{year}", std::to_string(year));
  replace_all(out, "{month}", std::to_string(month));
  replace_all(out, "{day}", std::to_string(day));
  return out;
}

void open_diary(int year, int month, int day, const std::string &editor,
                const std::string &diary_dir,
                const std::string &diary_template) {
  std::string path = get_diary_path(year, month, day, diary_dir);

  // Create parent directories
  fs::create_directories(fs::path(path).parent_path());

  // If file doesn't exist, create it with a header or template
  if (!fs::exists(path)) {
    std::ofstream ofs(path);
    std::string templ = diary_template.empty() ? "" : read_file(diary_template);
    if (!templ.empty()) {
      ofs << apply_template(templ, year, month, day);
      if (templ.back() != '\n') {
        ofs << "\n";
      }
    } else {
      ofs << "# Diary - " << year << "-" << std::setfill('0') << std::setw(2)
          << month << "-" << std::setfill('0') << std::setw(2) << day << "\n\n";
    }
    ofs.close();
  }

  // Launch editor in the current terminal instance.
  // We rely on ftxui's WithRestoredIO to have restored the terminal state.
  std::string cmd = editor + " \"" + path + "\"";
  (void)std::system(cmd.c_str());
}
