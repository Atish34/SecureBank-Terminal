# SecureBank Terminal
### RFID and PIN-Based ATM Transaction Simulator with PC Database Integration

SecureBank Terminal is an embedded-systems project that simulates basic ATM operations using an **ARM7 LPC2148 microcontroller**, an **RFID card reader**, a **4×4 matrix keypad**, a **16×2 LCD**, and a **Linux PC application**. The microcontroller handles the user interface and peripherals, while the PC application validates users, processes requests, maintains balances, and records transactions in text files.

> **Project type:** Embedded Systems + UART Serial Communication + C/Linux  
> **Main controller:** LPC2148 (ARM7TDMI-S)  
> **Firmware IDE:** Keil µVision (project files are included)  
> **PC software:** C program compiled with GCC on Linux

---

## Contents
- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware and Peripherals](#hardware-and-peripherals)
- [Pin Connections](#pin-connections)
- [How It Works](#how-it-works)
- [Menu and Keypad Operation](#menu-and-keypad-operation)
- [Serial Communication Protocol](#serial-communication-protocol)
- [Project Structure](#project-structure)
- [Setup and Running](#setup-and-running)
- [User Database Format](#user-database-format)
- [Transaction Log](#transaction-log)
- [Troubleshooting](#troubleshooting)
- [Limitations and Security Note](#limitations-and-security-note)

## Features

- RFID card detection and card-ID transfer to the PC.
- PIN verification before access to ATM operations.
- PIN input masking on the LCD (entered digits appear as `*`, where the masked input routine is used).
- Account balance inquiry.
- Cash deposit simulation.
- Cash withdrawal simulation with balance validation.
- Three consecutive incorrect PIN attempts can block an account, according to the PC-side account-status logic.
- Account active/blocked status stored in the user database.
- Mini-statement showing recent transactions.
- PIN-change request/flow in the firmware.
- 16×2 LCD menus and keypad navigation.
- UART communication between the microcontroller and the PC.
- Persistent account records and transaction history using text files.

**Important:** This is an educational ATM simulator. It does not connect to a real bank, dispense physical cash, or provide production-grade banking security.

## System Architecture

```text
        RFID Card
            |
            v
      RFID Reader
            |
       UART1 (TTL)
            |
            v
   +-------------------+
   | LPC2148 / ARM7    |
   |                   |
   | 4x4 Keypad        |
   | 16x2 LCD          |
   | ATM state/menu    |
   +-------------------+
            |
       UART0 (TTL)
            |
      MAX232 / USB-UART
            |
            v
   +-------------------+
   | Linux PC program  |
   | pc_main.c         |
   |                   |
   | users.txt         |
   | transactions.txt  |
   +-------------------+
```

The LPC2148 firmware reads the card, collects keypad input, and displays messages. It sends protocol requests over UART0 to the Linux program. The Linux program checks the card/PIN and account status, processes transactions, updates `users.txt`, and appends transaction records to `transactions.txt`. It sends a response back to the microcontroller for display.

## Hardware and Peripherals

| Component | Purpose |
|---|---|
| LPC2148 development board | Runs the embedded firmware and controls peripherals |
| RFID reader and RFID card/tag | Reads the card identifier used to select an account |
| 4×4 matrix keypad | PIN entry, amount entry, and menu selection |
| 16×2 character LCD (HD44780-compatible) | Displays prompts, menus, balances, and transaction results |
| MAX232 level converter (if using a true RS-232 PC serial port) | Converts MCU UART logic-level signals to/from RS-232 voltage levels |
| USB-to-TTL UART adapter (alternative PC connection) | Connects UART logic levels directly to a PC USB port |
| Linux PC/laptop | Runs the account/database and transaction-handling program |
| Jumper wires and suitable power supply | Hardware interconnections and power |

### MAX232: where it is used

The LPC2148 UART pins use **logic-level UART signals**. A traditional RS-232 serial port uses different voltage levels and polarity, so a **MAX232 is placed between the MCU UART and an RS-232 interface**. Do not connect an RS-232 voltage-level signal directly to an LPC2148 UART pin.

If a USB-to-TTL UART adapter is used, connect its logic-level TX/RX/GND appropriately; a MAX232 is generally not needed between the adapter and the MCU. Confirm the voltage level supported by the adapter and board before connecting.

## Pin Connections

The following pin assignments are taken from the project source definitions. Confirm the exact pin mapping and board wiring before powering the circuit.

### 1. 16×2 LCD — 8-bit interface

The LCD driver writes the data byte to `IOPIN0` starting at bit 16. The control-pin definitions are in `mcu/lcd_defines.h`.

| LCD signal | LPC2148 pin | Project definition |
|---|---|---|
| D0–D7 | P0.16–P0.23 | `LCD_DATA 16` (8-bit data bus) |
| RS | P0.5 | `LCD_RS 5` |
| R/W | P0.6 | `LCD_RW 6` |
| Enable (E) | P0.4 | `LCD_EN 4` |
| VSS / VDD | GND / suitable supply | Follow the LCD module specification |
| V0 / contrast | Contrast circuit/potentiometer | Adjust for readable text |

**Note:** Some comments in the source beside the LCD definitions do not consistently describe the pin numbers. The numeric macros and driver operations are the better reference; verify them against your physical circuit.

### 2. 4×4 Matrix Keypad

The keypad uses Port 1 pins, as defined in `mcu/KPM_defines.h`.

| Keypad lines | LPC2148 pins |
|---|---|
| Row 0 | P1.16 |
| Row 1 | P1.17 |
| Row 2 | P1.18 |
| Row 3 | P1.19 |
| Column 0 | P1.20 |
| Column 1 | P1.21 |
| Column 2 | P1.22 |
| Column 3 | P1.23 |

The key lookup table in `mcu/KPM.c` is:

```text
1  2  3  A
4  5  6  B
7  8  9  *
C  0  =  E
```

The firmware uses numeric keys for PIN/amount input and the configured control keys for backspace, confirmation, and exit. The exact action depends on the current screen.

### 3. UART0 — LPC2148 to Linux PC

UART0 is used by the firmware to send ATM requests and receive PC responses.

| Signal | LPC2148 pin | Connection |
|---|---|---|
| TXD0 | P0.0 | Connect to the other interface's RX |
| RXD0 | P0.1 | Connect to the other interface's TX |
| GND | GND | Common ground |

The firmware selects UART0 using `PINSEL0` and sets a baud-rate divisor in `mcu/uart0_pc.c`. The PC application is configured for **9600 baud, 8 data bits, no parity, 1 stop bit (8-N-1)**. The divisor value must match the LPC2148 peripheral clock configuration; verify it for your board.

### 4. UART1 — RFID reader to LPC2148

The RFID interface is implemented in `mcu/rfid_uart1.c`.

| Signal | LPC2148 pin used by the source | Connection |
|---|---|---|
| TXD1 | P0.8 | Connect to RFID reader TX/RX as required by the reader's UART interface |
| RXD1 | P0.9 | Connect to the opposite UART data direction |
| GND | GND | Common ground |

UART communication is crossed: **reader TX → MCU RXD1**, and **reader RX → MCU TXD1** only if the reader needs commands from the MCU. For a reader that only transmits card IDs, TX → RX and GND may be sufficient. Confirm the reader's datasheet and output format. The firmware expects a framed card value and validates an 8-digit identifier; a reader with a different output format may require code changes.

### 5. Connections overview

- Connect all grounds together where the interfaces require a common reference.
- Cross TX and RX between UART devices.
- Do not connect two TX outputs together.
- Check the LPC2148 board's pin multiplexing, voltage levels, and peripheral clock before testing.
- The source code is the reference for the current pin definitions; this README cannot verify undocumented physical wiring.

## How It Works

1. **Idle / card scan:** The firmware waits for the RFID reader to provide a card ID.
2. **Card verification:** The card ID is sent to the Linux program. The PC checks whether the card exists and whether its account is active.
3. **PIN entry:** If the card is accepted, the LCD prompts for a PIN. The firmware sends the card ID and entered PIN to the PC.
4. **Authentication:** The PC validates the PIN and account status. The user proceeds to the menu only after a successful response.
5. **Operation selection:** The user chooses balance, deposit, withdrawal, mini-statement, or PIN-change functions from the keypad.
6. **Request processing:** The MCU sends the selected operation and any amount to the PC application.
7. **Database update:** The PC validates the request, updates the account balance where needed, saves account data, and records deposit/withdrawal activity.
8. **Result display:** The PC response is sent back to the MCU, and the LCD shows the result.
9. **Exit:** The user can return to the menu or end the session and return to the card-scan screen, according to the firmware's current key handling.

## Menu and Keypad Operation

The firmware's menu is divided into two pages.

| Key | Operation |
|---|---|
| `1` | Balance inquiry |
| `2` | Deposit |
| `3` | Withdrawal |
| `4` | Next menu page / back to previous page |
| `5` | Mini-statement (second page) |
| `6` | PIN change (second page) |
| `*` | Exit session / return to card scanning, depending on screen |
| `B` | Backspace during numeric entry |
| `E` | Confirm numeric input or use the screen's configured exit action |

The exact behavior of `E` and `*` is screen-dependent. If your physical keypad labels differ from the lookup table in `KPM.c`, update the lookup table or wiring accordingly.

## Serial Communication Protocol

The firmware and PC program exchange text messages over UART0. Messages use `$` as the end marker.

### MCU → PC examples

```text
#CARD:12535708$
#CARD:12535708#PIN:1234$
#TXN:BALANCE#REQ$
#TXN:DEPOSIT#AMOUNT:100$
#TXN:WITHDRAW#AMOUNT:200$
```

### PC → MCU examples

```text
@OK$
@ERR#INVALID_CARD$
@ERR#INVALID_PIN$
@OK#BALANCE:7500.00$
```

These are protocol examples to illustrate the message format; actual response strings and operation fields should be checked in `mcu/demo.c` / `mcu/demo_fixed.c` and `linux/pc_main.c`, because those files may differ between firmware versions. Both sides must use the same message format and terminator or communication can time out or fail.

## Project Structure

```text
ARM_majorproject/
├── linux/
│   ├── pc_main.c          # PC-side ATM/account and transaction logic
│   ├── uart.c              # Linux serial-port helper functions
│   ├── uart.h              # UART helper declarations
│   ├── users.txt           # Demo account records
│   └── transactions.txt    # Transaction history/log
│
└── mcu/
    ├── main.c              # MCU application/state-machine source
    ├── demo.c              # Firmware variant
    ├── demo_fixed.c        # Updated firmware variant
    ├── rfid_uart1.c        # RFID reader UART1 driver/interrupt
    ├── uart0_pc.c          # UART0 PC communication functions
    ├── uart0_pc.h
    ├── KPM.c               # Keypad driver and numeric input
    ├── KPM.h
    ├── KPM_defines.h       # Keypad pin definitions
    ├── lcd.c               # LCD driver
    ├── lcd.h
    ├── lcd_defines.h       # LCD pin/command definitions
    ├── delay.c
    ├── delay.h
    ├── define.h
    ├── types.h
    ├── Startup.s           # Startup code
    └── project.uvproj      # Keil µVision project
```

The archive may also include generated build files (`.hex`, `.axf`, `.o`, `.map`, `.dep`, `.crf`, `.lst`, `.htm`, `.plg`) and IDE backup files. These are build artifacts, not all required for understanding the source. The source files listed above are the main files to read first.

## Setup and Running

### A. Build and run the Linux PC application

1. Use a Linux machine (or a Linux environment with access to the serial device).
2. Connect the LPC2148 UART0 through a compatible USB-to-TTL adapter or MAX232 plus a compatible serial interface.
3. Find the serial device, for example:

   ```bash
   ls /dev/ttyUSB*
   ls /dev/ttyACM*
   ```

4. Open a terminal in the `linux` directory:

   ```bash
   cd ARM_majorproject/linux
   gcc pc_main.c uart.c -o pc_main
   ```

5. Run the program with the actual serial-device path:

   ```bash
   ./pc_main /dev/ttyUSB0
   ```

   Replace `/dev/ttyUSB0` with the correct device on your computer. If permission is denied, check your Linux serial-port permissions and group membership.

6. Keep `users.txt` in the program's working directory because the program opens it by filename. The same applies to `transactions.txt`.

### B. Build and flash the LPC2148 firmware

1. Open `mcu/project.uvproj` in Keil µVision with a compatible ARM7/LPC2148 toolchain and device support installed.
2. Review the project target and startup settings.
3. Confirm the selected application source. The archive contains multiple firmware variants (`main.c`, `demo.c`, and `demo_fixed.c`); **do not compile multiple files that each define the same application entry point**. Select the intended version and ensure its required driver files are included.
4. Build the project and resolve compiler/linker errors.
5. Program the resulting firmware onto the LPC2148 board using a compatible programmer/ISP workflow.
6. Verify the RFID reader, keypad, LCD, UART wiring, baud rate, and clock configuration.
7. Start the Linux PC program, then power/reset the MCU and test with a demo card.

**Build note:** This README documents the supplied source layout; it does not claim that every firmware variant builds without modification. Select and test one matching MCU firmware version and PC protocol version together.

## User Database Format

The PC application reads `users.txt`. Each record is one line with fields separated by `|`:

```text
CARD_ID|PIN|NAME|BALANCE|STATUS|FAILED_ATTEMPTS
```

Example format:

```text
12535708|1234|Demo User|7500.00|ACTIVE|0
87654321|6789|Test User|2500.50|ACTIVE|0
```

Field meanings:

| Field | Description |
|---|---|
| `CARD_ID` | RFID card identifier |
| `PIN` | Demo PIN |
| `NAME` | Account holder's display name |
| `BALANCE` | Account balance used by the simulator |
| `STATUS` | Account status such as `ACTIVE` or `BLOCKED` |
| `FAILED_ATTEMPTS` | Failed PIN attempt counter |

These are **demo credentials only**. Change the sample values before sharing the repository publicly if they represent real people or real credentials. Do not store real banking PINs or sensitive account data in this file.

## Transaction Log

The PC application appends transaction records to `transactions.txt`. The basic record format is:

```text
CARD_ID|TRANSACTION_TYPE|AMOUNT
```

Example:

```text
12535708|Deposit|100.00
12535708|Withdraw|200.00
```

The mini-statement reads transaction records and displays recent activity for the currently authenticated card. The precise ordering and supported transaction types depend on the current `pc_main.c` implementation.

## Troubleshooting

| Problem | Things to check |
|---|---|
| PC program says the serial device cannot be opened | Confirm the device path, adapter connection, permissions, and that another program is not using the port |
| LCD shows “PC not responding” | Start `pc_main` first; check UART0 TX/RX crossover, common ground, baud rate, clock/divisor, and `$` message terminators |
| RFID card is not detected | Confirm reader power, UART1 RX/TX mapping, reader baud rate, card-ID format, and interrupt configuration |
| Card is detected but rejected | Check the exact card ID in `users.txt`, including leading zeros and whitespace |
| PIN always fails | Verify the PIN field, keypad layout, request format, and that MCU and PC firmware versions agree |
| Keypad keys are incorrect | Check row/column wiring and the `KPMLUT` lookup table in `KPM.c` |
| LCD text is garbled or missing | Check LCD data/control wiring, contrast, initialization, and power |
| Transactions are not saved | Run the program from the directory containing `users.txt`, check write permissions, and inspect `transactions.txt` |
| Build has duplicate-symbol errors | Ensure only one application file (`main.c`, `demo.c`, or `demo_fixed.c`) defines the selected main logic |

## Limitations and Security Note

- This project is intended for academic demonstration and learning.
- The database is stored in plain-text files, and demo PINs are not securely protected.
- UART messages are not encrypted or authenticated.
- The program is not connected to a real banking network and must not be used for real financial transactions.
- Before publishing on GitHub, remove real personal information, credentials, local build outputs if not needed, and any private configuration.
- Hardware pin assignments and clock-dependent baud-rate divisors must be verified against the exact board and wiring.

## Learning Outcomes

This project demonstrates practical use of:
- ARM7/LPC2148 microcontroller programming in C.
- GPIO and peripheral pin multiplexing.
- UART serial communication and message framing.
- UART receive interrupts for RFID input.
- LCD and matrix-keypad interfacing.
- Embedded state-machine design.
- Linux serial-port programming in C.
- File-based record persistence and transaction logging.
- Debugging communication between embedded firmware and a PC application.

## Author

**Atish Vijay Udawant**

For academic demonstration and portfolio use. Update this section with your GitHub profile, project video, circuit diagram, or hardware photographs if available.
