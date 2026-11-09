# 🤖 ESP32 Hardware-in-the-Loop CI Pipeline

**Tech Stack:** GitHub Actions · GitLab CI · FreeRTOS · Python · ESP32 · PlatformIO · TLS/MQTT  
**Category:** Embedded Systems · Firmware · CI/CD · Hardware-in-the-Loop Testing

> From a simple superloop to a secure, fully-automated embedded system — **build → flash → test → report**, running on physical hardware, not just simulation.

---

## 📂 Project Structure — Two Distinct Branches

This repository shows the evolution of an embedded design, with two clearly separated paths:

| Branch | Architecture | Key Features |
|---|---|---|
| **`main`** | Superloop (single-threaded) | Minimal foundation · No RTOS · Basic sensors & comms · Simple, easy to debug |
| **`freertos-tasks`** | FreeRTOS Multi-Tasking | Secure TLS/MQTT · decoupled task design · full CI/CD automation · Hardware-in-the-Loop testing |

---

## 🌿 Branch: `main` — Superloop Foundation

The starting baseline: a clean, straightforward firmware with no external OS dependency.

- **Superloop / main-loop architecture** — predictable, fully deterministic execution
- Sensor support: DHT temperature & humidity
- Peripherals: UART, I²C, SPI
- WiFi & basic MQTT connectivity
- Build: PlatformIO configuration
- Purpose: **baseline comparison** — reference for reliability and performance improvements

---

## 🚀 Branch: `freertos-tasks` — FreeRTOS + Secure Comms + HIL CI

The advanced production-grade implementation — fully automated and security-hardened.

### FreeRTOS Task Architecture
- Decoupled dedicated tasks: sensor acquisition, MQTT/TLS transmission, BLE advertising, system monitoring
- Priority-driven scheduling
- Non-blocking design — no single component stalls the system
- Clean separation between data collection, processing, and transmission

### Secure Communications
- **TLS 1.2+** encrypted MQTT over WiFi
- Secure credential handling — credentials generated at CI runtime, never committed to repo
- Persistent MQTT sessions & automatic reconnection logic
- JSON-formatted telemetry payloads

### Hardware-in-the-Loop (HIL) Test Framework
- **Python serial automation harness** — controls and validates the physical ESP32 over USB
- Tests execute on **real hardware**, not just emulation
- Coverage:
  - ✅ System boot & health
  - ✅ DHT sensor readings
  - ✅ WiFi connection stability
  - ✅ MQTT publish/receive under outage conditions
  - ✅ I²C / SPI bus diagnostics
  - ✅ BLE advertising
  - ✅ Reset & recovery behaviour
- **Quantified reliability gains** over 90‑second simulated broker outage:
  - Longest sensor gap: **6.99 s → 5.01 s**
  - Lost telemetry messages: **52 → 0**

### CI/CD Pipeline
- **Runners:** GitHub Actions self-hosted runner + GitLab CI / Jenkins configurations
- **Workflow:**
  1. Checkout code → inject secure credentials
  2. Build firmware via PlatformIO
  3. Flash directly to physical ESP32
  4. Run full HIL test suite
  5. Report pass/fail status
- Trigger: push, pull request, or manual dispatch
- Secrets managed via encrypted repository variables

---

## 🛠️ Problems Solved

| Challenge | Resolution |
|---|---|
| Self-hosted runner not picking up jobs | Corrected runner labels & dedicated hardware registration |
| Missing credentials at build time | On-the-fly header generation — no secrets committed |
| Windows serial encoding crashes | Output sanitisation & robust parsing |
| Board not ready after flashing | Tuned reset timing & port open delays |
| MQTT outage false failures | Matched actual board output, not idealised text |
| Intermittent BLE timeouts | Extended per-test timeouts to match real hardware latency |

---

## 📊 Current Status

| Item | Status |
|---|---|
| Firmware build | ✅ Stable — PlatformIO compiles cleanly |
| Automated flash | ✅ Operational — deploys to physical hardware |
| HIL test suite | ✅ 12/15 passing — remaining items being refined |
| CI/CD pipeline | ✅ Fully green — every push runs the pipeline |
| Reliability | ✅ Zero lost messages during broker outage testing |

---

## 🔗 Repository Links

- **Project Home:** [github.com/kp003919/ESP32_GITHUP_ACTION_CI](https://github.com/kp003919/ESP32_GITHUP_ACTION_CI)
- **`main` Branch:** Superloop baseline — simple, minimal, deterministic
- **`freertos-tasks` Branch:** FreeRTOS + TLS + HIL CI — production-grade automation

---

## 💡 Summary

> This project demonstrates the complete embedded engineering journey — from a simple polling loop through to a secure, multi-tasking RTOS design, fully validated by automated hardware testing. It applies software-development best practices to firmware: version control, repeatable builds, continuous integration, and regression testing — all running on real silicon.