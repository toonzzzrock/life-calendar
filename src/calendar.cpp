#include "calendar.hpp"
#include "config.hpp"
#include "diary.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace ftxui;

namespace {
struct MonthInfo {
  int year = 0;
  int month = 0;
  bool is_past = false;
  bool is_current = false;
  bool is_future = false;
  bool has_full_diary = false;
};

struct LayoutInfo {
  int width = 0;
  int height = 0;

  int left_x = 0;
  int left_y = 0;
  int left_w = 0;
  int left_h = 0;

  int right_x = 0;
  int right_y = 0;
  int right_w = 0;
  int right_h = 0;

  int right_top_h = 0;
  int right_bottom_h = 0;

  int left_grid_x = 0;
  int left_grid_y = 0;
  int left_grid_w = 0;
  int left_grid_h = 0;
  int left_cols = 0;
  int left_rows = 0;
  int left_months_per_cell = 1;
  int left_cell_count = 0;

  int month_grid_x = 0;
  int month_grid_y = 0;
};

static bool is_leap(int y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m) {
  static const int dm[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && is_leap(y)) {
    return 29;
  }
  return dm[m];
}

static int weekday_index(int y, int m, int d) {
  using namespace std::chrono;
  auto sys_day = sys_days{year{y} / month{static_cast<unsigned>(m)} /
                          day{static_cast<unsigned>(d)}};
  return int(weekday{sys_day}.c_encoding()); // 0=Sun
}

static std::string format_date(int y, int m, int d) {
  std::ostringstream oss;
  oss << y << "-" << std::setfill('0') << std::setw(2) << m << "-"
      << std::setfill('0') << std::setw(2) << d;
  return oss.str();
}

static std::string month_name(int m) {
  static const char *names[] = {"",    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  if (m < 1 || m > 12) {
    return "";
  }
  return names[m];
}

static std::vector<std::string> preview_diary_lines(const std::string &path,
                                                    int max_lines) {
  std::vector<std::string> lines;
  std::ifstream ifs(path);
  if (!ifs) {
    return lines;
  }
  std::string line;
  while (max_lines-- > 0 && std::getline(ifs, line)) {
    if (line.size() > 64) {
      line = line.substr(0, 61) + "...";
    }
    lines.push_back(line);
  }
  return lines;
}
} // namespace

class CalendarGridBase : public ComponentBase {
public:
  CalendarGridBase(
      const Config &config,
      std::function<void(int year, int month, int day)> on_select_day)
      : config_(config), on_select_day_(std::move(on_select_day)) {
    BuildMonths();
    RefreshDiaryStatus();
  }

  Element OnRender() override {
    UpdateLayout();

    auto left =
        HighlightPanel(RenderLifeCalendar(), active_panel_ == Panel::Life);
    auto right_top =
        HighlightPanel(RenderMonthCalendar(), active_panel_ == Panel::Month);
    auto right_bottom = RenderCountdown();

    auto right = vbox({
        right_top | size(HEIGHT, EQUAL, layout_.right_top_h),
        right_bottom | size(HEIGHT, EQUAL, layout_.right_bottom_h),
    });

    return hbox({
        left | size(WIDTH, EQUAL, layout_.left_w),
        right | size(WIDTH, EQUAL, layout_.right_w),
    });
  }

  bool OnEvent(Event event) override {
    UpdateLayout();

    if (event == Event::Custom) {
      return true;
    }

    if (event.is_mouse()) {
      auto &mouse = event.mouse();
      HandleMouse(mouse);
      return true;
    }

    if (event == Event::Tab) {
      active_panel_ =
          (active_panel_ == Panel::Life) ? Panel::Month : Panel::Life;
      return true;
    }

    if (active_panel_ == Panel::Life) {
      return HandleLifeKeys(event);
    }
    return HandleMonthKeys(event);
  }

  bool Focusable() const override { return true; }

