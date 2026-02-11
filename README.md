# Life Calendar

A terminal-based **Life Calendar** TUI that visualizes your entire life as a grid of monthly squares — from birth to expected death date. Select a month and open day-by-day diary entries in your preferred editor.

## Features

- 📅 **Visual life grid** — each square = one month of your life
- 🎨 **Color-coded** — past, current, future, and full-diary months
- 🖱️ **Mouse + keyboard** — click or navigate with hjkl/arrows, Enter to edit
- 📝 **Day-by-day diary** — open notes for past or current dates only
- 🧩 **Three-panel layout** — life grid, month view, countdown
- ⏳ **Countdown clock** — time remaining to the configured end date

## Quick Start

```bash
# Enter dev shell
nix develop --impure

# Build
mkdir -p build && cd build
cmake .. && make -j$(nproc)

# Run (from project root)
cd ..
./build/life-calendar
```

Or pass a custom config path:

```bash
./build/life-calendar /path/to/config.toml
```

## Config

Create `config.toml` in the project directory or `~/.config/life-calendar/config.toml`:

```toml
birth_date = "1990-01-01"
death_date = "2070-01-01"
editor = "nvim"
diary_dir = "~/.life-calendar/diary"
diary_template = "~/.life-calendar/template.md"
```

| Field            | Description                        | Default                  |
| ---------------- | ---------------------------------- | ------------------------ |
| `birth_date`     | Your birth date (YYYY-MM-DD)       | required                 |
| `death_date`     | Expected end date (YYYY-MM-DD)     | required                 |
| `editor`         | Editor command to open diary files | `vi`                     |
| `diary_dir`      | Directory for diary `.md` files    | `~/.life-calendar/diary` |
| `diary_template` | Optional template for new notes    | empty                    |

Template placeholders (used only when creating a new file):

- `{date}` -> `YYYY-MM-DD`
- `{year}` -> year as number
- `{month}` -> month as number
- `{day}` -> day as number

## Keybindings

| Key                    | Action                                    |
| ---------------------- | ----------------------------------------- |
| `h/j/k/l` or arrows    | Navigate months or days (panel-dependent) |
| `Tab`                  | Switch focus between panels               |
| `Enter` or mouse click | Open diary for selected day               |
| `Home/End`             | Jump to first/last month or day           |
| `q` or `Esc`           | Quit                                      |

## NixOS Integration

Add to your system flake:

```nix
{
  inputs.life-calendar.url = "path:/home/you/life-calendar";

  # In your configuration.nix
  environment.systemPackages = [ inputs.life-calendar.packages.${system}.default ];
}
```

## License

MIT
