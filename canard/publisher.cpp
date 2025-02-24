#include "publisher.h"

using namespace Canard;

bool PublisherBase::send(uint16_t data_type_id,
            uint64_t data_type_signature,
            uint8_t* msg_buf,
            uint32_t len
#if CANARD_ENABLE_CANFD
            , bool canfd
#endif
            ) {
    if (len == 0) {
        return false;
    }
    Transfer msg_transfer {};
    msg_transfer.transfer_type = CanardTransferTypeBroadcast;
    msg_transfer.data_type_id = data_type_id;
    msg_transfer.data_type_signature = data_type_signature;
    msg_transfer.payload = msg_buf;
    msg_transfer.payload_len = len;
#if CANARD_ENABLE_CANFD
    msg_transfer.canfd = canfd;
#endif
#if CANARD_MULTI_IFACE
    msg_transfer.iface_mask = CANARD_IFACE_ALL;
#endif
    return Sender::send(msg_transfer);
}

bool Sender::send(Transfer& transfer, uint8_t destination_node_id) {
    switch (transfer.transfer_type)
    {
    case CanardTransferTypeBroadcast:
        transfer.inout_transfer_id = TransferObject::get_tid_ptr(interface.get_index(),transfer.data_type_id, CanardTransferTypeBroadcast, interface.get_node_id(), destination_node_id);
        transfer.priority = priority;
        transfer.timeout_ms = timeout;
        return interface.broadcast(transfer);
    case CanardTransferTypeRequest:
        transfer.inout_transfer_id = TransferObject::get_tid_ptr(interface.get_index(),transfer.data_type_id, CanardTransferTypeRequest, interface.get_node_id(), destination_node_id);
        transfer.priority = priority;
        transfer.timeout_ms = timeout;
        return interface.request(destination_node_id, transfer);
    case CanardTransferTypeResponse:
    default:
        return false;
    }
}