  void RefreshDiaryStatus() {
    int today_days = TodayDays();
    for (auto &m : months_) {
      int num_days = days_in_month(m.year, m.month);
      int month_end = days_from_epoch(m.year, m.month, num_days);
      if (month_end > today_days) {
        m.has_full_diary = false;
        continue;
      }
      bool all_days = true;
      for (int d = 1; d <= num_days; ++d) {
        if (!diary_exists(m.year, m.month, d, config_.diary_dir)) {
          all_days = false;
          break;
        }
      }
      m.has_full_diary = all_days;
    }
  }

private:
  enum class Panel { Life, Month };

  Element HighlightPanel(Element panel, bool active) {
    if (!active) {
      return panel;
    }
    return panel | color(Color::YellowLight);
  }

  void BuildMonths() {
    months_.clear();

    int today_y = 0, today_m = 0, today_d = 0;
    get_today(today_y, today_m, today_d);
    int today_days = days_from_epoch(today_y, today_m, today_d);

    int y = config_.birth_year;
    int m = config_.birth_month;
    while (y < config_.death_year ||
           (y == config_.death_year && m <= config_.death_month)) {
      MonthInfo info;
      info.year = y;
      info.month = m;

      int num_days = days_in_month(y, m);
      int start_days = days_from_epoch(y, m, 1);
      int end_days = days_from_epoch(y, m, num_days);

      info.is_past = end_days < today_days;
      info.is_current = start_days <= today_days && today_days <= end_days;
      info.is_future = start_days > today_days;
      info.has_full_diary = false;

      months_.push_back(info);

      ++m;
      if (m > 12) {
        m = 1;
        ++y;
      }
    }

    focused_month_ = 0;
    for (std::size_t i = 0; i < months_.size(); ++i) {
      if (months_[i].is_current) {
        focused_month_ = static_cast<int>(i);
        break;
      }
    }

    int ty = 0, tm = 0, td = 0;
    get_today(ty, tm, td);
    selected_day_ = td;
    ClampSelectedDay();
  }

  int TodayDays() const {
    int y = 0, m = 0, d = 0;
    get_today(y, m, d);
    return days_from_epoch(y, m, d);
  }

  void ClampSelectedDay() {
    const auto &m = months_[focused_month_];
    int num_days = days_in_month(m.year, m.month);
    selected_day_ = std::clamp(selected_day_, 1, num_days);
  }

  void SetFocusedMonth(int idx) {
    if (months_.empty()) {
      return;
    }
    idx = std::clamp(idx, 0, static_cast<int>(months_.size() - 1));
    if (idx == focused_month_) {
      return;
    }
    focused_month_ = idx;

    int ty = 0, tm = 0, td = 0;
    get_today(ty, tm, td);
    const auto &m = months_[focused_month_];
    if (m.year == ty && m.month == tm) {
      selected_day_ = td;
    }
    ClampSelectedDay();
  }

  void MoveMonth(int delta) { SetFocusedMonth(focused_month_ + delta); }

  bool HandleLifeKeys(const Event &event) {
    if (layout_.left_cols <= 0) {
      return false;
    }
    if (event == Event::ArrowLeft || event == Event::Character('h')) {
      MoveMonth(-1);
      return true;
    }
    if (event == Event::ArrowRight || event == Event::Character('l')) {
      MoveMonth(1);
      return true;
    }
    if (event == Event::ArrowUp || event == Event::Character('k')) {
      MoveMonth(-layout_.left_cols);
      return true;
    }
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      MoveMonth(layout_.left_cols);
      return true;
    }
    if (event == Event::Home) {
      SetFocusedMonth(0);
      return true;
    }
    if (event == Event::End) {
      SetFocusedMonth(static_cast<int>(months_.size() - 1));
      return true;
    }
    if (event == Event::Return) {
      ActivateSelectedDay();
      return true;
    }
    return false;
  }

