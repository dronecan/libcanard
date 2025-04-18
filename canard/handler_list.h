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
#include "helpers.h"

#ifndef CANARD_NUM_HANDLERS
#define CANARD_NUM_HANDLERS 3
#endif

namespace Canard {
 
/// @brief HandlerList to register all handled message types.
class HandlerList {
public:
    /// @brief HandlerList Constructor
    /// @param _transfer_type Type of transfer: CanardTransferTypeBroadcast, CanardTransferTypeRequest, CanardTransferTypeResponse
    /// @param _msgid ID of the message/service
    /// @param _signature Signature of the message/service
    /// @param _index Index of the handler list
    HandlerList(CanardTransferType _transfer_type, uint16_t _msgid, uint64_t _signature, uint8_t _index) NOINLINE_FUNC :
    index(_index) {
        msgid = _msgid;
        signature = _signature;
        transfer_type = _transfer_type;
    }

    /// @brief delete copy constructor and assignment operator
    HandlerList(const HandlerList&) = delete;

    /// @brief accept a message if it is handled by this handler list
    /// @param index Index of the handler list
    /// @param msgid ID of the message/service
    /// @param transfer_type canard tranfer type (Broadcast, request or reply)
    /// @param[out] signature Signature of the message/service
    /// @return true if the message is handled by this handler list
    static bool accept_message(uint8_t index,  uint16_t msgid, CanardTransferType transfer_type, uint64_t &signature) NOINLINE_FUNC
    {
#ifdef WITH_SEMAPHORE
        WITH_SEMAPHORE(sem[index]);
#endif
        HandlerList* entry = head[index];
        while (entry != nullptr) {
            if (entry->msgid == msgid && entry->transfer_type == transfer_type) {
                signature = entry->signature;
                return true;
            }
            entry = entry->next;
        }
        return false;
    }

    /// @brief handle a message if it is handled by this handler list
    /// @param index Index of the handler list
    /// @param transfer transfer object of the request
    static void handle_message(uint8_t index, const CanardRxTransfer& transfer) NOINLINE_FUNC
    {
#ifdef WITH_SEMAPHORE
        WITH_SEMAPHORE(sem[index]);
#endif
        HandlerList* entry = head[index];
        while (entry != nullptr) {
            if (transfer.data_type_id == entry->msgid &&
                entry->transfer_type == transfer.transfer_type) {
                entry->handle_message(transfer);
            }
            entry = entry->next;
        }
    }

    /// @brief Method to handle a message implemented by the derived class
    /// @param transfer transfer object of the request
    virtual void handle_message(const CanardRxTransfer& transfer) = 0;

#ifdef WITH_SEMAPHORE
    // get semaphore reference for a handler_index
    static Canard::Semaphore *get_semaphore(uint8_t handler_index) {
        return handler_index < CANARD_NUM_HANDLERS ? &sem[handler_index] : nullptr;
    }
#endif

protected:
    virtual ~HandlerList() {}
    uint8_t index;
    HandlerList* next;
#ifdef WITH_SEMAPHORE
    static Canard::Semaphore sem[CANARD_NUM_HANDLERS];
#endif

    // add ourselves to the handler list. the caller must be holding the semaphore.
    void link(void) NOINLINE_FUNC {
        next = head[index];
        head[index] = this;
    }

    // remove ourselves from the handler list. the caller must be holding the semaphore.
    void unlink(void) NOINLINE_FUNC {
        HandlerList* entry = head[index];
        if (entry == this) {
            head[index] = next;
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

private:
    static HandlerList* head[CANARD_NUM_HANDLERS];
    uint16_t msgid;
    uint64_t signature;
    CanardTransferType transfer_type;
};

} // namespace Canard

#define DEFINE_HANDLER_LIST_HEADS() Canard::HandlerList* Canard::HandlerList::head[CANARD_NUM_HANDLERS] = {}
#define DEFINE_HANDLER_LIST_SEMAPHORES() Canard::Semaphore Canard::HandlerList::sem[CANARD_NUM_HANDLERS] = {}
