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

namespace Canard {

class CoreTestInterface;
class TestNetwork {
    friend class CoreTestInterface;
public:
    static constexpr int N = 10;
    TestNetwork() {}
    void route_msg(CoreTestInterface *send_iface, uint8_t source_node_id, uint8_t destination_node_id, Transfer transfer);
    TestNetwork(const TestNetwork&) = delete;
    TestNetwork& operator=(const TestNetwork&) = delete;

    static TestNetwork& get_network() {
        static TestNetwork network;
        return network;
    }
private:
    CoreTestInterface *ifaces[N];
};

class CoreTestInterface : public Interface {
public:
    CoreTestInterface(int index) :
    Interface(index) {
        TestNetwork::get_network().ifaces[index] = this;
    }

    /// @brief delete copy constructor and assignment operator
    CoreTestInterface(const CoreTestInterface&) = delete;
    CoreTestInterface& operator=(const CoreTestInterface&) = delete;
    CoreTestInterface() = delete;

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

    /// @brief set node id
    /// @param node_id
    void set_node_id(uint8_t node_id);

    /// @brief get node id
    /// @return node id
    uint8_t get_node_id() const override { return node_id; }

    void handle_transfer(CanardRxTransfer &transfer);

private:
    uint8_t node_id;
};

} // namespace Canard

/// @brief get singleton instance of interface, used by node to publish messages
#define CXX_TEST_INTERFACE_DEFINE(index) \
    Canard::CoreTestInterface test_interface_##index {index};

#define CXX_TEST_INTERFACE(index) test_interface_##index
