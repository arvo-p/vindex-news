# Vindex

![Capture](Images/capture.PNG)

**Vindex** is my minimalist, keyboard-driven terminal feed reader. It features a fast C-based client (`vindex` / `vindex.exe`) with tabbed browsing and a C++ feed fetcher (`vindexServer` / `vindexServer.exe`) that uses `libcurl` and `rapidxml` to parse RSS and Atom feeds.

Runs on Linux and Windows.


---

## Features

- **Keyboard-Centric UI:** Vim-like keyboard navigation (`j`/`k`/`gg`) and Unix-style commands.
- **Tabbed Interface:** Read feeds in multiple tabs, group items by category, or filter them by source domain.
- **Smart Media Launcher:** Clicking entries opens standard URLs in your default browser, while video/YouTube URLs automatically launch directly in **mpv** for seamless terminal viewing.
- **Expanded & Standard Layouts:** Automatically switches between single-column and dual-column layouts based on terminal window resizing.
- **Custom Categorization:** Organize RSS/Atom feeds into custom group tabs (e.g., tech, sports, world).
- **Blacklists:** Filter out feed items dynamically using URL-based and word-based exclusion rules.
- **SQLite-Free Storage:** Fast loading utilizing flat files stored in AppData.

---

## Compilation

### Linux
From the project root, run:
```sh
make
make install
```
This builds `client/vindex` and `server/vindexServer` (requires `gcc`, `g++`, `pthread`, and `libcurl`), and installs both into `~/.local/bin/`.

### Windows
Both components are compiled using GCC/G++ on Windows.