  bool HandleMonthKeys(const Event &event) {
    if (event == Event::ArrowLeft || event == Event::Character('h')) {
      selected_day_ = std::max(1, selected_day_ - 1);
      return true;
    }
    if (event == Event::ArrowRight || event == Event::Character('l')) {
      selected_day_ += 1;
      ClampSelectedDay();
      return true;
    }
    if (event == Event::ArrowUp || event == Event::Character('k')) {
      selected_day_ = std::max(1, selected_day_ - 7);
      return true;
    }
    if (event == Event::ArrowDown || event == Event::Character('j')) {
      selected_day_ += 7;
      ClampSelectedDay();
      return true;
    }
    if (event == Event::Home) {
      selected_day_ = 1;
      return true;
    }
    if (event == Event::End) {
      ClampSelectedDay();
      selected_day_ = days_in_month(months_[focused_month_].year,
                                    months_[focused_month_].month);
      return true;
    }
    if (event == Event::Return) {
      ActivateSelectedDay();
      return true;
    }
    return false;
  }

  void ActivateSelectedDay() {
    if (months_.empty()) {
      return;
    }
    const auto &m = months_[focused_month_];
    int day = selected_day_;
    int target_days = days_from_epoch(m.year, m.month, day);
    int today_days = TodayDays();
    if (target_days > today_days) {
      status_message_ = "Cannot create a diary entry in the future.";
      return;
    }
    status_message_.clear();
    if (on_select_day_) {
      on_select_day_(m.year, m.month, day);
    }
  }

  void HandleMouse(const Mouse &mouse) {
    if (mouse.button != Mouse::Left || mouse.motion != Mouse::Released) {
      return;
    }

    int x = mouse.x;
    int y = mouse.y;

    if (x >= layout_.left_grid_x &&
        x < layout_.left_grid_x + layout_.left_grid_w &&
        y >= layout_.left_grid_y &&
        y < layout_.left_grid_y + layout_.left_grid_h) {
      int col = x - layout_.left_grid_x;
      int row = y - layout_.left_grid_y;
      int cell = row * layout_.left_cols + col;
      if (cell >= 0 && cell < layout_.left_cell_count) {
        int month_idx = cell * layout_.left_months_per_cell;
        SetFocusedMonth(month_idx);
        active_panel_ = Panel::Life;
      }
      return;
    }

    if (x >= layout_.month_grid_x && x < layout_.month_grid_x + 21 &&
        y >= layout_.month_grid_y && y < layout_.month_grid_y + 6) {
      int col = (x - layout_.month_grid_x) / 3;
      int row = y - layout_.month_grid_y;
      int first_wd = weekday_index(months_[focused_month_].year,
                                   months_[focused_month_].month, 1);
      int day = row * 7 + col - first_wd + 1;
      int num_days = days_in_month(months_[focused_month_].year,
                                   months_[focused_month_].month);
      if (day >= 1 && day <= num_days) {
        selected_day_ = day;
        active_panel_ = Panel::Month;
        ActivateSelectedDay();
      }
      return;
    }
  }

  void UpdateLayout() {
    auto term = Terminal::Size();
    layout_.width = std::max(1, term.dimx);
    layout_.height = std::max(1, term.dimy);

    layout_.left_w = std::max(26, (layout_.width * 2) / 3);
    layout_.right_w = std::max(20, layout_.width - layout_.left_w);
    if (layout_.left_w + layout_.right_w > layout_.width) {
      layout_.left_w = layout_.width - layout_.right_w;
    }

    layout_.left_x = 0;
    layout_.left_y = 0;
    layout_.left_h = layout_.height;

    layout_.right_x = layout_.left_w;
    layout_.right_y = 0;
    layout_.right_h = layout_.height;

    layout_.right_bottom_h = 3;
    layout_.right_top_h = std::max(1, layout_.right_h - layout_.right_bottom_h);

    layout_.left_grid_x = layout_.left_x + 1;
    layout_.left_grid_y = layout_.left_y + 1;
    layout_.left_grid_w = std::max(1, layout_.left_w - 2);
    layout_.left_grid_h = std::max(1, layout_.left_h - 6);

    layout_.left_cols = std::max(1, layout_.left_grid_w);
    layout_.left_rows = std::max(1, layout_.left_grid_h);

    int total_months = static_cast<int>(months_.size());
    int cell_count = layout_.left_cols * layout_.left_rows;
    layout_.left_months_per_cell =
        std::max(1, (total_months + cell_count - 1) / cell_count);
    layout_.left_cell_count =
        (total_months + layout_.left_months_per_cell - 1) /
        layout_.left_months_per_cell;

    if (focused_month_ < 0 && total_months > 0) {
      focused_month_ = 0;
    }

    layout_.month_grid_x = layout_.right_x + 1;
    layout_.month_grid_y = layout_.right_y + 2;
  }

