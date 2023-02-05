#include "canard_interface.h"

using namespace Canard;

void CanardInterface::init(void* mem_arena, size_t mem_arena_size) {
    canardInit(&canard, mem_arena, mem_arena_size, onTransferReception, shouldAcceptTransfer, this);
}

bool CanardInterface::broadcast(const Transfer &bcast_transfer) {
    // do canard broadcast
    return canardBroadcast(&canard,
                            bcast_transfer.data_type_signature,
                            bcast_transfer.data_type_id,
                            bcast_transfer.inout_transfer_id,
                            bcast_transfer.priority,
                            bcast_transfer.payload,
                            bcast_transfer.payload_len) > 0;
}

bool CanardInterface::request(uint8_t destination_node_id, const Transfer &req_transfer) {
    // do canard request
    return canardRequestOrRespond(&canard,
                                    destination_node_id,
                                    req_transfer.data_type_signature,
                                    req_transfer.data_type_id,
                                    req_transfer.inout_transfer_id,
                                    req_transfer.priority,
                                    CanardRequest,
                                    req_transfer.payload,
                                    req_transfer.payload_len) > 0;
}

bool CanardInterface::respond(uint8_t destination_node_id, const Transfer &res_transfer) {
    // do canard respond
    return canardRequestOrRespond(&canard,
                                    destination_node_id,
                                    res_transfer.data_type_signature,
                                    res_transfer.data_type_id,
                                    res_transfer.inout_transfer_id,
                                    res_transfer.priority,
                                    CanardResponse,
                                    res_transfer.payload,
                                    res_transfer.payload_len) > 0;
}

void CanardInterface::handle_frame(const CanardCANFrame &frame, uint64_t timestamp_usec) {
    canardHandleRxFrame(&canard, &frame, timestamp_usec);
}

void CanardInterface::onTransferReception(CanardInstance* ins, CanardRxTransfer* transfer) {
    CanardInterface* iface = (CanardInterface*) ins->user_reference;
    iface->handle_message(*transfer);
}

bool CanardInterface::shouldAcceptTransfer(const CanardInstance* ins,
                                           uint64_t* out_data_type_signature,
                                           uint16_t data_type_id,
                                           CanardTransferType transfer_type,
                                           uint8_t source_node_id) {
    CanardInterface* iface = (CanardInterface*) ins->user_reference;
    return iface->accept_message(data_type_id, *out_data_type_signature);
}

void CanardTestNetwork::route_frame(CanardTestInterface *send_iface, const CanardCANFrame &frame, uint64_t timestamp_usec) {
    for (int i = 0; i < N; i++) {
        if ((send_iface != ifaces[i]) && (ifaces[i] != nullptr)) {
            ifaces[i]->handle_frame(frame, timestamp_usec);
        }
    }
}

void CanardTestInterface::update_tx(uint64_t timestamp_usec) {
    for (const CanardCANFrame* txf = canardPeekTxQueue(&canard); txf != NULL; txf = canardPeekTxQueue(&canard)) {
        CanardTestNetwork::get_network().route_frame(this, *txf, timestamp_usec);
        canardPopTxQueue(&canard);
    }
}

void CanardTestInterface::set_node_id(uint8_t node_id) {
    canardSetLocalNodeID(&canard, node_id);
}