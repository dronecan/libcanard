/*
 * Copyright (C) 2026 Texas Instruments Incorporated
 *
 * Distributed under the MIT License, available in the file LICENSE.
 *
 *
 *
 * Libcanard interface layer for TI MCAN peripheral.
 * WARNING: This driver is NOT thread-safe. It does not use IRQ or
 *          critical sections. It is safe to call its API functions
 *          from IRQ context provided the application ensures that
 *          no concurrent calls occur from thread and IRQ context
 *          simultaneously.
 *
 * SysConfig dependency:
 *   CAN bit timing, message RAM layout, GPIO pin assignments, and
 *   peripheral clocks must all be configured via the project's
 *   example.syscfg file before calling canardTI_MCAN_Init().
 *   This driver does not configure timing registers at runtime.
 *   The following SysConfig-generated file must be included by the
 *   application build:
 *     ti_sdk_dl_config.h / ti_sdk_dl_config.c
 */

#ifndef CANARD_AM13X_H
#define CANARD_AM13X_H

#include <stdint.h>
#include <stdbool.h>
#include "canard.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                          Error Codes                                       */
/* ========================================================================== */

/*
 * Error code values are defined as positive integers and returned negated by all
 * API functions. This avoids collision with libcanard error codes which
 * are small negative integers (e.g. -CANARD_ERROR_OUT_OF_MEMORY = -2).
 *
 * Example check:
 *   if (ret == -CANARD_TI_MCAN_ERROR_INVALID_ARGUMENT) { ... }
 */
#define CANARD_TI_MCAN_ERROR_INVALID_ARGUMENT       1000
#define CANARD_TI_MCAN_ERROR_NOT_INIT               1001
#define CANARD_TI_MCAN_ERROR_MODE_NOT_SET           1002
#define CANARD_TI_MCAN_ERROR_TRANSCEIVER            1003

/* ========================================================================== */
/*                          Operating Modes                                   */
/* ========================================================================== */

/*
 * CanardTI_MCAN_IfaceMode
 * IfaceModeNormal:
 *   Normal bus operation. Use for all production node operation.
 *
 * IfaceModeSilent:
 *   Monitor mode. The node receives frames but does not transmit,
 *   including no ACK bits. Use for automatic bitrate detection or
 *   passive bus monitoring. 
 *
 * IfaceModeLoopback:
 *   Internal loopback. TX frames are received locally without
 *   appearing on the physical bus. Useful for self-test.
 *
 * IfaceModeAutomaticTxAbortOnError:
 *   Cancels any pending TX on a bus error event. Required during
 *   dynamic node ID allocation per the DroneCAN specification.
 */
typedef enum
{
    CanardTI_MCAN_IfaceModeNormal                  = 0,
    CanardTI_MCAN_IfaceModeSilent                  = 1,
    CanardTI_MCAN_IfaceModeLoopback                = 2,
    CanardTI_MCAN_IfaceModeAutomaticTxAbortOnError = 3
} CanardTI_MCAN_IfaceMode;

/* ========================================================================== */
/*                          Acceptance Filter Configuration                  */
/* ========================================================================== */

/*
 * CanardTI_MCAN_AcceptanceFilterConfig
 * The id and mask fields support the CANARD_CAN_FRAME_EFF flag,
 *   CANARD_CAN_FRAME_EFF set in id and mask  → extended (29-bit) filter
 *   CANARD_CAN_FRAME_EFF clear               → standard (11-bit) filter
 *
 * The EFF flag is stripped before writing to hardware registers.
 * TI MCAN has separate filter RAM for standard and extended frames.
 * canardTI_MCAN_ConfigureAcceptanceFilters() routes each entry to the
 * correct hardware filter RAM automatically based on the EFF flag.
 *
 * Examples:
 *
 *   Accept all extended frames (DroneCAN default):
 *     id   = CANARD_CAN_FRAME_EFF
 *     mask = CANARD_CAN_FRAME_EFF
 *
 *   Accept NodeStatus (DTID 341) from any node at any priority:
 *     id   = CANARD_CAN_FRAME_EFF | 0x00015500U
 *     mask = CANARD_CAN_FRAME_EFF | 0x00FFFF80U
 *
 *   Accept all standard frames:
 *     id   = 0U
 *     mask = 0U
 */
typedef struct
{
    uint32_t id;
    uint32_t mask;
} CanardTI_MCAN_AcceptanceFilterConfig;

/* ========================================================================== */
/*                          Statistics                                        */
/* ========================================================================== */

typedef struct
{
    uint32_t txCount;
    uint32_t rxCount;
    uint32_t errorCount;
    uint32_t rxOverflowCount;
} CanardTI_MCAN_Stats;

/* ========================================================================== */
/*                          Initialisation                                   */
/* ========================================================================== */

