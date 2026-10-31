# Telegram Dead Man's Switch

A small C++ program that **publishes your file(s) to a Telegram channel if you stop checking in**.

Every day you send a secret password to your own Telegram bot. That tells the program "I'm still here". If you forget for too long (24 hours by default), the bot posts your files to your channel.

> **Responsible use.** This tool is meant for publishing *your own* material: a will, a backup of your work, educational documents, journalism insurance, and so on. Do not use it to harass people, leak other people's private data, or share anything illegal. You are responsible for what you publish, and you must follow Telegram's Terms of Service and the laws where you live.

---

## Read this first: the important things

| # | Important | What to do |
|---|---|---|
| 1 | **Telegram is blocked in your country?** A VPN in your browser is **not enough**. This program needs a **proxy** setting. | Read [Telegram is blocked in your country? Use a proxy](#telegram-is-blocked-in-your-country-use-a-proxy) (5 minutes). |
| 2 | **The computer must stay on and online.** If it is off at the deadline, nothing is published. | Use an always-on computer, or a small server in a country where Telegram is not blocked. See [Step 12](#step-12-keep-it-running-all-the-time). |
| 3 | **Test first** with a private test channel, never with your real channel. | Follow [Step 11](#step-11-do-a-full-rehearsal-important). |
| 4 | **Never share or upload** your bot token, your password, or `config.json`. | They are already in `.gitignore`. If a token leaks, send `/revoke` to @BotFather. |
| 5 | The bot must be an **administrator** of the channel with **Post Messages** on. | [Step 3](#step-3-make-the-bot-an-administrator-of-the-channel). |
| 6 | You must press **START** in your bot once, or it cannot write to you. | [Step 1](#step-1-create-your-bot). |
| 7 | This is **not a 100% guarantee**. | Read [Security notes and honest limits](#security-notes-and-honest-limits). |

---

## Table of contents

**Start here**
- [Read this first: the important things](#read-this-first-the-important-things)
- [The idea, in plain words](#1-the-idea-in-plain-words)
- [Words you will see (glossary)](#2-words-you-will-see-glossary)
- [What you need](#3-what-you-need)

**Setup, step by step**
- [Step 1: Create your bot](#step-1-create-your-bot)
- [Step 2: Create your channel](#step-2-create-your-channel)
- [Step 3: Make the bot an administrator](#step-3-make-the-bot-an-administrator-of-the-channel)
- [Step 4: Find your channel ID](#step-4-find-your-channel-id)
- [Step 5: Find your own user ID](#step-5-find-your-own-user-id)
- [Step 6: Prepare your file(s)](#step-6-prepare-your-files)
- [Step 7: Install the build tools](#step-7-install-the-build-tools)
- [Step 8: Download and build the program](#step-8-download-and-build-the-program) (Linux, macOS, Windows, Code::Blocks)
- [Step 9: Write your config.json](#step-9-write-your-configjson)
- **[Extra step if Telegram is blocked for you: use a proxy](#telegram-is-blocked-in-your-country-use-a-proxy)**
- [Step 10: Run the self-check](#step-10-run-the-self-check)
- [Step 11: Do a full rehearsal](#step-11-do-a-full-rehearsal-important)
- [Step 12: Keep it running all the time](#step-12-keep-it-running-all-the-time)

**Using it**
- [Using it every day](#using-it-every-day)
- [All bot commands](#all-bot-commands)
- [All settings](#all-settings)

**When something goes wrong**
- [Troubleshooting](#troubleshooting)
- [Security notes and honest limits](#security-notes-and-honest-limits)

**For developers**
- [How the program works inside](#how-the-program-works-inside)
- [License](#license)

---

## 1. The idea, in plain words

Imagine you have a **secret letter**, and you give it to a trusted friend with this rule:

> "Every day I will tell you the secret word. If I ever stop telling you for a whole day, open the letter and show it to everyone."

This program is that friend.

```
          every day
 YOU  ───────────────►  YOUR BOT  (the program running on a computer)
      "my-secret-word"       │
                             │  resets the clock to 24:00:00
                             ▼
                      ┌───────────────┐
                      │  24h  timer   │
                      └───────┬───────┘
                              │ if the clock reaches 00:00:00
                              ▼
                      YOUR CHANNEL gets the file(s)
```

Before the time runs out, the bot also sends **you** a warning message (4 hours before, by default), so you can't forget by accident.

---

## 2. Words you will see (glossary)

| Word | What it means |
|---|---|
| **Telegram bot** | A robot account on Telegram. Programs can control it. Ours is the "friend" holding your letter. |
| **BotFather** | The official Telegram bot that creates other bots. |
| **Token** | The bot's secret key. Whoever has it controls the bot. **Treat it like a password.** |
| **Channel** | A Telegram place where admins post and many people can read. Our files get posted here. |
| **Administrator (admin)** | Someone with extra rights in the channel. The bot must be an admin, or it is not allowed to post. |
| **Channel ID** | The channel's "address": `@name` for public channels, or a number like `-1001234567890` for private ones. |
| **Owner ID (user ID)** | Your personal number on Telegram. The bot only listens to *you*. |
| **Password** | The secret word you send to the bot every day. (Different from the token.) |
| **Config file** | A text file (`config.json`) where you write your settings. |
| **JSON** | The format of the config file. Text goes in `"quotes"`, numbers don't, and lines are separated by commas. |
| **Build / compile** | Turning the source code into a program you can run. |
| **VPS** | A small computer you rent that is always on. Optional, but very useful here. |

---

## 3. What you need

- [ ] A **Telegram account** (on your phone is fine).
- [ ] A **computer that stays on** and connected to the internet. Your own PC works. A cheap always-on computer (a small rented server, or a Raspberry Pi at home) is better. **If the computer is off, nothing can be published**, see [Security notes](#security-notes-and-honest-limits).
- [ ] The **file(s)** you want to publish (each **under 50 MB**, which is Telegram's limit for bots).
- [ ] About **30 to 45 minutes** the first time.

You do **not** need to know C++ to follow this guide. You only copy and paste commands.

---

## Step 1: Create your bot

1. Open Telegram and search for **@BotFather**. Choose the official one (it has a blue check mark).
2. Send: `/newbot`
3. BotFather asks for a **name**. This is what people see, for example `My Safety Net`.
4. BotFather asks for a **username**. It must be unique and **end with `bot`**, for example `my_safety_net_12345_bot`.
5. BotFather replies with a message containing your **token**. It looks like this:

   ```
   123456789:AAHdqTcvCH1vGWJxfSeofSAs0K5PALDsaw
   ```

6. **Copy the token and keep it secret.** Anyone who has it can control your bot. If it ever leaks, send `/revoke` to BotFather to get a new one.
7. **Very important:** open your new bot in Telegram (click the link BotFather gave you) and press the **START** button. A bot is not allowed to write to you first, so without this step it can never warn you.

> Tip: use a bot **only for this project**. Do not reuse a bot that another program also uses.

---

## Step 2: Create your channel

1. In Telegram: menu → **New Channel**.
2. Give it a name and press the checkmark.
3. Choose the type:
   - **Public**: anyone can find it, and it has a link like `t.me/mychannel`. The channel ID is then simply `@mychannel`. Easiest.
   - **Private**: only people with the invite link can join. The channel ID is a number (you will find it in Step 4).

> You can create a **second, private test channel** now. In Step 11 you will use it for a rehearsal, so that nothing real is ever published by mistake.

---

## Step 3: Make the bot an administrator of the channel

Without this, Telegram does not allow the bot to post.

1. Open your channel.
2. Tap the channel name at the top → **Administrators** (on some versions: *Manage channel* → *Administrators*).
3. Tap **Add Administrator**.
4. Search for your bot by its username and select it.
5. Make sure **Post Messages** is switched **ON**. Everything else can be OFF.
6. Save.

(The exact menus look a little different in each Telegram app, but the idea is always the same.)

---

## Step 4: Find your channel ID

**Public channel:** the ID is `@` plus the channel's username, for example `@mychannel`. You are done.

**Private channel:** you need the number.

1. Post any message in the channel (for example "hello").
2. Open this address in your web browser. Replace `PASTE_TOKEN_HERE` with your bot token:

   ```
   https://api.telegram.org/botPASTE_TOKEN_HERE/getUpdates
   ```

   (There is no space and no `<` `>`. It is `bot` followed directly by the token.)
3. You will see a block of text. Look for something like this:

   ```
   "chat":{"id":-1001234567890,"title":"My Test Channel","type":"channel"}
   ```

4. The number after `"id":` is your channel ID, **including the minus sign**: `-1001234567890`.

If the page shows `"result":[]` (empty), post another message in the channel and reload the page. Also make sure the bot really is an administrator (Step 3).

> Do this **before** you start the program. Do not share screenshots of that page, because the address contains your token.

---

## Step 5: Find your own user ID

The program only obeys **you**. It needs your personal Telegram number to know who you are.

1. Open your bot and send it any message, for example `hello`.
2. Open the same address as before in your browser:

   ```
   https://api.telegram.org/botPASTE_TOKEN_HERE/getUpdates
   ```

3. Find your message in the text. It looks like:

   ```
   "from":{"id":123456789,"is_bot":false,"first_name":"You"}
   ```

4. The number after `"id":` is your **owner ID**, for example `123456789`. It is a **positive** number.

---

## Step 6: Prepare your file(s)

1. Put the file(s) you want to publish in a safe folder on the computer that will run the program. Name them simply, like `secret.pdf`.
2. Each file must be **smaller than 50 MB**. If it is bigger, split it or compress it (for example into several `.zip` parts).
3. Any file type works: PDF, ZIP, text, images, and so on.
4. You can also add a **text message** that is posted before the files (for example a short explanation). You will set that in `config.json`.

---

## Step 7: Install the build tools

You need three things: a **C++ compiler**, **CMake**, and two libraries (**libcurl** and **nlohmann/json**).

### Linux (Ubuntu / Debian / Raspberry Pi OS)

```bash
sudo apt update
sudo apt install -y g++ cmake git libcurl4-openssl-dev nlohmann-json3-dev
```

(You need GCC 9 or newer. `g++ --version` shows yours.)

### macOS

Install [Homebrew](https://brew.sh), then:

```bash
brew install cmake curl nlohmann-json
```

### Windows 10 / 11

1. Install **Visual Studio 2022 Community** (free). In the installer choose the workload **"Desktop development with C++"**. This includes CMake.
2. Install **Git** from <https://git-scm.com>.
3. Install the libraries with **vcpkg** (Microsoft's helper). Open **"x64 Native Tools Command Prompt for VS 2022"** and run:

   ```bat
   cd C:\
   git clone https://github.com/microsoft/vcpkg
   cd vcpkg
   bootstrap-vcpkg.bat
   vcpkg install curl:x64-windows nlohmann-json:x64-windows
   ```

   This takes a few minutes. Remember the path `C:\vcpkg`, you will need it in the next step.

---

## Step 8: Download and build the program

### Linux / macOS

```bash
git clone https://github.com/taheriran021/telegram-dead-mans-switch.git
cd telegram-dead-mans-switch
cmake -B build
cmake --build build
```

The finished program is `build/deadswitch`.

(Prefer one single command without CMake? On Linux this also works:
`g++ -std=c++17 -O2 src/deadswitch.cpp -o deadswitch -lcurl`)

### Windows

In the same "x64 Native Tools Command Prompt":

```bat
git clone https://github.com/taheriran021/telegram-dead-mans-switch.git
cd telegram-dead-mans-switch
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

The finished program is `build\Release\deadswitch.exe`.

> If you get an error like "Could not find CURL", the path after `CMAKE_TOOLCHAIN_FILE=` is wrong. It must point to your vcpkg folder.
> If the program says a `.dll` is missing when you start it, copy the `.dll` files from `build\Release\` next to the `.exe` (vcpkg normally does this for you).

### Windows alternative: Code::Blocks (MinGW)

If you prefer Code::Blocks instead of Visual Studio and CMake:

1. Create a new **Console application** (C++) project and replace the content of `main.cpp` with the content of `src/deadswitch.cpp`.
2. **Settings → Compiler → Compiler Flags:** tick **C++17** (`-std=c++17`).
3. Make sure the **libcurl** and **nlohmann/json** include folders and the libcurl libraries are set in **Project → Build options** (Search directories and Linker settings).
4. **Project → Properties → Build targets → Execution working dir:** set it to the folder that contains your `config.json` and your file(s). The program looks for them there, and it will also create `state.json` there.
5. **Project → Set programs' arguments:** type `--check` (two dashes first), press **F9**. Later, empty this box to run the program normally.
6. Keep the black console window **open** while the program runs.

---

## Step 9: Write your config.json

1. In the project folder, **copy** `config.example.json` and name the copy `config.json`.
2. Open `config.json` in any text editor (Notepad is fine) and fill in your values:

```json
{
  "bot_token": "123456789:AAHdqTcvCH1vGWJxfSeofSAs0K5PALDsaw",
  "owner_id": 123456789,
  "channel_id": "@mychannel",
  "password": "purple-Elephant-sings-at-7am",
  "files": ["secret.pdf"],
  "message": "If you can read this, I could not check in. Here is my document.",
  "default_hours": 24,
  "warn_before_hours": 4
}
```

What each line means:

| Setting | What to write |
|---|---|
| `bot_token` | The token from Step 1, **in quotes**. |
| `owner_id` | Your number from Step 5, **without quotes**. |
| `channel_id` | `"@mychannel"` for a public channel, or `"-1001234567890"` for a private one, **in quotes**. |
| `password` | Your secret word. **At least 8 characters, no spaces, must not start with `/`.** Long is better, and use something nobody could guess. Never reuse a real password from elsewhere. |
| `files` | A list of file paths, in quotes, separated by commas: `["a.pdf", "b.zip"]`. |
| `message` | Optional text posted before the files. Use `""` for none. Up to about 4000 characters. |
| `proxy` | Optional. Leave `""` if Telegram opens normally on your computer. See [the proxy guide](#telegram-is-blocked-in-your-country-use-a-proxy) if it does not. |
| `default_hours` | How long the timer is. `24` = one day. `0.5` = 30 minutes. (You can change it later from Telegram.) |
| `warn_before_hours` | How long before the deadline the bot starts warning you. `0` = no warnings. |

**JSON rules that trip people up:**

- Text is in `"double quotes"`. Numbers (like `owner_id`) are **not**.
- Every line ends with a comma **except the last one**.
- On Windows, in file paths use forward slashes `C:/Users/me/secret.pdf` or double backslashes `C:\\Users\\me\\secret.pdf`. A single backslash breaks the file.
- Relative paths like `"secret.pdf"` are relative to the folder you start the program from.

**Keep the config private.** It contains your token and password. It is already listed in `.gitignore`, so Git will not upload it. On Linux also run `chmod 600 config.json`.

> **Alternative:** instead of writing secrets in the file, you can use environment variables: `DEADSWITCH_BOT_TOKEN`, `DEADSWITCH_OWNER_ID`, `DEADSWITCH_CHANNEL_ID`, `DEADSWITCH_PASSWORD`, `DEADSWITCH_PROXY`. They override the file.

---

## Telegram is blocked in your country? Use a proxy

**Do this if** `--check` shows `Timeout was reached`, `Failed to connect`, or `SSL connect error`, even though Telegram opens in your browser.

**Why it happens.** Many VPN apps only change how your **browser** connects (or use a "system proxy" mode). Other programs, like this one, do not use it, so they still hit the block. The fix is to tell the program the VPN's **local address**. That is the `proxy` setting.

### Option A: a VPN that covers the whole computer (easiest)
Turn on a VPN in **VPN / TUN / system-wide mode**, so that *every* program uses it. Then you do **not** need the `proxy` setting. Test it in a Command Prompt:

```
curl -I https://api.telegram.org
```

If you see a line like `HTTP/1.1 302` or `HTTP/2 302`, it works. If it says `Failed to connect` or times out, use Option B.

### Option B: use the `proxy` setting
1. **Turn your VPN/proxy app on** and wait until it says connected.
2. **Find its local port.** Open the app's settings and look for words like *mixed port*, *local port*, *HTTP port* or *SOCKS port*. Common values (your app may differ, always check yours):

   | App | Typical local port |
   |---|---|
   | Hiddify | `12334` |
   | v2rayN | `10808` (SOCKS) / `10809` (HTTP) |
   | Clash | `7890` |
   | NekoRay / NekoBox | `2080` |

3. **Test the port** in a Command Prompt (`Win + R`, type `cmd`, Enter). Use your own port number:

   ```
   curl -x http://127.0.0.1:12334 -I https://api.telegram.org
   ```

   It works if you see `HTTP/1.1 200 Connection established` and then `302`. If it fails, the port is wrong, so try another one.
4. **Write it into `config.json`:**

   ```json
   "proxy": "http://127.0.0.1:12334",
   ```

   - For an **HTTP** proxy use `http://127.0.0.1:PORT`.
   - For a **SOCKS** proxy use `socks5h://127.0.0.1:PORT`.
   - Never write `https://` for the proxy address, it must be `http://` (this is a very common typo).
5. **Keep the VPN/proxy app running** every time the program runs.
6. Run `--check` again ([Step 10](#step-10-run-the-self-check)). You should see four `[OK]` lines.

You can also set the proxy with the environment variable `DEADSWITCH_PROXY` instead of the config file.

> **Best long-term solution:** run the program on a small server in a country where Telegram is **not** blocked. Then you do not need any proxy or VPN, and it can run 24/7. See [Step 12](#step-12-keep-it-running-all-the-time).

---

## Step 10: Run the self-check

The program has a built-in doctor. Run it:

```bash
# Linux / macOS
./build/deadswitch --check

# Windows
build\Release\deadswitch.exe --check
```

A healthy result looks like this:

```
[OK]      Bot token works (@my_safety_net_12345_bot).
[OK]      Channel found: My Test Channel
[OK]      The bot is an administrator and may post messages.
[OK]      The bot can send messages to you.

Everything looks good.
```

If you see `[PROBLEM]`, read the line under it. It tells you what to fix. The most common ones are in [Troubleshooting](#troubleshooting). Fix, run `--check` again, repeat until everything is `[OK]`.

---

## Step 11: Do a full rehearsal (important!)

Never trust a safety system you have not tested. Do this with a **private test channel** (from Step 2), so nothing real is published.

1. Put the **test channel's ID** in `config.json`.
2. Start the program:

   ```bash
   ./build/deadswitch
   ```

   Your bot sends you "Dead man's switch started" with the time left.
3. In Telegram, send your bot: `/test YOUR_PASSWORD`. You receive your file(s) in your private chat. This proves the files can be read and sent.
4. Send just your **password** alone. The bot answers "Password correct" and deletes your message.
5. Send: `/settime 3m YOUR_PASSWORD`. The timer is now 3 minutes.
6. **Wait and do nothing.** After 3 minutes the file(s) appear in the test channel. 🎉 That proves the whole chain works.
7. Send `/resume YOUR_PASSWORD` to arm it again, then `/settime 24h YOUR_PASSWORD` to set the real length.
8. Stop the program (press `Ctrl + C`), put your **real channel's ID** in `config.json`, and run `--check` once more.

---

## Step 12: Keep it running all the time

The program only works while it is running. Choose one way:

### Option A: Linux server, always on (best)

This uses `systemd`, which starts the program when the computer boots and restarts it if it crashes.

```bash
# 1. make a user with no login rights, and a folder
sudo useradd --system --home /opt/deadswitch --shell /usr/sbin/nologin deadswitch
sudo mkdir -p /opt/deadswitch

# 2. copy the program, your config, and your files into it
sudo cp build/deadswitch config.json secret.pdf /opt/deadswitch/
sudo chown -R deadswitch:deadswitch /opt/deadswitch
sudo chmod 600 /opt/deadswitch/config.json

# 3. install and start the service
sudo cp deploy/deadswitch.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now deadswitch

# 4. look at what it says
sudo systemctl status deadswitch
sudo journalctl -u deadswitch -f
```

(In `config.json` the file paths are relative to `/opt/deadswitch`, so `"secret.pdf"` works.)

### Option B: Windows PC

1. Make a folder like `C:\DeadSwitch` and put `deadswitch.exe`, `config.json`, your files, and any `.dll` files there.
2. Open **Task Scheduler** → **Create Task…**
3. *General* tab: give it a name, tick **"Run whether user is logged on or not"**.
4. *Triggers* tab: **New** → *Begin the task:* **At startup**.
5. *Actions* tab: **New** → *Start a program* → pick `deadswitch.exe`, and in **"Start in"** write `C:\DeadSwitch`.
6. *Settings* tab: tick **"If the task fails, restart every 1 minute"**.
7. Also in Windows **Power Options**, set the computer to **never sleep**.

### Option C: Just try it for now

Open a terminal and run `./deadswitch`. It works until you close the window. Fine for testing, **not** for the real thing.

---

## Using it every day

Your whole daily job is **one message**: send your password to your bot.

- The bot answers "Password correct. The timer was reset." and **deletes your message** so the password does not stay in your chat.
- Pin the bot chat in Telegram so it is easy to find.
- Set a daily reminder on your phone.
- Remember the warning the bot sends you (4 hours before the deadline by default).

Going on a trip or a hospital stay and can't check in? Beforehand, send `/settime 14d YOUR_PASSWORD` (two weeks), or `/pause YOUR_PASSWORD`. **Do not forget to `/resume` afterwards.**

---

## All bot commands

They only work in **your private chat** with the bot. Anyone else is ignored. The password must be the **last word** of the message. Any message containing the password is deleted automatically.

| You send | What happens |
|---|---|
| `YOUR_PASSWORD` | Check-in. The timer restarts. |
| `/help` | Shows the list of commands. |
| `/status` | Shows the time left and the deadline. |
| `/check` | Tests the channel and the bot's rights. |
| `/settime 12h YOUR_PASSWORD` | Changes the timer length and restarts it. Use `m` (minutes), `h` (hours), `d` (days). Examples: `90m`, `12h`, `3d`, `1.5d`. A bare number means hours. Allowed: 1 minute to 365 days. |
| `/setwarn 6h YOUR_PASSWORD` | Warns you from 6 hours before the deadline. `/setwarn off YOUR_PASSWORD` turns warnings off. |
| `/pause YOUR_PASSWORD` | Stops the timer. Nothing will be published. |
| `/resume YOUR_PASSWORD` | Starts the timer again from the full length. Also **re-arms** the switch after it has fired. |
| `/test YOUR_PASSWORD` | Sends the files to **you only**, as a rehearsal. Nothing goes to the channel. |
| `/sendnow YOUR_PASSWORD` | Publishes to the channel **immediately**. |

Command line options of the program itself:

| Option | Meaning |
|---|---|
| `--check` | Test settings and Telegram access, then exit. |
| `--config <file>` | Use a different config file. |
| `--state <file>` | Use a different state file. |
| `--version`, `--help` | Show version or help. |

---

## All settings

| Setting | Type | Required | Default | Description |
|---|---|---|---|---|
| `bot_token` | text | yes | | Token from BotFather. |
| `owner_id` | number | yes | | Your Telegram user ID. |
| `channel_id` | text | yes | | `@name` or `-100…` number of the target channel. |
| `password` | text | yes | | 8+ characters, no spaces, must not start with `/`. |
| `files` | list of text | one of `files` / `message` | `[]` | Files to publish, in this order. Each up to 50 MB. |
| `message` | text | one of `files` / `message` | `""` | Text posted first. Up to about 4000 characters. |
| `proxy` | text | no | `""` | Send all Telegram traffic through a proxy, e.g. `socks5h://127.0.0.1:10808` or `http://127.0.0.1:10809`. Only needed if Telegram is blocked on your network. |
| `default_hours` | number | no | `24` | Timer length for a fresh start. Later changed with `/settime`. |
| `warn_before_hours` | number | no | `4` | When to start warning. `0` = off. |

The program also keeps a file called **`state.json`**. It stores the timer so that a restart or a crash does not lose it. Do not edit it while the program runs. Deleting it (while the program is stopped) starts a brand-new timer.

---

## Troubleshooting

**`[PROBLEM] Cannot use the bot token: network problem: Timeout was reached`**
Your computer cannot reach Telegram. First open `https://api.telegram.org` in the browser **on that same computer**. If it does not open, Telegram is blocked on your network. Two fixes:
1. Turn on a **VPN that covers the whole computer** (TUN / "system-wide" mode), then run the program again. Nothing else to change.
2. If you use a proxy app (v2ray, NekoRay, Clash, ...), find its **local port** in the app's settings and write it in `config.json`, for example `"proxy": "socks5h://127.0.0.1:10808"` (SOCKS) or `"proxy": "http://127.0.0.1:10809"` (HTTP). The program does not read the Windows system proxy by itself, so this line is needed. The port number depends on your app.

**`SSL connect error` or `Failure when receiving data from the peer`**
Your VPN or proxy connection is unstable for a moment. The program retries by itself (you may see `Network hiccup ... trying again` in the log), so a single one of these is harmless. If it happens all the time, switch to another server or protocol in your VPN app.

**`[PROBLEM] Cannot use the bot token … Unauthorized`**
The token is wrong. Copy it again from BotFather (`/mybots` → your bot → API Token). Check there are no spaces or missing characters.

**`[PROBLEM] Cannot find the channel … chat not found`**
`channel_id` is wrong, or the bot was never added to the channel. For a public channel it must be `"@name"` (with the `@`). For a private one it must be the long number **with the minus sign**, in quotes.

**`[PROBLEM] The bot is not an administrator of the channel`** / **`"Post Messages" is switched off`**
Go back to [Step 3](#step-3-make-the-bot-an-administrator-of-the-channel) and fix the rights.

**`[PROBLEM] The bot cannot write to you`**
Open your bot in Telegram and press **START**. Also double-check `owner_id`.

**`CONFIG ERROR: file not found`**
The path in `files` is wrong, or you started the program from a different folder. Use a full path to be sure.

**`CONFIG ERROR: … is not valid JSON`**
A quote, comma, or bracket is missing. Paste your file into an online "JSON validator" (remove your secrets first!) to find the exact line.

**The bot does not answer my password**
- The program may not be running (check the terminal or `systemctl status deadswitch`).
- You may be writing to the wrong bot, or from a different Telegram account than `owner_id`.
- The log may say `another copy … uses the same bot token`. Only **one** copy may run per bot token. Stop the other one.

**Log says `error_code 409` / "Conflict"**
Two programs are using the same bot token at once (or a webhook is set). Close the other one. The program removes webhooks automatically on start.

**The timer fired but nothing appeared in the channel**
The log says why (for example "Forbidden: bot is not a member of the channel"). The program retries every 60 seconds until it works, so fix the problem (rights, channel ID) and it will publish by itself.

**I started the program and it published immediately**
The computer was off for longer than the timer. Downtime counts as "not checking in". Before starting, the program does read the messages Telegram kept for it (up to 24 hours), so a password you sent while it was off still counts (as a check-in at the moment the program starts).

**The times look wrong**
The program uses the computer's clock. Make sure the clock and time zone are correct (turn on automatic time).

**I forgot my password**
Edit `config.json`, choose a new one, and restart the program.

---

## Security notes and honest limits

Please read this part. A dead man's switch sounds bulletproof, but it is **not**.

- **If the computer is off or offline at the deadline, nothing happens.** On the next start it publishes (if the time already ran out). A reliable always-on machine is much better than a laptop.
- **Telegram is a third party.** Telegram can see your bot's messages and the files. It can also ban a bot or a channel. Don't rely on this as your *only* plan for anything critical.
- **Whoever controls your Telegram account** can also check in for you (and read your chat). Use two-step verification on Telegram.
- **Whoever can read `config.json`** knows the token and password. Keep the file private (`chmod 600`), and never upload it to GitHub (it is in `.gitignore`).
- **The password travels through Telegram** as a normal message, then the bot deletes it. Telegram's servers still saw it. So use a password that you use **nowhere else**.
- **Only one person can check in**: you (matched by `owner_id`). If something happens to you and a trusted person needs to be able to check in, this version does not support it.
- **Files are not encrypted** by this program. If someone gets into your channel before the deadline they cannot see the files (nothing is there yet), but the files do sit on your computer in plain form.
- Bots can delete messages only if they are fairly recent (less than 48 hours old) and only in a private chat like this one. That is fine for this use.
- The program sends the files again from the beginning if it crashes *in the middle of the upload* and Telegram had already received it. A duplicate is possible, a missing file is not.
- This is a hobby-grade tool, provided **as is**, without any warranty. Test it often, and never use it for anything where failure could cause real harm.

---

## How the program works inside

For the curious (and for contributors):

1. Reads `config.json` (plus environment variables) and checks every value.
2. Runs a **self-check** against Telegram (token, channel, admin rights).
3. Loads `state.json` (or starts a new timer) and saves it again immediately.
4. Loops forever:
   - Computes the time left (`last check-in + timer length − now`).
   - If the time is over: it first reads waiting Telegram messages (so a last-second password wins), and if the time is still over it posts the message and files. Progress is saved after every item, so a failed upload is retried without repeating finished items.
   - If a warning is due: it messages you.
   - It listens for your messages with **long polling** (`getUpdates`, 20 s).
5. Only messages from your private chat with `owner_id` are processed. The check-in time is the computer's own clock at the moment the message is processed (so a wrong Telegram/PC clock difference cannot break it), and the last handled `update_id` is stored, so old messages can never reset the timer again after a restart.
6. State is written to a temporary file and then renamed, so a crash cannot leave a half-written file.

Source code: [`src/deadswitch.cpp`](src/deadswitch.cpp) (single file, about 800 lines).

### Project layout

```
telegram-dead-mans-switch/
├── src/deadswitch.cpp          the whole program
├── CMakeLists.txt              build recipe (Windows / Linux / macOS)
├── config.example.json         copy to config.json and edit
├── deploy/deadswitch.service   systemd service for Linux servers
├── .gitignore                  keeps secrets out of Git
├── LICENSE                     MIT
└── README.md                   this file
```

### Ideas for the future

Pull requests are welcome. Some ideas: several people who may check in, encrypted files, a second backup channel, e-mail warnings, a Docker image.

---

## License

MIT. See [LICENSE](LICENSE). Replace `<YOUR NAME>` in the license file with your own name before publishing.
