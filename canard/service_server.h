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
#include "handler_list.h"
#include "interface.h"

namespace Canard {

/// @brief Server class to handle service requests
/// @tparam svctype 
template <typename svctype>
class Server : public HandlerList {

public:
    /// @brief Server constructor
    /// @param _interface Interface object
    /// @param _cb Callback object
    /// @param _index HandlerList instance id
    Server(Interface &_interface, Callback<typename svctype::c_req_type> &_cb) : 
    HandlerList(CanardTransferTypeRequest, svctype::ID, svctype::SIGNATURE, _interface.get_index()),
    interface(_interface),
    cb(_cb) {
        // multiple servers are not allowed, so no list
    }

    // delete copy constructor and assignment operator
    Server(const Server&) = delete;

    /// @brief handles incoming messages
    /// @param transfer transfer object of the request
    void handle_message(const CanardRxTransfer& transfer) override {
        typename svctype::c_req_type msg {};
        svctype::req_decode(&transfer, &msg);
        transfer_id = transfer.transfer_id;
        // call the registered callback
        cb(transfer, msg);
    }

    /// @brief Send a response to the request
    /// @param transfer transfer object of the request
    /// @param msg message containing the response
    /// @return true if the response was put into the queue successfully
    bool respond(const CanardRxTransfer& transfer, typename svctype::c_rsp_type& msg) {
        // encode the message
        uint16_t len = svctype::rsp_encode(&msg, rsp_buf
#if CANARD_ENABLE_CANFD
        , !transfer.canfd
#elif CANARD_ENABLE_TAO_OPTION
        , true
#endif
        );
        // send the message if encoded successfully
        if (len > 0) {
#if CANARD_ENABLE_CANFD
            rsp_transfer.canfd = transfer.canfd;
#endif
#if CANARD_MULTI_IFACE
            rsp_transfer.iface_mask = iface_mask;
#endif
            rsp_transfer.transfer_type = CanardTransferTypeResponse;
            rsp_transfer.inout_transfer_id = &transfer_id;
            rsp_transfer.data_type_id = svctype::ID;
            rsp_transfer.data_type_signature = svctype::SIGNATURE;
            rsp_transfer.payload = rsp_buf;
            rsp_transfer.payload_len = len;
            rsp_transfer.priority = transfer.priority;
            rsp_transfer.timeout_ms = timeout;
            return interface.respond(transfer.source_node_id, rsp_transfer);
        }
        return false;
    }

    /// @brief Set the timeout for the response
    /// @param _timeout timeout in milliseconds
    void set_timeout_ms(uint32_t _timeout) {
        timeout = _timeout;
    }

private:
    Transfer rsp_transfer;
    uint8_t rsp_buf[svctype::RSP_MAX_SIZE];
    Interface &interface;
    Callback<typename svctype::c_req_type> &cb;

    uint32_t timeout = 1000;
    uint8_t transfer_id = 0;
#if CANARD_MULTI_IFACE
    uint8_t iface_mask = CANARD_IFACE_ALL;
#endif
};

} // namespace Canard

/// Helper macros to create server instances

/// @brief create a server instance
/// @param IFACE interface instance name
/// @param SRVNAME server instance name
/// @param SVCTYPE service type name
/// @param REQHANDLER request handler function
#define CANARD_CREATE_SERVER(IFACE, SRVNAME, SVCTYPE, REQHANDLER) \
    Canard::StaticCallback<SVCTYPE##_cxx_iface::c_req_type> SRVNAME##_callback{REQHANDLER}; \
    Canard::Server<SVCTYPE##_cxx_iface> SRVNAME{IFACE, SRVNAME##_callback};

/// @brief create a client instance
/// @param IFACE interface instance name
/// @param SRVNAME server instance name
/// @param SVCTYPE service type
/// @param CLASS class name
/// @param REQHANDLER request handler callback member function of OBJ
#define CANARD_CREATE_SERVER_CLASS(IFACE, SRVNAME, SVCTYPE, CLASS, REQHANDLER) \
    Canard::ObjCallback<CLASS, SVCTYPE##_cxx_iface::c_req_type> SRVNAME##_callback{this, REQHANDLER}; \
    Canard::Server<SVCTYPE##_cxx_iface> SRVNAME{IFACE, SRVNAME##_callback};