/**
 * canardTI_MCAN_Init
 *
 * Initialises the TI MCAN peripheral for use with libcanard.
 *   - Takes an mcan_instance pointer. TI devices may have multiple
 *     MCAN peripherals; pass the one configured in example.syscfg
 *     (e.g. MCAN0).
 *   - No timing parameter. Bit timing is configured entirely by
 *     SysConfig in example.syscfg and applied before this call.
 *
 * Prerequisites:
 *   - SYSCFG_DL_init() must have been called before this function.
 *   - The MCAN peripheral must be in Normal operating mode.
 *   - The TCAN transceiver STB GPIO pin must be configured in
 *     example.syscfg as GPIO_GRP_0_TCAN_STB_PIN (active-low standby).
 *
 * This function can be invoked any number of times; every invocation
 * re-initialises everything from scratch.
 *
 * @param mcanInstance  Pointer to MCAN_Regs peripheral. Must not be NULL.
 * @param ifaceMode     Operating mode.
 * @return  0           Success.
 * @return  negative    Negated CANARD_TI_MCAN_ERROR_* code on failure.
 */
int16_t canardTI_MCAN_Init(void*                  mcanInstance,
                            CanardTI_MCAN_IfaceMode ifaceMode);

/* ========================================================================== */
/*                          Acceptance Filters                                */
/* ========================================================================== */

/**
 * canardTI_MCAN_ConfigureAcceptanceFilters
 * Programmes hardware acceptance filters from the provided list.
 * Entries with CANARD_CAN_FRAME_EFF set are routed to extended filter
 * RAM. Entries without it are routed to standard filter RAM.
 *
 * Setting zero filters rejects all frames 
 * To accept all extended frames pass one filter with:
 *   id = CANARD_CAN_FRAME_EFF, mask = CANARD_CAN_FRAME_EFF
 *
 * Note: During filter reconfiguration the peripheral briefly enters
 * SW_INIT mode. Frames arriving during this window may be lost.
 *
 * @param filterConfigs     Array of id/mask filter descriptors.
 * @param numFilterConfigs  Number of entries. Maximum 8.
 * @return  0               Success.
 * @return  negative        Negated CANARD_TI_MCAN_ERROR_* code on failure.
 */
int16_t canardTI_MCAN_ConfigureAcceptanceFilters(
    const CanardTI_MCAN_AcceptanceFilterConfig* filterConfigs,
    uint8_t                                     numFilterConfigs);

/* ========================================================================== */
/*                          Transmit                                          */
/* ========================================================================== */

/**
 * canardTI_MCAN_Transmit
 * Writes one libcanard frame to the MCAN TX buffer and requests
 * transmission. Priority inversion is checked: if a lower-priority
 * frame is pending and the new frame has higher priority, the pending
 * frame is superseded.
 *
 * This function never blocks.
 *
 * @param frame  Frame to transmit. Must not be NULL.
 * @return  1    Frame accepted and queued for TX.
 * @return  0    TX buffer busy. Caller should retry on next cycle.
 * @return  negative  Negated CANARD_TI_MCAN_ERROR_* code on failure.
 */
int16_t canardTI_MCAN_Transmit(const CanardCANFrame* frame);

/**
 * canardTI_MCAN_IsTxReady
 *
 * Returns true when the TX buffer is free and a new frame can be
 * submitted via canardTI_MCAN_Transmit().
 */
bool canardTI_MCAN_IsTxReady(void);

/**
 * canardTI_MCAN_IsTxComplete
 *
 * Returns true once the last submitted frame has been successfully
 * transmitted on the bus (TXBTO register bit set).
 * Call this after canardTI_MCAN_Transmit() returns 1 to confirm
 * the frame has left the node.
 */
bool canardTI_MCAN_IsTxComplete(void);

/* ========================================================================== */
/*                          Receive                                           */
/* ========================================================================== */

/**
 * canardTI_MCAN_Receive
 * Reads one frame from MCAN RX FIFO 0 into out_frame.
 * This function never blocks.
 *
 * @param outFrame  Destination frame buffer. Must not be NULL.
 * @return  1       Frame read successfully.
 * @return  0       No frame available (FIFO empty).
 * @return  negative  Negated CANARD_TI_MCAN_ERROR_* code on failure.
 */
int16_t canardTI_MCAN_Receive(CanardCANFrame* outFrame);

/**
 * canardTI_MCAN_IsRxAvailable
 *
 * Returns true when at least one frame is waiting in RX FIFO 0.
 * Use as a lightweight check before calling canardTI_MCAN_Receive().
 */
bool canardTI_MCAN_IsRxAvailable(void);

/* ========================================================================== */
/*                          Statistics                                        */
/* ========================================================================== */

CanardTI_MCAN_Stats canardTI_MCAN_GetStats(void);

void canardTI_MCAN_ResetStats(void);

#ifdef __cplusplus
}
#endif
#endif /* CANARD_AM13X_H */
