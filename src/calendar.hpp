#pragma once

#include <functional>
#include <memory>
#include <string>

struct Config; // forward declare

// Date info for a day
struct DayInfo {
  int year;
  int month;
  int day;
  bool is_past;
  bool is_today;
  bool has_diary;
};

#include <ftxui/component/component.hpp>

// Opaque handle to refresh diary status after editor closes
class CalendarGridBase;

struct CalendarHandle {
  ftxui::Component component;
  std::shared_ptr<CalendarGridBase> impl;

  void RefreshDiaryStatus();
};

// Create the FTXUI life calendar component.
// on_select_day is called when a day is selected.
CalendarHandle MakeLifeCalendarApp(
    const Config &config,
    std::function<void(int year, int month, int day)> on_select_day);
