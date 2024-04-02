#include "handler_list.h"

using namespace Canard;

HandlerList::HandlerList(CanardTransferType _transfer_type, uint16_t _msgid, uint64_t _signature, uint8_t _index) :
index(_index) {
    if (index >= CANARD_NUM_HANDLERS) {
        return;
    }
#ifdef CANARD_MUTEX_ENABLED
    WITH_SEMAPHORE(sem[index]);
#endif
    next = head[index];
    head[index] = this;
    msgid = _msgid;
    signature = _signature;
    transfer_type = _transfer_type;
}

HandlerList::~HandlerList()
{
#ifdef CANARD_MUTEX_ENABLED
    WITH_SEMAPHORE(sem[index]);
#endif
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

bool HandlerList::accept_message(uint8_t index, uint16_t msgid, uint64_t &signature)
{
#ifdef CANARD_MUTEX_ENABLED
    WITH_SEMAPHORE(sem[index]);
#endif
    HandlerList* entry = head[index];
    while (entry != nullptr) {
        if (entry->msgid == msgid) {
            signature = entry->signature;
            return true;
        }
        entry = entry->next;
    }
    return false;
}

void HandlerList::handle_message(uint8_t index, const CanardRxTransfer& transfer)
{
#ifdef CANARD_MUTEX_ENABLED
    WITH_SEMAPHORE(sem[index]);
#endif
    HandlerList* entry = head[index];
    while (entry != nullptr) {
        if (transfer.data_type_id == entry->msgid &&
            entry->transfer_type == transfer.transfer_type) {
            entry->handle_message(transfer);
            return;
        }
        entry = entry->next;
    }
}
