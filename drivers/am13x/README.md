# Libcanard Driver for TI AM13E230x MCU

This is a compact driver that connects the
[libcanard](https://github.com/dronecan/libcanard) library to
the TI MCAN peripheral on AM13E230x microcontrollers. It has no
dependencies beyond a small part of the standard C library and the TI
DriverLib, and can be used with any operating system or on bare metal.

---

## How It All Fits Together

`canard_am13x.c` knows nothing about DroneCAN
protocol. It only knows how to move raw CAN frames in and out of the
TI MCAN peripheral registers. `app_mcan.c` sits above both and calls
whichever it needs at each step.

---

## Project Structure

This is what the CCS workspace looks like for any given example.

```
example_root_folder
│
├── example.syscfg                  ← Hardware configuration. Open this
│                                     in CCS SysConfig editor to change
│                                     MCAN settings, GPIO, timers.
│
├── am13e230x_lp/
│   └── app_cfg.h                   ← Application constants: node ID,
│                                     memory pool size, DTID values,
│                                     timer instance, tick rate, etc.
│
├── libcanard/                      ← Migrate the entire libcanard
│   ├── canard.c                      directory into the project root.
│   ├── canard.h                      Do not modify these files.
│   ├── canard_internals.h
│   └── drivers/
│       ├── canard_am13x.c          ← TI driver layer implementation.
│       └── canard_am13x.h          ← TI driver layer header.
│
├── app_mcan.c                      ← Application layer. Your DroneCAN
│                                     encode/decode logic lives here.
│
├── main.c                          ← Entry point.
│
└── targetConfigs/                  ← CCS debug configuration.
```

### Adding libcanard to the Build

After copying the `libcanard` directory into the project root, you need
to tell the compiler where to find the headers. Go to:

**Project Properties → Build → Arm Compiler → Include Options**

Add these two paths:

```
-I"${PROJECT_ROOT}/libcanard"
-I"${PROJECT_ROOT}/libcanard/drivers/"
```

---

## SysConfig Setup

TI toolchain uses sysconfig tool to handle peripheral initialization via a no-code GUI interface. This readme also includes a sample example.syscfg file that can be used as a starting point for DroneCAN applications.
can use it as a starting point.

### MCAN Settings (both TX and RX)

- **CAN FD Mode**: Enable
- **Bit Rate Switching**: Enable
- The driver transmits classic CAN frames (`fdf=0`, `brs=0`), so only
  the **arbitration bit rate** is used for actual transmission and
  reception. Note this value down; it must match on all nodes.
- Typical arbitration bit rate: **1 Mbps** or **500 Kbps**

### RX Example: Additional SysConfig Setting

In the MCAN filter configuration, set:

> **Accept Non-Matching Frames** → **Store in RX FIFO 0**

Do this for both Standard and Extended frames, this ensures frames that do not match any programmed filter still reach
the application. Without this, the hardware silently drops them.

### TX Example: Additional SysConfig Setting

Under the Timer (TIMG) configuration:

> Set a **custom timer profile** and configure the period to **1000 ms**
> or **500 ms** depending on how often you want NodeStatus to broadcast.

This timer drives the `g_tick` flag in `app_mcan.c` which triggers the
periodic TX.

#### Adjusting Hardware TX Buffers

If your application demands heavy packet transmission or needs to hold more frames in the hardware queue:
1. Open `example.syscfg` in the GUI interface.
2. Navigate to **MCAN** on the left panel.
3. Drop down the **MCAN Basic Configuration** section.
4. Expand the **Message RAM Configuration** subsection.
5. Locate **Number of Dedicated TX Buffers** and increase the value if needed (default configuration is 1).

### Reference `example.syscfg` File

Below is the complete SysConfig source configuration file used for generating this driver environment:

```javascript
/**
 * Import the modules used in this configuration.
 */
const config = scripting.addModule("/drivers/config");
const gpio   = scripting.addModule("/drivers/gpio");
const gpio1  = gpio.addInstance();
const mcan   = scripting.addModule("/drivers/mcan", {}, false);
const mcan1  = mcan.addInstance();
const sysctl = scripting.addModule("/drivers/sysctl");
const timer  = scripting.addModule("/drivers/timer", {}, false);
const timer1 = timer.addInstance();
const uart   = scripting.addModule("/drivers/uart", {}, false);
const uart1  = uart.addInstance();

/**
 * Write custom configuration values to the imported modules.
 */
gpio1.$name                         = "GPIO_GRP_0";
gpio1.port                          = "PORTA";
gpio1.associatedPins[0].$name       = "TCAN_STB";
gpio1.associatedPins[0].pin.$assign = "PA24";

mcan1.$name                            = "MCAN";
mcan1.wkupReqEnable                    = true;
mcan1.emulationEnable                  = true;
mcan1.autoWkupEnable                   = true;
mcan1.tdcEnable                        = true;
mcan1.additionalCoreConfig             = true;
mcan1.rrfe                             = true;
mcan1.rrfs                             = true;
mcan1.txEventFIFOWaterMark             = 0;
mcan1.txEventFIFOSize                  = 0;
mcan1.txBufNum                         = 1;
mcan1.txBufElemSize                    = "DL_MCAN_ELEM_SIZE_8BYTES";
mcan1.nomTimeSeg1_manual               = 126;
mcan1.nomTimeSeg2_manual               = 31;
mcan1.nomSynchJumpWidth_manual         = 31;
mcan1.dataTimeSeg1_manual              = 14;
mcan1.dataTimeSeg2_manual              = 3;
mcan1.dataSynchJumpWidth_manual        = 3;
mcan1.spPercent                        = 80;
mcan1.desiredNomRate                   = 250;
mcan1.rxFIFO1startAddr                 = 0;
mcan1.rxFIFO1size                      = 0;
mcan1.rxFIFO1waterMark                 = 0;
mcan1.m0interrupts                     = ["DL_MCAN_INTERRUPT_LINE1"];
mcan1.registerInterrupt                = true;
mcan1.enableInterrupt                  = true;
mcan1.interruptLine                    = ["DL_MCAN_INTR_LINE_NUM_1"];
mcan1.interruptLine1Flag               = ["DL_MCAN_INTR_SRC_BUS_OFF_STATUS","DL_MCAN_INTR_SRC_ERR_PASSIVE","DL_MCAN_INTR_SRC_PROTOCOL_ERR_ARB","DL_MCAN_INTR_SRC_PROTOCOL_ERR_DATA","DL_MCAN_INTR_SRC_RX_FIFO0_FULL","DL_MCAN_INTR_SRC_RX_FIFO0_NEW_MSG","DL_MCAN_INTR_SRC_RX_FIFO0_WATERMARK","DL_MCAN_INTR_SRC_TRANS_COMPLETE","DL_MCAN_INTR_SRC_WARNING_STATUS"];
mcan1.interruptFlags                   = ["DL_MCAN_INTR_SRC_BUS_OFF_STATUS","DL_MCAN_INTR_SRC_ERR_PASSIVE","DL_MCAN_INTR_SRC_PROTOCOL_ERR_ARB","DL_MCAN_INTR_SRC_PROTOCOL_ERR_DATA","DL_MCAN_INTR_SRC_RX_FIFO0_FULL","DL_MCAN_INTR_SRC_RX_FIFO0_NEW_MSG","DL_MCAN_INTR_SRC_RX_FIFO0_WATERMARK","DL_MCAN_INTR_SRC_TRANS_COMPLETE","DL_MCAN_INTR_SRC_WARNING_STATUS"];
mcan1.lss                              = 2;
mcan1.lse                              = 2;
mcan1.flesa                            = 8;
mcan1.txStartAddr                      = 24;
mcan1.txEventFIFOStartAddr             = 96;
mcan1.rxBufStartAddr                   = 96;
mcan1.rxFIFO0startAddr                 = 112;
mcan1.rxFIFO0size                      = 10;
mcan1.rxFIFO0waterMark                 = 8;
mcan1.stdFiltType                      = "10";
mcan1.stdFiltElem                      = "001";
mcan1.extFiltElem                      = "001";
mcan1.extFiltType                      = "00";
mcan1.extFiltID2                       = 536870911;
mcan1.stdFiltID1                       = 3;
mcan1.stdFiltID2                       = 4;
mcan1.peripheral.$assign               = "MCAN0";
mcan1.peripheral.txPin.$assign         = "PA12";
mcan1.peripheral.rxPin.$assign         = "PA11";
mcan1.txPinConfig.direction            = scripting.forceWrite("OUTPUT");
mcan1.txPinConfig.hideOutputInversion  = scripting.forceWrite(false);
mcan1.txPinConfig.onlyInternalResistor = scripting.forceWrite(false);
mcan1.txPinConfig.passedPeripheralType = scripting.forceWrite("Digital");
mcan1.txPinConfig.$name                = "ti_driverlib_gpio_GPIOPinGeneric0";
mcan1.rxPinConfig.hideOutputInversion  = scripting.forceWrite(false);
mcan1.rxPinConfig.onlyInternalResistor = scripting.forceWrite(false);
mcan1.rxPinConfig.passedPeripheralType = scripting.forceWrite("Digital");
mcan1.rxPinConfig.$name                = "ti_driverlib_gpio_GPIOPinGeneric1";

sysctl.useHFCLK     = true;
sysctl.SYSPLLSource = "DL_SYSCTL_SYSPLL_REF_HFCLK";
sysctl.SYSPLL_Qdiv  = 32;

timer1.$name             = "APP_TIMER_0";
timer1.registerInterrupt = true;
timer1.timerClkDiv       = "DL_TIMER_CLOCK_DIVIDE_8";
timer1.timerClkPrescale  = 256;
timer1.timerMode         = "DL_TIMER_TIMER_MODE_PERIODIC";
timer1.interrupts        = ["DL_TIMER_INTERRUPT_ZERO_EVENT"];
timer1.timerPeriod       = "1000 ms";

uart1.$name                            = "APP_MCANTX_LOGUART_0";
uart1.targetBaudRate                   = 115200;
uart1.peripheral.$assign               = "UC4";
uart1.peripheral.rxPin.$assign         = "PA1";
uart1.peripheral.txPin.$assign         = "PA0";
uart1.txPinConfig.direction            = scripting.forceWrite("OUTPUT");
uart1.txPinConfig.hideOutputInversion  = scripting.forceWrite(false);
uart1.txPinConfig.onlyInternalResistor = scripting.forceWrite(false);
uart1.txPinConfig.passedPeripheralType = scripting.forceWrite("Digital");
uart1.txPinConfig.$name                = "drivers_gpio_v0_gpioPinConfig0";
uart1.rxPinConfig.hideOutputInversion  = scripting.forceWrite(false);
uart1.rxPinConfig.onlyInternalResistor = scripting.forceWrite(false);
uart1.rxPinConfig.passedPeripheralType = scripting.forceWrite("Digital");
uart1.rxPinConfig.$name                = "drivers_gpio_v0_gpioPinConfig1";

/**
 * Pinmux solution for unlocked pins/peripherals. This ensures that minor changes to the automatic solver in a future
 * version of the tool will not impact the pinmux you originally saw.  These lines can be completely deleted in order to
 * re-solve from scratch.
 */
sysctl.peripheral.$suggestSolution       = "SYSCTL";
sysctl.peripheral.x1Pin.$suggestSolution = "PC16_X1";
sysctl.peripheral.x2Pin.$suggestSolution = "PC17_X2";
timer1.peripheral.$suggestSolution       = "TIMG4_0";

```
---

## Initialisation Sequence

```
main()
 │
 ├── Device_Init()
 │     TI device support layer. Sets up clocks and power domains.
 │
 ├── SYSCFG_DL_init()
 │     Generated by example.syscfg. Configures MCAN peripheral,
 │     GPIO pins, and timers. Must run before canardTI_MCAN_Init().
 │
 ├── canardTI_MCAN_Init(MCAN0, CanardTI_MCAN_IfaceModeNormal)
 │     Verifies MCAN is running, releases TCAN
 │     transceiver from standby, applies bus mode.
 │
 ├── canardTI_MCAN_ConfigureAcceptanceFilters(filters, count)
 │     Programmes hardware RX filters.
 │
 ├── canardInit(...)
 │     libcanard. Initialises instance, assigns memory pool,
 │     registers shouldAccept and onReception callbacks.
 │
 ├── canardSetLocalNodeID(&g_canard, APP_NODE_ID)
 │     libcanard. Assigns this node's DroneCAN ID.
 │
 ├── DL_TimerG_startCounter(APP_TIMER_INST)
 │     Starts the periodic tick timer configured in example.syscfg.
 │
 └── loop: App_McanTx_Run() or App_McanRx_Run()
```
---

### WARNING: Not Thread-Safe

This driver does not use IRQ or critical sections. It is safe to call
its API functions from IRQ context **provided the application ensures
that no concurrent calls occur from thread and IRQ context
simultaneously**. The application is responsible for all guarding.