  Element RenderLifeCalendar() {
    Elements rows;
    rows.reserve(layout_.left_rows);

    int total_months = static_cast<int>(months_.size());

    for (int r = 0; r < layout_.left_rows; ++r) {
      Elements cols;
      cols.reserve(layout_.left_cols);
      for (int c = 0; c < layout_.left_cols; ++c) {
        int cell = r * layout_.left_cols + c;
        if (cell >= layout_.left_cell_count) {
          cols.push_back(text(" "));
          continue;
        }
        int start_idx = cell * layout_.left_months_per_cell;
        int end_idx =
            std::min(start_idx + layout_.left_months_per_cell, total_months);

        bool has_current = false;
        bool has_past = false;
        bool has_future = false;
        bool all_full = true;
        for (int i = start_idx; i < end_idx; ++i) {
          const auto &m = months_[i];
          has_current = has_current || m.is_current;
          has_past = has_past || m.is_past;
          has_future = has_future || m.is_future;
          all_full = all_full && m.has_full_diary;
        }

        Color fg = Color::GrayDark;
        if (has_current) {
          fg = Color::Yellow;
        } else if (all_full && has_past) {
          fg = Color::Green;
        } else if (has_past) {
          fg = Color::RGB(90, 140, 220);
        } else if (has_future) {
          fg = Color::GrayDark;
        }

        auto elem = text("#") | color(fg);

        int focus_cell = focused_month_ / layout_.left_months_per_cell;
        if (cell == focus_cell) {
          if (active_panel_ == Panel::Life) {
            elem = elem | bold | inverted;
          } else {
            elem = elem | underlined | dim;
          }
        }
        cols.push_back(elem);
      }
      rows.push_back(hbox(std::move(cols)));
    }

    std::string title = "Life Calendar";
    const auto &m = months_[focused_month_];
    std::ostringstream info;
    info << month_name(m.month) << " " << m.year << "  "
         << (m.has_full_diary ? "Full month diary" : "Month incomplete");

    auto legend = hbox({
        text("#") | color(Color::RGB(90, 140, 220)),
        text(" Past  ") | color(Color::GrayLight),
        text("#") | color(Color::Green),
        text(" Full  ") | color(Color::GrayLight),
        text("#") | color(Color::Yellow),
        text(" Current  ") | color(Color::GrayLight),
        text("#") | color(Color::GrayDark),
        text(" Future") | color(Color::GrayLight),
    });

    auto status = vbox({
        text(info.str()) | color(Color::White),
        text(status_message_.empty() ? "" : status_message_) |
            color(Color::RedLight),
        legend,
    });

    return window(text(title) | bold | color(Color::Cyan),
                  vbox({
                      vbox(std::move(rows)) | flex,
                      separator() | color(Color::GrayDark),
                      status,
                  }));
  }

