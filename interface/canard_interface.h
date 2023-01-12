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
 *
 */

#pragma once

#include <canard/interface.h>
#include <canard.h>

namespace Canard {

class CanardInterface : public Interface {

public:
    CanardInterface(uint8_t index) :
    Interface(index) {}

    /// @brief delete copy constructor and assignment operator
    CanardInterface(const CanardInterface&) = delete;
    CanardInterface& operator=(const CanardInterface&) = delete;
    CanardInterface() = delete;

    void init(void* mem_arena, size_t mem_arena_size);

    /// @brief broadcast message to all listeners on Interface
    /// @param bc_transfer
    /// @return true if message was added to the queue
    bool broadcast(const Transfer &bcast_transfer) override;

    /// @brief request message from
    /// @param destination_node_id
    /// @param req_transfer
    /// @return true if request was added to the queue
    bool request(uint8_t destination_node_id, const Transfer &req_transfer) override;

    /// @brief respond to a request
    /// @param destination_node_id
    /// @param res_transfer
    /// @return true if response was added to the queue
    bool respond(uint8_t destination_node_id, const Transfer &res_transfer) override;

    void handle_frame(const CanardCANFrame &frame, uint64_t timestamp_usec);

    virtual void update_tx(uint64_t timestamp_usec) = 0;

    static void onTransferReception(CanardInstance* ins, CanardRxTransfer* transfer);
    static bool shouldAcceptTransfer(const CanardInstance* ins,
                                     uint64_t* out_data_type_signature,
                                     uint16_t data_type_id,
                                     CanardTransferType transfer_type,
                                     uint8_t source_node_id);

    uint8_t get_node_id() const override { return canard.node_id; }

protected:
    CanardInstance canard {};
};

class CanardTestInterface;
class CanardTestNetwork {
    friend class CanardTestInterface;
public:
    static constexpr int N = CANARD_NUM_HANDLERS;
    CanardTestNetwork() {}
    void route_frame(CanardTestInterface *send_iface, const CanardCANFrame &frame, uint64_t timestamp_usec);
    CanardTestNetwork(const CanardTestNetwork&) = delete;
    CanardTestNetwork& operator=(const CanardTestNetwork&) = delete;

    static CanardTestNetwork& get_network() {
        static CanardTestNetwork network;
        return network;
    }
private:
    CanardTestInterface *ifaces[N];
};

class CanardTestInterface : public CanardInterface {
public:
    CanardTestInterface(int index) :
    CanardInterface(index) {
        CanardTestNetwork::get_network().ifaces[index] = this;
    }
    // push tx frame queue to network
    void update_tx(uint64_t timestamp_usec) override;
    // set node id
    void set_node_id(uint8_t node_id);
};

} // namespace Canard

/// @brief get singleton instance of interface, used by node to publish messages
#define CXX_CANARD_TEST_INTERFACE_DEFINE(index) \
    Canard::CanardTestInterface test_interface_##index {index};

#define CXX_CANARD_TEST_INTERFACE(index) test_interface_##index
