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
  // Load config
  Config config;
  try {
    std::string config_path;
    if (argc > 1) {
      config_path = argv[1];
    }
    config = load_config(config_path);
  } catch (const std::exception &e) {
    std::cerr << "Error loading configuration: " << e.what() << "\n";
    return 1;
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