  Element RenderMonthCalendar() {
    const auto &m = months_[focused_month_];
    int num_days = days_in_month(m.year, m.month);
    int first_wd = weekday_index(m.year, m.month, 1);

    Elements lines;
    lines.push_back(hbox({
        text("Su ") | color(Color::GrayLight),
        text("Mo ") | color(Color::GrayLight),
        text("Tu ") | color(Color::GrayLight),
        text("We ") | color(Color::GrayLight),
        text("Th ") | color(Color::GrayLight),
        text("Fr ") | color(Color::GrayLight),
        text("Sa ") | color(Color::GrayLight),
    }));

    for (int row = 0; row < 6; ++row) {
      Elements cols;
      for (int col = 0; col < 7; ++col) {
        int cell = row * 7 + col;
        int day_num = cell - first_wd + 1;
        if (day_num < 1 || day_num > num_days) {
          cols.push_back(text("   "));
          continue;
        }

        std::ostringstream oss;
        oss << std::setw(2) << day_num << " ";
        auto elem = text(oss.str());

        int ty = 0, tm = 0, td = 0;
        get_today(ty, tm, td);
        bool is_today = (m.year == ty && m.month == tm && day_num == td);
        if (is_today) {
          elem = elem | color(Color::Yellow) | bold;
        }

        int target_days = days_from_epoch(m.year, m.month, day_num);
        if (target_days > TodayDays()) {
          elem = elem | color(Color::GrayDark);
        } else if (diary_exists(m.year, m.month, day_num, config_.diary_dir)) {
          elem = elem | color(Color::Green);
        }

        if (day_num == selected_day_) {
          if (active_panel_ == Panel::Month) {
            elem = elem | inverted | bold;
          } else {
            elem = elem | underlined | dim;
          }
        }

        cols.push_back(elem);
      }
      lines.push_back(hbox(std::move(cols)));
    }

    std::ostringstream title;
    title << month_name(m.month) << " " << m.year;

    auto preview_lines = preview_diary_lines(
        get_diary_path(m.year, m.month, selected_day_, config_.diary_dir), 100);
    Elements preview_elems;
    if (preview_lines.empty()) {
      preview_elems.push_back(text("No note yet.") | color(Color::GrayDark));
    } else {
      for (const auto &line : preview_lines) {
        preview_elems.push_back(text(line));
      }
    }

    return window(text(title.str()) | bold | color(Color::Cyan),
                  vbox({
                      vbox(std::move(lines)),
                      separator() | color(Color::GrayDark),
                      vbox(std::move(preview_elems)) | flex,
                  }));
  }

  Element RenderCountdown() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto target = sys_days{year{config_.death_year} /
                           month{static_cast<unsigned>(config_.death_month)} /
                           day{static_cast<unsigned>(config_.death_day)}} +
                  days{1};

    auto diff = target - now;
    if (diff < seconds(0)) {
      diff = seconds(0);
    }
    auto total_seconds = duration_cast<seconds>(diff).count();
    long long days = total_seconds / 86400;
    long long hours = (total_seconds % 86400) / 3600;
    long long minutes = (total_seconds % 3600) / 60;
    long long seconds_left = total_seconds % 60;

    std::ostringstream time_oss;
    time_oss << days << "d " << std::setfill('0') << std::setw(2) << hours
             << ":" << std::setw(2) << minutes << ":" << std::setw(2)
             << seconds_left;

    std::ostringstream title;
    title << "Countdown to " << config_.death_date_str;

    return window(text(title.str()) | bold | color(Color::Cyan),
                  text(time_oss.str()) | bold | color(Color::White));
  }

  Config config_;
  std::function<void(int year, int month, int day)> on_select_day_;
  std::vector<MonthInfo> months_;
  LayoutInfo layout_;
  Panel active_panel_ = Panel::Life;
  int focused_month_ = 0;
  int selected_day_ = 1;
  std::string status_message_;
};

void CalendarHandle::RefreshDiaryStatus() {
  if (impl) {
    impl->RefreshDiaryStatus();
  }
}

CalendarHandle MakeLifeCalendarApp(
    const Config &config,
    std::function<void(int year, int month, int day)> on_select_day) {
  CalendarHandle handle;
  handle.impl =
      std::make_shared<CalendarGridBase>(config, std::move(on_select_day));
  handle.component = handle.impl;
  return handle;
}
