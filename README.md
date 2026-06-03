<div align="center">

  # Moonhook V4

  [![Release](https://img.shields.io/github/v/release/U-235Consumer/MoonhookV4?t=1)](https://github.com/U-235Consumer/MoonhookV4/releases/latest)
  [![Stars](https://img.shields.io/github/stars/U-235Consumer/MoonhookV4?t=1)](https://github.com/U-235Consumer/MoonhookV4/stargazers)
  [![License](https://img.shields.io/github/license/U-235Consumer/MoonhookV4?t=1)](https://github.com/U-235Consumer/MoonhookV4/blob/main/LICENSE)
</div>

Moonhook V4 is a complete rewrite of [Moonhook V3](https://github.com/U-235Consumer/MoonhookV3) in C++. It retains all of the original Discord utility and automation features while adding a **Luau plugin system** that lets you extend and add new options without touching the core codebase.

## Features

**Webhooks**
- Send messages (plain text)
- Multi-threaded webhook spamming
- Update webhook name and avatar (from file or URL)
- Delete webhooks

**Bots** *(Requires Bot Token and Guild ID)*
- Delete all channels
- Spam create channels
- Delete all roles
- Spam create roles
- Multi-threaded message spam (via webhooks in every channel)
- Ban everyone
- Full server nuke (parallel channel/role deletion & creation + message spam)

**Plugins** *(Luau scripting)*
- Drop `.lua` / `.luau` / `.luac` / `.luauc` files into the `Plugins/` folder
- Register custom options into the main menu, Webhooks panel, or Bots panel
- Full access to the `moonhook` API: HTTP requests, Webhook & Bot objects, console I/O, JSON encode/decode

## Plugin API

Plugins are written in [Luau](https://luau.org/) and have access to the global `moonhook` table:

| Function / Field | Description |
|---|---|
| `moonhook.Option(name, type, fn)` | Register a new menu option. `type`: `moonhook.MAIN_MENU_OPTION`, `WEBHOOKS_OPTION`, or `BOTS_OPTION` |
| `moonhook.Request({Url, Method, Headers, Body})` | Make an HTTP request; returns `{Success, StatusCode, StatusMessage, Headers, Body}` |
| `moonhook.log(text)` | Print a log line to the console |
| `moonhook.error(text)` | Print an error line to the console |
| `moonhook.input(prompt)` | Prompt the user for a string |
| `moonhook.int_input(prompt)` | Prompt the user for an integer |
| `moonhook.wait(ms)` | Sleep for `ms` milliseconds |
| `moonhook.pause()` | Wait for the user to press Enter |
| `moonhook.encode(table)` | Encode a Lua table to a JSON string |
| `moonhook.decode(str)` | Decode a JSON string into a Lua table |
| `moonhook.MAIN_MENU_OPTION` | Option type constant (0) — adds to the main menu |
| `moonhook.WEBHOOKS_OPTION` | Option type constant (1) — adds to the Webhooks panel |
| `moonhook.BOTS_OPTION` | Option type constant (2) — adds to the Bots panel |

Webhook options receive a `ctx` table with `ctx.url` and `ctx.new(url)` to create a `Webhook` object.  
Bot options receive a `ctx` table with `ctx.token`, `ctx.guild_id`, and `ctx.new(token)` to create a `Bot` object.

**Example plugin:**
```lua
moonhook.Option("My Custom Option", moonhook.WEBHOOKS_OPTION, function(ctx)
    local hook = ctx.new(ctx.url)
    moonhook.log("Sending hello...")
    hook:send("Hello from my plugin!")
end)
```

## Building

Requirements: **CMake 3.15+**, a **C++20** compiler (MSVC or GCC/Clang), and an internet connection (dependencies are fetched automatically).

```bash
cmake -B build
cmake --build build --config Release
```

The compiled binary and `libcurl` DLL will be placed in the build output directory.

## Usage

1. Build the project (see above) or download `moonhookv4.exe` from the [GitHub Releases](https://github.com/U-235Consumer/MoonhookV4/releases/latest).
2. *(Optional)* Drop any `.lua` / `.luau` plugin files into a `Plugins/` folder next to the executable.
3. Run the executable.
4. Follow the on-screen prompts to select the Webhooks panel, Bots panel, or any loaded plugin options.

## Credits
- **V4 Rewrite**: Jasper (@U-235Consumer) and Tobias(@Tobobb)
- **V1 and V2**: Tobias (@Tobobb)
