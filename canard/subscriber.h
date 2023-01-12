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

#include <stdint.h>
#include <canard.h>
#include "callbacks.h"
#include "handler_list.h"

namespace Canard {

/// @brief Class to handle broadcast messages
/// @tparam msgtype 
template <typename msgtype>
class Subscriber : public HandlerList {
public:
    /// @brief Subscriber Constructor
    /// @param _cb callback function
    /// @param _index HandlerList instance id
    Subscriber(Callback<typename msgtype::c_msg_type> &_cb, uint8_t _index) :
    HandlerList(CanardTransferTypeBroadcast, msgtype::ID, msgtype::SIGNATURE, _index),
    cb (_cb) {
        next = branch_head[index];
        branch_head[index] = this;
    }

    // delete copy constructor and assignment operator
    Subscriber(const Subscriber&) = delete;

    // destructor, remove the entry from the singly-linked list
    ~Subscriber() {
        Subscriber<msgtype>* entry = branch_head[index];
        if (entry == this) {
            branch_head[index] = next;
            return;
        }
        while (entry != nullptr) {
            if (entry->next == this) {
                entry->next = next;
                return;
            }
            entry = entry->next;
        }
    }

    /// @brief parse the message and call the callback
    /// @param transfer transfer object
    void handle_message(const CanardRxTransfer& transfer) override {
        typename msgtype::c_msg_type msg {};
        msgtype::decode(&transfer, &msg);
        // call all registered callbacks in one go
        Subscriber<msgtype>* entry = branch_head[index];
        while (entry != nullptr) {
            entry->cb(transfer, msg);
            entry = entry->next;
        }
    }

private:
    Subscriber<msgtype>* next;
    static Subscriber<msgtype> *branch_head[CANARD_NUM_HANDLERS];
    Callback<typename msgtype::c_msg_type> &cb;
};

template <typename msgtype>
Subscriber<msgtype>* Subscriber<msgtype>::branch_head[] = {nullptr};

} // namespace Canard

/// Helper macros to register message handlers

/// @brief Register a message handler using indexed handler_list.
/// @param NID handler_list instance id
/// @param SUBNAME name of the subscriber instance
/// @param MSGTYPE message type name
/// @param MSG_HANDLER callback function, called when message is received
#define CANARD_SUBSCRIBE_MSG_INDEXED(NID, SUBNAME, MSGTYPE, MSG_HANDLER) \
    Canard::StaticCallback<MSGTYPE##_cxx_iface::c_msg_type> SUBNAME##_callback{MSG_HANDLER}; \
    Canard::Subscriber<MSGTYPE##_cxx_iface> SUBNAME{SUBNAME##_callback, NID};

/// @brief Register a message handler
/// @param SUBNAME name of the subscriber instance
/// @param MSGTYPE message type name
/// @param MSG_HANDLER callback function, called when message is received
#define CANARD_SUBSCRIBE_MSG(SUBNAME, MSGTYPE, MSG_HANDLER) \
    Canard::StaticCallback<MSGTYPE##_cxx_iface::c_msg_type> SUBNAME##_callback{MSG_HANDLER}; \
    Canard::Subscriber<MSGTYPE##_cxx_iface> SUBNAME{SUBNAME##_callback, 0};

/// @brief Register a message handler with object instance using indexed handler_list
/// @param NID handler_list instance id
/// @param SUBNAME name of the subscriber instance
/// @param MSGTYPE message type name
/// @param CLASS class name
/// @param MSG_HANDLER callback function, called when message is received
#define CANARD_SUBSCRIBE_MSG_CLASS_INDEX(NID, SUBNAME, MSGTYPE, CLASS, MSG_HANDLER) \
    Canard::ObjCallback<CLASS, MSGTYPE##_cxx_iface::c_msg_type> SUBNAME##_callback{this, MSG_HANDLER}; \
    Canard::Subscriber<MSGTYPE##_cxx_iface> SUBNAME{SUBNAME##_callback, NID};

/// @brief Register a message handler with object instance
/// @param SUBNAME name of the subscriber instance
/// @param MSGTYPE message type name
/// @param CLASS class name
/// @param MSG_HANDLER callback function, called when message is received
#define CANARD_SUBSCRIBE_MSG_CLASS(SUBNAME, MSGTYPE, CLASS, MSG_HANDLER) \
    Canard::ObjCallback<CLASS, MSGTYPE##_cxx_iface::c_msg_type> SUBNAME##_callback{this, MSG_HANDLER}; \
    Canard::Subscriber<MSGTYPE##_cxx_iface> SUBNAME{SUBNAME##_callback, 0};
