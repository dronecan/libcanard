#include "service_server.h"

using namespace Canard;

ServerBase::ServerBase(Interface &_interface, uint16_t _msgid, uint64_t _signature) :
    HandlerList(CanardTransferTypeRequest, _msgid, _signature, _interface.get_index()),
    interface(_interface)
{}

bool ServerBase::respond(const CanardRxTransfer& transfer,
                        uint16_t data_type_id,
                        uint64_t data_type_signature,
                        uint8_t* rsp_buf,
                        uint32_t len)
{
    // send the message if encoded successfully
    if (len == 0) {
        return false;
    }
    Transfer rsp_transfer = {
        .transfer_type = CanardTransferTypeResponse,
        .data_type_signature = data_type_signature,
        .data_type_id = data_type_id,
        .inout_transfer_id = &transfer_id,
        .priority = transfer.priority,
        .payload = rsp_buf,
        .payload_len = len,
#if CANARD_MULTI_IFACE
        .iface_mask = iface_mask,
#endif
#if CANARD_ENABLE_CANFD
        .canfd = transfer.canfd,
#endif
        .timeout_ms = timeout,
    };
    return interface.respond(transfer.source_node_id, rsp_transfer);
}
