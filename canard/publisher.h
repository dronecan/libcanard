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
#include "interface.h"
#include <canard.h>
#include "transfer_object.h"
#include "helpers.h"

namespace Canard {

/// @brief Base class for data senders using Transfer Object, such as publishers and requesters
class Sender {
public:
    Sender(Interface &_interface) :
    interface(_interface)
    {}

    // delete copy constructor and assignment operator
    Sender(const Sender&) = delete;

    inline void set_priority(uint8_t _priority) {
        priority = _priority;
    }

    inline void set_timeout_ms(uint32_t _timeout) {
        timeout = _timeout;
    }

    inline uint32_t get_timeout_ms() {
        return timeout;
    }

protected:
    Interface &interface; ///< Interface to send the message on
    /// @brief Send a message
    /// @param Transfer message to send
    /// @return true if the message was put into the queue successfully
    bool send(Transfer& transfer, uint8_t destination_node_id = CANARD_BROADCAST_NODE_ID) NOINLINE_FUNC;
private:
    uint8_t priority = CANARD_TRANSFER_PRIORITY_MEDIUM; ///< Priority of the message
    uint32_t timeout = 1000; ///< Timeout of the message in ms
};

class PublisherBase : public Sender {
public:
    PublisherBase(Interface &_interface) :
    Sender(_interface)
    {}

protected:
    bool send(uint16_t data_type_id,
            uint64_t data_type_signature,
            uint8_t* msg_buf,
            uint32_t len
#if CANARD_ENABLE_CANFD
            , bool canfd
#endif
            ) NOINLINE_FUNC;
};

template <typename msgtype>
class Publisher : public PublisherBase {
public:
    Publisher(Interface &_interface) :
    PublisherBase(_interface)
    {}

    // delete copy constructor and assignment operator
    Publisher(const Publisher&) = delete;

    /// @brief Broadcast a message
    /// @param msg message to send
    /// @return true if the message was put into the queue successfully
    bool broadcast(msgtype& msg) {
        return broadcast(msg, interface.is_canfd());
    }

    /// @brief Broadcast a message
    /// @param msg message to send
    /// @param canfd true if the message should be sent as CAN FD
    /// @return true if the message was put into the queue successfully
    bool broadcast(msgtype& msg, bool canfd) {
#if !CANARD_ENABLE_CANFD
        if (canfd) {
            return false;
        }
#endif
        // encode the message
        uint32_t len = msgtype::cxx_iface::encode(&msg, msg_buf 
#if CANARD_ENABLE_CANFD
        , !canfd
#elif CANARD_ENABLE_TAO_OPTION
        , true
#endif
        );
        // send the message if encoded successfully
        return PublisherBase::send(msgtype::cxx_iface::ID,
                    msgtype::cxx_iface::SIGNATURE,
                    (uint8_t*)msg_buf,
                    len
#if CANARD_ENABLE_CANFD
                    ,canfd
#endif
                    );
    }
private:
    uint8_t msg_buf[msgtype::cxx_iface::MAX_SIZE]; ///< Buffer to store the encoded message
};
} // namespace Canard

/// @brief Macro to create a publisher
/// @param IFACE name of the interface
/// @param PUBNAME name of the publisher
/// @param MSGTYPE type of the message
#define CANARD_PUBLISHER(IFACE, PUBNAME, MSGTYPE) \
    Canard::Publisher<MSGTYPE> PUBNAME{IFACE};
