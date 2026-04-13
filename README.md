# NetPulse 🔍

> **Rootless ICMP Network Discovery Tool for Termux**
> *Made by Aryan Giri*

---

## Overview

NetPulse is a fast, parallel ICMP-based network scanner written in C, designed specifically for **Termux on Android** — no root required. It discovers live hosts on your local network by leveraging the system `ping` binary with a configurable thread pool.

```
  ███╗   ██╗███████╗████████╗██████╗ ██╗   ██╗██╗     ███████╗███████╗
  ████╗  ██║██╔════╝╚══██╔══╝██╔══██╗██║   ██║██║     ██╔════╝██╔════╝
  ██╔██╗ ██║█████╗     ██║   ██████╔╝██║   ██║██║     ███████╗█████╗
  ██║╚██╗██║██╔══╝     ██║   ██╔═══╝ ██║   ██║██║     ╚════██║██╔══╝
  ██║ ╚████║███████╗   ██║   ██║     ╚██████╔╝███████╗███████║███████╗
  ╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚═╝      ╚═════╝ ╚══════╝╚══════╝╚══════╝

   Rootless ICMP Network Scanner  │  v1.0  │  Made by Aryan Giri
```

---

## Features

| Feature              | Details                                              |
| -------------------- | ---------------------------------------------------- |
| 🚀 Parallel scanning | 200 simultaneous threads                             |
| 🔐 Rootless          | Uses system `ping` — no raw socket privileges needed |
| 🧮 Auto subnet calc  | Converts subnet mask → CIDR, derives full IP range   |
| 📊 CSV export        | Optional timestamped output file                     |
| 🎨 Color terminal UI | Live progress bar + real-time host discovery         |
| 📱 Termux native     | Built for Android ARMv8 / x86                        |

---

## Installation (Termux)

```bash
# 1. Install dependencies
pkg update && pkg install git clang make

# 2. Clone the repository
git clone https://github.com/giriaryan694-a11y/NetPulse.git
cd NetPulse

# 3. Compile
make

# 4. Run
./netpulse
```

---

## Getting Your Network Info (Android)

To use NetPulse, you need your **Gateway IP** and **Subnet Mask**. On most Android devices:

1. Go to **Settings → WiFi**
2. Tap on your connected WiFi network
3. Tap the **gear (⚙️) icon** or **network details**
4. Look for:

   * **Gateway** (e.g., `192.168.1.1`)
   * **Subnet mask** (e.g., `255.255.255.0`)

These values are required to scan your local network.

---

## Usage

```
  Enter Gateway IP   : 192.168.1.1
  Enter Subnet Mask  : 255.255.255.0
```

NetPulse will:

1. Calculate the CIDR prefix (`/24`)
2. Derive the network address (`192.168.1.0`)
3. Generate all 254 host IPs (`192.168.1.1` → `192.168.1.254`)
4. Ping all IPs in parallel using 200 threads
5. Print alive hosts in real-time
6. Offer to save results as CSV

**Example output:**

```
  ✔  192.168.1.1    — Host is UP
  ✔  192.168.1.100  — Host is UP
  ✔  192.168.1.105  — Host is UP

  ┌────────┬───────────────────┬───────────────────┐
  │  No.   │    IP Address     │      Status       │
  ├────────┼───────────────────┼───────────────────┤
  │  1     │  192.168.1.1      │  UP ✔             │
  │  2     │  192.168.1.100    │  UP ✔             │
  │  3     │  192.168.1.105    │  UP ✔             │
  └────────┴───────────────────┴───────────────────┘

  [*] Total alive  : 3
  [*] Total scanned: 254
  [*] Scan duration: 4 second(s)

  [?] Save results to CSV? (y/n): y
  [+] Results saved → netpulse_20240815_143022.csv
```

---

## CSV Output Format

```csv
No,IP Address,Status,Gateway,Subnet Mask,CIDR,Network,Scanned At
1,192.168.1.1,UP,192.168.1.1,255.255.255.0,/24,192.168.1.0,2024-08-15 14:30:22
2,192.168.1.100,UP,192.168.1.1,255.255.255.0,/24,192.168.1.0,2024-08-15 14:30:22
```

---

## Subnet Mask Reference

| Subnet Mask     | CIDR | Hosts |
| --------------- | ---- | ----- |
| 255.255.255.0   | /24  | 254   |
| 255.255.0.0     | /16  | 65534 |
| 255.255.255.128 | /25  | 126   |
| 255.255.255.192 | /26  | 62    |

---

## Compilation Options

```bash
# Standard build
make

# Install system-wide in Termux
make install

# Clean build artifacts
make clean

# Manual compile
gcc netpulse.c -o netpulse -lpthread -O2 -Wall
```

---

## How It Works

```
  Gateway: 192.168.1.1  +  Mask: 255.255.255.0
         ↓
  Network: 192.168.1.0  →  Range: .1 to .254
         ↓
  200 Threads → Each grabs an IP → ping -c 1 -W 1 <IP>
         ↓
  Exit code 0 = HOST UP  →  Add to results
```

The tool avoids raw sockets entirely. Instead, each thread calls the system `ping` binary which already has `CAP_NET_RAW` set via Android's permission model — making this fully rootless.

---

## Legal Notice

> This tool is intended for use on networks you own or have explicit permission to scan. Unauthorized network scanning may violate local laws. Use responsibly in controlled/lab environments.

---

## Author

**Made by Aryan Giri**
NetPulse v1.0 — Rootless ICMP Network Discovery
