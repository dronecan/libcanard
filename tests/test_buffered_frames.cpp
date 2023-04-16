/*
 * Copyright (c) 2022 Siddharth B Purohit, CubePilot Pty Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <gtest/gtest.h>
#include <canard.h>
#include <canard_internals.h>
#include <math.h>

#if CANARD_MAX_MTU > 8

static bool g_should_accept = true;
/**
 * This callback is invoked by the library when a new message or request or response is received.
 */
static void onTransferReceived(CanardInstance* ins,
                               CanardRxTransfer* transfer)
{
    (void)ins;
    (void)transfer;
}

static bool shouldAcceptTransfer(const CanardInstance* ins,
                                 uint64_t* out_data_type_signature,
                                 uint16_t data_type_id,
                                 CanardTransferType transfer_type,
                                 uint8_t source_node_id)
{
    (void)ins;
    (void)out_data_type_signature;
    (void)data_type_id;
    (void)transfer_type;
    (void)source_node_id;
    return g_should_accept;
}

TEST(CanardBufferedCANFrame, TestPushBytes)
{
    ASSERT_LE(sizeof(CanardTxQueueItem), CANARD_MEM_BLOCK_SIZE);
    ASSERT_EQ((sizeof(CanardCANFrame)-CANARD_MAX_MTU), sizeof(CanardTxQueueCANFrame));

    uint8_t canard_memory_pool[1024];
    CanardInstance canard;
    CanardTxQueueItem *queue_item = NULL;
    CanardCANFrame frame;
    double total_allocated_bytes = 0.0;

    canardInit(&canard, canard_memory_pool, sizeof(canard_memory_pool),
                onTransferReceived, shouldAcceptTransfer, NULL);

    // Test 1: Only one byte of payload
    queue_item = createTxItem(&canard.allocator);
    frame.data[0] = 0x11;
    frame.data_len = 1;
    ASSERT_EQ(canardBufferedCANFramePushBytes(&canard.allocator, queue_item, frame.data, frame.data_len), CANARD_OK);

    // ensure no space was allocated, except for the Tx Item
    ASSERT_EQ(canard.allocator.statistics.current_usage_blocks, 1);

    // check the frame
    canardBufferedCANFrameToCANFrame(&canard.allocator, &frame, queue_item);
    ASSERT_EQ(frame.data_len, 1);
    ASSERT_EQ(frame.data[0], 0x11);

    // push 7 more bytes
    memset(frame.data, 0x22, 7);
    frame.data_len = 7;
    ASSERT_EQ(canardBufferedCANFramePushBytes(&canard.allocator, queue_item, frame.data, frame.data_len), CANARD_OK);

    // depending on the size of payload head, a block may or may not be allocated
    if (CANARD_TX_QUEUE_PAYLOAD_HEAD_SIZE >= 8) {
        ASSERT_EQ(canard.allocator.statistics.current_usage_blocks, 1);
    } else {
        ASSERT_EQ(canard.allocator.statistics.current_usage_blocks, 2);
    }
    total_allocated_bytes = 8.0 - CANARD_TX_QUEUE_PAYLOAD_HEAD_SIZE;

    // check the frame
    canardBufferedCANFrameToCANFrame(&canard.allocator, &frame, queue_item);
    ASSERT_EQ(frame.data_len, 8);
    ASSERT_EQ(frame.data[0], 0x11);
    for (int i = 1; i < 8; i++) {
        ASSERT_EQ(frame.data[i], 0x22);
    }

    // push 48 more byte
    memset(frame.data, 0x33, 48);
    frame.data_len = 48;
    ASSERT_EQ(canardBufferedCANFramePushBytes(&canard.allocator, queue_item, frame.data, frame.data_len), CANARD_OK);
    total_allocated_bytes += 48.0;

    // (48 / (Block size)) should be allocated
    ASSERT_EQ(canard.allocator.statistics.current_usage_blocks, ceil(total_allocated_bytes / CANARD_BUFFER_BLOCK_DATA_SIZE) + 1);

    // check the frame
    canardBufferedCANFrameToCANFrame(&canard.allocator, &frame, queue_item);
    ASSERT_EQ(frame.data_len, 56);
    ASSERT_EQ(frame.data[0], 0x11);
    for (int i = 1; i < 8; i++) {
        ASSERT_EQ(frame.data[i], 0x22);
    }
    for (int i = 8; i < 56; i++) {
        ASSERT_EQ(frame.data[i], 0x33);
    }

    // push 4 more bytes
    memset(frame.data, 0x55, 4);
    frame.data_len = 4;
    ASSERT_EQ(canardBufferedCANFramePushBytes(&canard.allocator, queue_item, frame.data, frame.data_len), CANARD_OK);
    total_allocated_bytes += 4.0;
    ASSERT_EQ(canard.allocator.statistics.current_usage_blocks, ceil(total_allocated_bytes / CANARD_BUFFER_BLOCK_DATA_SIZE) + 1);

    // check the frame
    canardBufferedCANFrameToCANFrame(&canard.allocator, &frame, queue_item);
    ASSERT_EQ(frame.data_len, 60);
    ASSERT_EQ(frame.data[0], 0x11);
    for (int i = 1; i < 8; i++) {
        ASSERT_EQ(frame.data[i], 0x22);
    }
    for (int i = 8; i < 56; i++) {
        ASSERT_EQ(frame.data[i], 0x33);
    }
    for (int i = 56; i < 60; i++) {
        ASSERT_EQ(frame.data[i], 0x55);
    }

    // push 4 more bytes
    memset(frame.data, 0x66, 4);
    frame.data_len = 4;
    ASSERT_EQ(canardBufferedCANFramePushBytes(&canard.allocator, queue_item, frame.data, frame.data_len), CANARD_OK);
    total_allocated_bytes += 4.0;
    ASSERT_EQ(canard.allocator.statistics.current_usage_blocks, ceil(total_allocated_bytes / CANARD_BUFFER_BLOCK_DATA_SIZE) + 1);

    // check the frame
    canardBufferedCANFrameToCANFrame(&canard.allocator, &frame, queue_item);
    ASSERT_EQ(frame.data_len, 64);
    ASSERT_EQ(frame.data[0], 0x11);
    for (int i = 1; i < 8; i++) {
        ASSERT_EQ(frame.data[i], 0x22);
    }
    for (int i = 8; i < 56; i++) {
        ASSERT_EQ(frame.data[i], 0x33);
    }
    for (int i = 56; i < 60; i++) {
        ASSERT_EQ(frame.data[i], 0x55);
    }
    for (int i = 60; i < 64; i++) {
        ASSERT_EQ(frame.data[i], 0x66);
    }
}
#endif
