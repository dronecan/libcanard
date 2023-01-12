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

#include "cxx_test_interface.h"

using namespace Canard;

void TestNetwork::route_msg(CoreTestInterface *send_iface, uint8_t source_node_id, uint8_t destination_node_id, Transfer transfer) {
    // prepare CanardRxTransfer
    CanardRxTransfer rx_transfer {};
    rx_transfer.data_type_id = transfer.data_type_id;
    rx_transfer.payload_len = transfer.payload_len; 
    rx_transfer.payload_head = (uint8_t*)transfer.payload;
    rx_transfer.transfer_id = *transfer.inout_transfer_id;
    rx_transfer.source_node_id = source_node_id;
    rx_transfer.transfer_type = transfer.transfer_type;
    // send to all interfaces
    for (auto iface : ifaces) {
        if (iface != send_iface && iface != nullptr) {
            iface->handle_transfer(rx_transfer);
        }
    }
}

/// @brief broadcast message to all listeners on Interface
/// @param bc_transfer
/// @return true if message was added to the queue
bool CoreTestInterface::broadcast(const Transfer &bcast_transfer) {
    // call network router
    TestNetwork::get_network().route_msg(this, node_id, 255, bcast_transfer);
    return true;
}

/// @brief request message from
/// @param destination_node_id
/// @param req_transfer
/// @return true if request was added to the queue
bool CoreTestInterface::request(uint8_t destination_node_id, const Transfer &req_transfer) {
    // call network router
    TestNetwork::get_network().route_msg(this, node_id, destination_node_id, req_transfer);
    return true;
}

/// @brief respond to a request
/// @param destination_node_id
/// @param res_transfer
/// @return true if response was added to the queue
bool CoreTestInterface::respond(uint8_t destination_node_id, const Transfer &res_transfer) {
    // call network router
    TestNetwork::get_network().route_msg(this, node_id, destination_node_id, res_transfer);
    return true;
}

/// @brief set node id
/// @param node_id
void CoreTestInterface::set_node_id(uint8_t _node_id) {
    node_id = _node_id;
}

void CoreTestInterface::handle_transfer(CanardRxTransfer &transfer) {
    uint64_t signature = 0;
    // check if message should be accepted
    if (accept_message(transfer.data_type_id, signature)) {
        // call message handler
        handle_message(transfer);
    }
}

