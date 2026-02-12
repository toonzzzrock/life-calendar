#include "calendar.hpp"
#include "config.hpp"
#include "diary.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace ftxui;

int main(int argc, char *argv[]) {
  bool check_today = false;
  bool check_yesterday = false;
  bool open_if_missing_today = false;
  bool open_if_missing_yesterday = false;
  std::string config_path;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: life-calendar [config_path] [options]\n"
                << "Options:\n"
                << "  -h, --help                        Show this help message\n"
                << "  --check-today                     Check if today's diary exists and exit\n"
                << "  --check-yesterday                 Check if yesterday's diary exists and exit\n"
                << "  --open-if-today-missing           Open TUI only if today's diary is missing\n"
                << "  --open-if-yesterday-missing       Open TUI only if yesterday's diary is missing\n";
      return 0;
    } else if (arg == "--check-today") {
      check_today = true;
    } else if (arg == "--check-yesterday") {
      check_yesterday = true;
    } else if (arg == "--open-if-today-missing") {
      open_if_missing_today = true;
    } else if (arg == "--open-if-yesterday-missing") {
      open_if_missing_yesterday = true;
    } else if (config_path.empty() && arg[0] != '-') {
      config_path = arg;
    }
  }

  // Load config
  Config config;
  try {
    config = load_config(config_path);
  } catch (const std::exception &e) {
    std::cerr << "Error loading configuration: " << e.what() << "\n";
    return 1;
  }

  if (check_today || check_yesterday) {
    int y, m, d;
    if (check_today) {
      get_today(y, m, d);
    } else {
      get_yesterday(y, m, d);
    }
    bool exists = diary_exists(y, m, d, config.diary_dir);
    std::cout << (exists ? "true" : "false") << std::endl;
    return 0;
  }

  if (open_if_missing_today || open_if_missing_yesterday) {
    int y, m, d;
    if (open_if_missing_today) {
      get_today(y, m, d);
    } else {
      get_yesterday(y, m, d);
    }
    if (diary_exists(y, m, d, config.diary_dir)) {
      return 0;
    }
  }

  auto screen = ScreenInteractive::Fullscreen();

  CalendarHandle cal_handle;

  cal_handle = MakeLifeCalendarApp(config, [&](int year, int month, int day) {
    // Suspend the TUI, open the editor, then resume
    screen.WithRestoredIO([&] {
      open_diary(year, month, day, config.editor, config.diary_dir,
                 config.diary_template);
    })();

    // After editor closes, refresh diary status markers
    cal_handle.RefreshDiaryStatus();
  });

  // Wrap with CatchEvent for quit keys
  auto main_component = CatchEvent(cal_handle.component, [&](Event event) {
    if (event == Event::Character('q') || event == Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  std::atomic<bool> running{true};
  std::thread ticker([&] {
    using namespace std::chrono_literals;
    while (running.load()) {
      std::this_thread::sleep_for(1s);
      screen.PostEvent(Event::Custom);
    }
  });

  screen.Loop(main_component);

  running.store(false);
  if (ticker.joinable()) {
    ticker.join();
  }

  return 0;
}
