# DMA-Based-Data-Transfer-System
A bare-metal, register-level implementation of a continuous real-time data acquisition system for the STM32F407VG (Cortex-M4). Data flows from an ADC through DMA into a circular SRAM buffer, with interrupt-driven double-buffering and deferred main-loop processing.