#### 1. Compiling the Client
Navigate to the [client/](file:///C:/Users/gsppe/wezterm/news/project_files/client/) directory and run `compile.bat`:
```cmd
cd client
compile.bat
```
This runs:
```cmd
gcc -c main.c cmd.c colorscheme.c loadfiles.c nav.c tabs.c ui.c -I.
gcc main.o cmd.o colorscheme.o loadfiles.o nav.o tabs.o ui.o -o vindex.exe -lpthread -lshell32
```

#### 2. Compiling the Server
Navigate to the [server/](file:///C:/Users/gsppe/wezterm/news/project_files/server/) directory and run `compile.bat`:
```cmd
cd server
compile.bat
```
This runs:
```cmd
g++ -o vindexServer vindexServer.cpp files_path.cpp blacklist.cpp groups.cpp feed.cpp utils.cpp -IC:\MinGW\include\ -lcurl -lshell32
```

---

## Configuration & Usage

The application reads and saves its settings and data in:
- **Linux:** `~/.local/share/vindex/` (or `$XDG_DATA_HOME/vindex/`)
- **Windows:** `%APPDATA%\vindex\` (usually `C:\Users\<YourUsername>\AppData\Roaming\vindex\`)
*(Note: This folder and its configuration files are generated automatically the first time you run the client).*

### 1. Registering Groups (`user_groups.conf`)
You can use the `:group-register` command in the app, or manually edit `user_groups.conf` to declare custom categories:
```text
@groups
register-group 1 Tech
register-group 2 News
register-group 3 Video
```

### 2. Adding Feeds (`webfeeds.conf`)
You can use the `:feed-add` command in the app, or edit `webfeeds.conf` to list your feeds:
```text
https://news.ycombinator.com/rss [Tech,News]
https://www.youtube.com/feeds/videos.xml?channel_id=UCsBjURrdUwZs3IHrMcna1QG [Video]
```

### 3. Blacklisting & Highlighting Content (`blacklisted.conf` / `highlighted.conf`)
Filter out articles containing specific words or originating from certain URLs in `blacklisted.conf`:
```text
@blacklisted_urls
clickbait-site.com

@blacklisted_words
spoilers
giveaway
```

Highlight watchlist keywords in unread titles by listing one keyword per line (case-insensitive) in `highlighted.conf`:
```text
CVE
kernel
RISC-V
```

### 4. Colorscheme (`colorschemes/latenight`)
Customize colors and display options in `<data_dir>/colorschemes/latenight` (e.g., `~/.local/share/vindex/colorschemes/latenight` on Linux or `%APPDATA%\vindex\colorschemes\latenight` on Windows). Integer color values use xterm-256 palette indices (`0`–`255`), and background colors accept `-1` for the terminal's default/transparent background:

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `ui.background.color` | int | `233` | Global UI background color (`-1` for terminal default) |
| `ui.entries.color` | int | `250` | Unread entry title foreground color |
| `ui.seen.color` | int | `239` | Seen entry title foreground color |
| `ui.url.color` | int | `244` | URL domain foreground color |
| `ui.url.path.color` | int | `239` | URL path/query foreground color |
| `ui.urlprefix.color` | int | `182` | URL prefix symbol foreground color |
| `ui.urlprefix.char` | char | `@` | Character shown before each URL |
| `ui.walls.color` | int | `138` | Box border and separator color |
| `ui.entries_nav.color` | int | `138` | Entry index counter (`current\total`) color |
| `ui.zebra.enabled` | int | `1` | Enable (`1`) or disable (`0`) title background striping |
| `ui.zebra.even.color` | int | `233` | Even-row title background color (`-1` for default) |
| `ui.zebra.odd.color` | int | `234` | Odd-row title background color (`-1` for default; `ui.zebra.color` is an alias) |
| `ui.zebra.dot.enabled` | int | `0` | Enable (`1`) or disable (`0`) alternating `·` marker before titles |
| `ui.zebra.dot.color` | int | `240` | Alternating `·` marker foreground color |
| `ui.title.sentence_case` | int | `1` | Convert Title Case / ALL-CAPS titles to sentence case on display (`1` or `0`) |
| `ui.title.prefix.color` | int | `138` | Color for short unread prefixes before `": "` (`-1` to disable) |
| `ui.title.prefix.maxlen` | int | `24` | Max character length for `": "` title prefix highlighting |
| `ui.title.highlight.color` | int | `180` | Keyword watchlist highlight color in unread titles (`-1` to disable) |
| `ui.search.color` | int | `222` | Search match highlight color in `/` search tabs (`-1` to disable) |
| `ui.media.video.color` | int | `174` | Video URL prefix (`▶`) color (`-1` to use default `@` prefix) |
| `ui.media.pdf.color` | int | `109` | PDF URL prefix (`◈`) color (`-1` to use default `@` prefix) |
| `ui.bookmark.color` | int | `180` | Left-gutter bookmark flag (`⚑`) foreground color |
| `ui.date_separators.enabled` | int | `1` | Show (`1`) or hide (`0`) relative date separator lines (`One Day ago`, `One week ago`, etc.) |
| `ui.date_separators.color` | int | `138` | Date separator text foreground color |

Example `latenight` file:
```ini
@theme:Late Night

ui.urlprefix.char=@

ui.background.color=233
ui.entries.color=250
ui.url.color=244
ui.url.path.color=239
ui.seen.color=239

ui.entries_nav.color=138
ui.walls.color=138
ui.urlprefix.color=182

ui.zebra.enabled=1
ui.zebra.even.color=233
ui.zebra.odd.color=234
ui.zebra.dot.enabled=0
ui.zebra.dot.color=240
ui.title.sentence_case=1
ui.title.prefix.color=138
ui.title.prefix.maxlen=24
ui.title.highlight.color=180
ui.search.color=222
ui.media.video.color=174
ui.media.pdf.color=109
ui.bookmark.color=180
ui.date_separators.enabled=1
ui.date_separators.color=138
```

### 5. Fetching Feeds
Run the parser executable to fetch the newest articles:
```sh
./server/vindexServer   # or vindexServer.exe on Windows
```
This will parse the feeds, check them against the blacklist, sort them by publication date, and append them to `entries.dat`.
*To test a local feed configuration file:* `vindexServer test example.atom`.

### 6. Launching the Reader
Run `vindex` (or `vindex.exe`) to open the terminal reader:
```sh
./client/vindex         # or vindex.exe on Windows
```

---

## Keyboard Controls & Navigation

### List Navigation
- <kbd>j</kbd> — Move selection down by 1 item.
- <kbd>k</kbd> — Move selection up by 1 item.
- <kbd>g</kbd> then <kbd>g</kbd> — Jump to the top of the list.
- <kbd>Page Up</kbd> — Page up by screen height.
- <kbd>Page Down</kbd> — Page down by screen height.

### Tab Controls
- <kbd>h</kbd> — Switch to the **previous** tab.
- <kbd>l</kbd> — Switch to the **next** tab.
- <kbd>u</kbd> — Open a new tab filtering by the highlighted entry's domain (URL base).
- <kbd>d</kbd> — Close the current tab (the base "Main" tab cannot be closed).

### Execution and Mode Keys
- <kbd>Enter</kbd> — Open highlighted entry.
  - Video URLs (e.g., YouTube, MP4) launch in **mpv**.
  - All other links launch in the default browser.
  - The entry is marked as seen in `int.dat`.
- <kbd>r</kbd> — Reload and refresh list from the database.
- <kbd>/</kbd> — Search entries in the current tab (opens matching results in a new tab).
- <kbd>:</kbd> — Enter **Command Mode** (see below).
- <kbd>Q</kbd> — Save and quit the reader safely.

---

## Command Mode

Press <kbd>:</kbd> while viewing the feed to open the prompt. The command line features a powerful autocomplete engine; press <kbd>Tab</kbd> to cycle through suggestions. Quotes (`" "`) can be used to pass arguments containing spaces.

### Group Management
- `:group-open <group_name>` (Alias: `:g`) — Opens a new tab containing only articles from the requested group category.
- `:group-register <name>` (Alias: `:greg`) — Registers a new category group.
- `:group-rename <old_name> <new_name>` (Alias: `:gren`) — Renames a group globally.
- `:group-remove <name>` (Alias: `:grem`) — Deletes a group globally.

### Feed Management
- `:feeds` (Alias: `:f`) — Opens the **Webfeeds** table tab to view all your registered feeds, aliases, and assigned groups.
- `:feed-add <url>` (Alias: `:fa`) — Adds a new feed to your list.
- `:feed-remove <url>` (Alias: `:fr`) — Removes a feed from your list.
- `:feed-group <url> <group_name>` (Alias: `:fg`) — Assigns a category group to a feed.
- `:feed-ungroup <url> <group_name>` (Alias: `:fu`) — Removes a category group from a feed.
- `:feed-alias <url_or_alias> <alias_name>` (Alias: `:fal`) — Gives a feed an easy-to-read name (e.g. `"De Nieuwe Wereld"`). Omit the second argument to remove the alias.

*(Note: When viewing the `:feeds` tab, all Feed Management commands are context-aware. You can omit the `<url>` argument and the command will automatically apply to the feed currently highlighted by your cursor!)*

### Highlight Management
- `:highlight-add <word>` (Alias: `:ha`) — Adds a keyword to `highlighted.conf` and highlights matches in unread titles immediately.
- `:highlight-remove <word>` (Alias: `:hr`) — Removes a keyword from `highlighted.conf` (supports `Tab` autocomplete).

### Command Keys
- <kbd>Tab</kbd> — Autocompletes URLs, Aliases, and Group names dynamically.
- <kbd>Up</kbd> / <kbd>Down</kbd> — Navigate through autocomplete suggestions.
- <kbd>Ctrl+V</kbd> / <kbd>Right Click</kbd> — Paste long URLs directly into the prompt.
- <kbd>Backspace</kbd> — Erase character. Exits command mode if the buffer is empty.
- <kbd>Enter</kbd> — Run command.

---

