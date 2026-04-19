 Walkthrough — Cortex-M DMA-Based Data Transfer System

 Overview

A bare-metal, register-level implementation of a continuous real-time data acquisition system for the **STM32F407VG** (Cortex-M4). Data flows from an ADC through DMA into a circular SRAM buffer, with interrupt-driven double-buffering and deferred main-loop processing.

Project Structure

```
Project/
├── Inc/
│   ├── system_config.h      # All tuneable parameters (clocks, pins, buffer size)
│   ├── dma_adc.h             # ADC+DMA driver public API
│   ├── uart_output.h         # Non-blocking UART TX public API
│   └── perf_measure.h        # DWT cycle-counter utilities
└── Src/
    ├── main.c                # Entry point, clock config, main processing loop
    ├── dma_adc.c             # TIM3 + ADC1 + DMA2 Stream0 driver & ISR
    ├── uart_output.c         # USART2 + DMA1 Stream6 TX driver & ISR
    └── perf_measure.c        # DWT CYCCNT init/start/stop
```

Data Flow Architecture

```
┌─────────┐    TRGO     ┌───────┐    DMA2 S0    ┌──────────────┐
│  TIM3   │ ──────────► │ ADC1  │ ────────────► │ adc_buffer[] │
│ (10kHz) │   trigger   │ CH0   │   auto xfer   │ (SRAM, 1024) │
└─────────┘             └───────┘               └──────┬───────┘
                                                       │
                              HT / TC Interrupts       │
                              (set volatile flags)     │
                                                       ▼
                                              ┌────────────────┐
                                              │   main() loop  │
                                              │ process_buffer │
                                              └───────┬────────┘
                                                      │
                                              ┌───────▼────────┐
                                              │  USART2 DMA TX │
                                              │  (non-blocking) │
                                              └────────────────┘
```

Key Design Decisions

| Decision | Rationale |
|---|---|
| **Timer-triggered ADC** (continuous mode OFF) | Guarantees exact 10 kHz sampling. Continuous mode free-runs at hardware speed, which is non-deterministic. |
| **ISR sets flags only** | Prevents interrupt thread starvation. Heavy processing in ISR would block SysTick, UART, and lower-priority interrupts. |
| **Buffer in standard SRAM** | DMA2 cannot access CCM RAM (0x10000000) on the STM32F407. Silent failure results otherwise. |
| **Peripheral increment disabled** | DMA must always read from the same ADC data register address (`ADC1->DR`). |
| **DMA-based UART TX** | Keeps the main loop non-blocking. A polled `printf` at 115200 baud would easily overrun the processing window. |
| **DWT cycle counter** | Zero-overhead profiling to prove processing completes within the DMA fill window. |
| **Overrun detection** (red LED) | If both half-buffer flags are set simultaneously, processing is too slow — a visual alarm. |

Verification Checklist

- [ ] **Build:** Import into STM32CubeIDE, add CMSIS/device headers, compile with no errors.
- [ ] **LED toggle:** Green LED lights on startup confirming clock config is valid.
- [ ] **Buffer fill:** Set a breakpoint or live-watch on `adc_buffer[]` — values should update continuously without CPU polling.
- [ ] **Timing:** Toggle orange LED around `process_buffer()`. Measure with oscilloscope — pulse width must be shorter than 51.2 ms (half-buffer fill time at 10 kHz with 512 samples).
- [ ] **UART output:** Connect a serial terminal at 115200 baud. Observe continuous `AVG: xxx (yyy mV) [zzz us]` lines.
- [ ] **Overrun:** Red LED should remain OFF under normal conditions. If it lights, increase `BUFFER_SIZE` or reduce processing complexity.

Next Steps

- Integrate into STM32CubeIDE project (add startup file, linker script, CMSIS headers).
- Tune `SAMPLING_FREQ_HZ` and `BUFFER_SIZE` for your specific application.
- Replace `process_buffer()` averaging with your actual DSP algorithm (FIR filter, peak detection, etc.).
