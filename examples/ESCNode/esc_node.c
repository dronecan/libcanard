/*
  A simple example DroneCAN node implementing a 4-in-1 ESC

  This example implements 6 features:

   - announces on the bus using NodeStatus at 1Hz
   - answers GetNodeInfo requests
   - implements dynamic node allocation
   - listens for ESC RawCommand commands and extracts throttle levels
   - sends ESC Status messages (with synthetic data based on throttles)
   - a parameter server for reading and writing node parameters

  This example uses socketcan on Linux for CAN transport

  Example usage: ./esc_node vcan0
*/
/*
 This example application is distributed under the terms of CC0 (public domain dedication).
 More info: https://creativecommons.org/publicdomain/zero/1.0/
*/

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <canard.h>
#include <socketcan.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

// include the headers for the generated DroneCAN messages from the
// dronecan_dsdlc compiler
#include <dronecan_msgs.h>

/*
  libcanard library instance and a memory pool for it to use
 */
static CanardInstance canard;
static uint8_t memory_pool[1024];

/*
  in this example we will use dynamic node allocation if MY_NODE_ID is zero
 */
#define MY_NODE_ID 0

/*
  our preferred node ID if nobody else has it
 */
#define PREFERRED_NODE_ID 73


/*
  keep the state of 4 ESCs, simulating a 4 in 1 ESC node
 */
#define NUM_ESCS 4
static struct esc_state {
    float throttle;
    uint64_t last_update_us;
} escs[NUM_ESCS];


/*
  a set of parameters to present to the user. In this example we don't
  actually save parameters, this is just to show how to handle the
  parameter protocool
 */
static struct parameter {
    char *name;
    enum uavcan_protocol_param_Value_type_t type;
    float value;
    float min_value;
    float max_value;
} parameters[] = {
    { "CAN_NODE", UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE, MY_NODE_ID, 0, 127 },
    { "MyPID_P", UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE, 1.2, 0.1, 5.0 },
    { "MyPID_I", UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE, 1.35, 0.1, 5.0 },
    { "MyPID_D", UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE, 0.025, 0.001, 1.0 },
};

// some convenience macros
#define MIN(a,b) ((a)<(b)?(a):(b))
#define C_TO_KELVIN(temp) (temp + 273.15f)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

/*
  hold our node status as a static variable. It will be updated on any errors
 */
static struct uavcan_protocol_NodeStatus node_status;

/*
  get a 64 bit monotonic timestamp in microseconds since start. This
  is platform specific
 */
static uint64_t micros64(void)
{
    static uint64_t first_us;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t tus = (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL);
    if (first_us == 0) {
        first_us = tus;
    }
    return tus - first_us;
}

/*
  get monotonic time in milliseconds since startup
 */
static uint32_t millis32(void)
{
    return micros64() / 1000ULL;
}

/*
  get a 16 byte unique ID for this node, this should be based on the CPU unique ID or other unique ID
 */
static void getUniqueID(uint8_t id[16])
{
    memset(id, 0, 16);
    FILE *f = fopen("/etc/machine-id", "r");
    if (f) {
        fread(id, 1, 16, f);
        fclose(f);
    }
}

/*
  handle parameter GetSet request
 */
static void handle_param_GetSet(CanardInstance* ins, CanardRxTransfer* transfer)
{
    struct uavcan_protocol_param_GetSetRequest req;
    if (uavcan_protocol_param_GetSetRequest_decode(transfer, &req)) {
        return;
    }

    struct parameter *p = NULL;
    if (req.name.len != 0) {
        for (uint16_t i=0; i<ARRAY_SIZE(parameters); i++) {
            if (req.name.len == strlen(parameters[i].name) &&
                strncmp((const char *)req.name.data, parameters[i].name, req.name.len) == 0) {
                p = &parameters[i];
                break;
            }
        }
    } else if (req.index < ARRAY_SIZE(parameters)) {
        p = &parameters[req.index];
    }
    if (p != NULL && req.name.len != 0 && req.value.union_tag != UAVCAN_PROTOCOL_PARAM_VALUE_EMPTY) {
        /*
          this is a parameter set command. The implementation can
          either choose to store the value in a persistent manner
          immediately or can instead store it in memory and save to permanent storage on a
         */
        switch (p->type) {
        case UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE:
            p->value = req.value.integer_value;
            break;
        case UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE:
            p->value = req.value.real_value;
            break;
        default:
            return;
        }
    }

    /*
      for both set and get we reply with the current value
     */
    struct uavcan_protocol_param_GetSetResponse pkt;
    memset(&pkt, 0, sizeof(pkt));

    if (p != NULL) {
        pkt.value.union_tag = p->type;
        switch (p->type) {
        case UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE:
            pkt.value.integer_value = p->value;
            break;
        case UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE:
            pkt.value.real_value = p->value;
            break;
        default:
            return;
        }
        pkt.name.len = strlen(p->name);
        strcpy((char *)pkt.name.data, p->name);
    }

    uint8_t buffer[UAVCAN_PROTOCOL_PARAM_GETSET_RESPONSE_MAX_SIZE];
    uint16_t total_size = uavcan_protocol_param_GetSetResponse_encode(&pkt, buffer);

    canardRequestOrRespond(ins,
                           transfer->source_node_id,
                           UAVCAN_PROTOCOL_PARAM_GETSET_SIGNATURE,
                           UAVCAN_PROTOCOL_PARAM_GETSET_ID,
                           &transfer->transfer_id,
                           transfer->priority,
                           CanardResponse,
                           &buffer[0],
                           total_size);
}

/*
  handle parameter executeopcode request
 */
static void handle_param_ExecuteOpcode(CanardInstance* ins, CanardRxTransfer* transfer)
{
    struct uavcan_protocol_param_ExecuteOpcodeRequest req;
    if (uavcan_protocol_param_ExecuteOpcodeRequest_decode(transfer, &req)) {
        return;
    }
    if (req.opcode == UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_REQUEST_OPCODE_ERASE) {
        // here is where you would reset all parameters to defaults
    }
    if (req.opcode == UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_REQUEST_OPCODE_SAVE) {
        // here is where you would save all the changed parameters to permanent storage
    }

    struct uavcan_protocol_param_ExecuteOpcodeResponse pkt;
    memset(&pkt, 0, sizeof(pkt));

    pkt.ok = true;

    uint8_t buffer[UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_RESPONSE_MAX_SIZE];
    uint16_t total_size = uavcan_protocol_param_ExecuteOpcodeResponse_encode(&pkt, buffer);

    canardRequestOrRespond(ins,
                           transfer->source_node_id,
                           UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_SIGNATURE,
                           UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_ID,
                           &transfer->transfer_id,
                           transfer->priority,
                           CanardResponse,
                           &buffer[0],
                           total_size);
}

/*
  handle a GetNodeInfo request
*/
static void handle_GetNodeInfo(CanardInstance *ins, CanardRxTransfer *transfer)
{
    printf("GetNodeInfo request from %d\n", transfer->source_node_id);

    uint8_t buffer[UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_MAX_SIZE];
    struct uavcan_protocol_GetNodeInfoResponse pkt;

    memset(&pkt, 0, sizeof(pkt));

    node_status.uptime_sec = micros64() / 1000000ULL;
    pkt.status = node_status;

    // fill in your major and minor firmware version
    pkt.software_version.major = 1;
    pkt.software_version.minor = 2;
    pkt.software_version.optional_field_flags = 0;
    pkt.software_version.vcs_commit = 0; // should put git hash in here

    // should fill in hardware version
    pkt.hardware_version.major = 2;
    pkt.hardware_version.minor = 3;

    getUniqueID(pkt.hardware_version.unique_id);

    strncpy((char*)pkt.name.data, "ESCNode", sizeof(pkt.name.data));
    pkt.name.len = strnlen((char*)pkt.name.data, sizeof(pkt.name.data));

    uint16_t total_size = uavcan_protocol_GetNodeInfoResponse_encode(&pkt, buffer);

    canardRequestOrRespond(ins,
                           transfer->source_node_id,
                           UAVCAN_PROTOCOL_GETNODEINFO_SIGNATURE,
                           UAVCAN_PROTOCOL_GETNODEINFO_ID,
                           &transfer->transfer_id,
                           transfer->priority,
                           CanardResponse,
                           &buffer[0],
                           total_size);
}

/*
  handle a ESC RawCommand request
*/
static void handle_RawCommand(CanardInstance *ins, CanardRxTransfer *transfer)
{
    struct uavcan_equipment_esc_RawCommand cmd;
    if (uavcan_equipment_esc_RawCommand_decode(transfer, &cmd)) {
        return;
    }
    // remember the demand for the ESC status output
    const uint8_t num_throttles = MIN(cmd.cmd.len, NUM_ESCS);
    const uint64_t tnow = micros64();
    for (uint8_t i=0; i<num_throttles; i++) {
        // convert throttle to -1.0 to 1.0 range
        escs[i].throttle = cmd.cmd.data[i]/8192.0;
        escs[i].last_update_us = tnow;
    }
}

/*
  data for dynamic node allocation process
 */
static struct {
    uint32_t send_next_node_id_allocation_request_at_ms;
    uint32_t node_id_allocation_unique_id_offset;
} DNA;

/*
  handle a DNA allocation packet
 */
static void handle_DNA_Allocation(CanardInstance *ins, CanardRxTransfer *transfer)
{
    if (canardGetLocalNodeID(&canard) != CANARD_BROADCAST_NODE_ID) {
        // already allocated
        return;
    }

    // Rule C - updating the randomized time interval
    DNA.send_next_node_id_allocation_request_at_ms =
        millis32() + UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_MIN_REQUEST_PERIOD_MS +
        (random() % UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_MAX_FOLLOWUP_DELAY_MS);

    if (transfer->source_node_id == CANARD_BROADCAST_NODE_ID) {
        printf("Allocation request from another allocatee\n");
        DNA.node_id_allocation_unique_id_offset = 0;
        return;
    }

    // Copying the unique ID from the message
    struct uavcan_protocol_dynamic_node_id_Allocation msg;

    uavcan_protocol_dynamic_node_id_Allocation_decode(transfer, &msg);

    // Obtaining the local unique ID
    uint8_t my_unique_id[sizeof(msg.unique_id.data)];
    getUniqueID(my_unique_id);

    // Matching the received UID against the local one
    if (memcmp(msg.unique_id.data, my_unique_id, msg.unique_id.len) != 0) {
        printf("Mismatching allocation response\n");
        DNA.node_id_allocation_unique_id_offset = 0;
        // No match, return
        return;
    }

    if (msg.unique_id.len < sizeof(msg.unique_id.data)) {
        // The allocator has confirmed part of unique ID, switching to
        // the next stage and updating the timeout.
        DNA.node_id_allocation_unique_id_offset = msg.unique_id.len;
        DNA.send_next_node_id_allocation_request_at_ms -= UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_MIN_REQUEST_PERIOD_MS;

        printf("Matching allocation response: %d\n", msg.unique_id.len);
    } else {
        // Allocation complete - copying the allocated node ID from the message
        canardSetLocalNodeID(ins, msg.node_id);
        printf("Node ID allocated: %d\n", msg.node_id);
    }
}

/*
  ask for a dynamic node allocation
 */
static void request_DNA()
{
    const uint32_t now = millis32();
    static uint8_t node_id_allocation_transfer_id = 0;

    DNA.send_next_node_id_allocation_request_at_ms =
        now + UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_MIN_REQUEST_PERIOD_MS +
        (random() % UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_MAX_FOLLOWUP_DELAY_MS);

    // Structure of the request is documented in the DSDL definition
    // See http://uavcan.org/Specification/6._Application_level_functions/#dynamic-node-id-allocation
    uint8_t allocation_request[CANARD_CAN_FRAME_MAX_DATA_LEN - 1];
    allocation_request[0] = (uint8_t)(PREFERRED_NODE_ID << 1U);

    if (DNA.node_id_allocation_unique_id_offset == 0) {
        allocation_request[0] |= 1;     // First part of unique ID
    }

    uint8_t my_unique_id[16];
    getUniqueID(my_unique_id);

    static const uint8_t MaxLenOfUniqueIDInRequest = 6;
    uint8_t uid_size = (uint8_t)(16 - DNA.node_id_allocation_unique_id_offset);
    
    if (uid_size > MaxLenOfUniqueIDInRequest) {
        uid_size = MaxLenOfUniqueIDInRequest;
    }

    memmove(&allocation_request[1], &my_unique_id[DNA.node_id_allocation_unique_id_offset], uid_size);

    // Broadcasting the request
    const int16_t bcast_res = canardBroadcast(&canard,
                                              UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_SIGNATURE,
                                              UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_ID,
                                              &node_id_allocation_transfer_id,
                                              CANARD_TRANSFER_PRIORITY_LOW,
                                              &allocation_request[0],
                                              (uint16_t) (uid_size + 1));
    if (bcast_res < 0) {
        printf("Could not broadcast ID allocation req; error %d\n", bcast_res);
    }

    // Preparing for timeout; if response is received, this value will be updated from the callback.
    DNA.node_id_allocation_unique_id_offset = 0;
}


/*
 This callback is invoked by the library when a new message or request or response is received.
*/
static void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer)
{
    // switch on data type ID to pass to the right handler function
    if (transfer->transfer_type == CanardTransferTypeRequest) {
        // check if we want to handle a specific service request
        switch (transfer->data_type_id) {
        case UAVCAN_PROTOCOL_GETNODEINFO_ID: {
            handle_GetNodeInfo(ins, transfer);
            break;
        }
        case UAVCAN_PROTOCOL_PARAM_GETSET_ID: {
            handle_param_GetSet(ins, transfer);
            break;
        }
        case UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_ID: {
            handle_param_ExecuteOpcode(ins, transfer);
            break;
        }
        }
    }
    if (transfer->transfer_type == CanardTransferTypeBroadcast) {
        // check if we want to handle a specific broadcast message
        switch (transfer->data_type_id) {
        case UAVCAN_EQUIPMENT_ESC_RAWCOMMAND_ID: {
            handle_RawCommand(ins, transfer);
            break;
        }
        case UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_ID: {
            handle_DNA_Allocation(ins, transfer);
            break;
        }
        }
    }
}


/*
 This callback is invoked by the library when it detects beginning of a new transfer on the bus that can be received
 by the local node.
 If the callback returns true, the library will receive the transfer.
 If the callback returns false, the library will ignore the transfer.
 All transfers that are addressed to other nodes are always ignored.

 This function must fill in the out_data_type_signature to be the signature of the message.
 */
static bool shouldAcceptTransfer(const CanardInstance *ins,
                                 uint64_t *out_data_type_signature,
                                 uint16_t data_type_id,
                                 CanardTransferType transfer_type,
                                 uint8_t source_node_id)
{
    if (transfer_type == CanardTransferTypeRequest) {
        // check if we want to handle a specific service request
        switch (data_type_id) {
        case UAVCAN_PROTOCOL_GETNODEINFO_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_GETNODEINFO_REQUEST_SIGNATURE;
            return true;
        }
        case UAVCAN_PROTOCOL_PARAM_GETSET_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_PARAM_GETSET_SIGNATURE;
            return true;
        }
        case UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_SIGNATURE;
            return true;
        }
        }
    }
    if (transfer_type == CanardTransferTypeBroadcast) {
        // see if we want to handle a specific broadcast packet
        switch (data_type_id) {
        case UAVCAN_EQUIPMENT_ESC_RAWCOMMAND_ID: {
            *out_data_type_signature = UAVCAN_EQUIPMENT_ESC_RAWCOMMAND_SIGNATURE;
            return true;
        }
        case UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_SIGNATURE;
            return true;
        }
        }
    }
    // we don't want any other messages
    return false;
}

/*
  send the 1Hz NodeStatus message. This is what allows a node to show
  up in the DroneCAN GUI tool and in the flight controller logs
 */
static void send_NodeStatus(void)
{
    uint8_t buffer[UAVCAN_PROTOCOL_GETNODEINFO_RESPONSE_MAX_SIZE];

    node_status.uptime_sec = micros64() / 1000000ULL;
    node_status.health = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_OK;
    node_status.mode = UAVCAN_PROTOCOL_NODESTATUS_MODE_OPERATIONAL;
    node_status.sub_mode = 0;
    // put whatever you like in here for display in GUI
    node_status.vendor_specific_status_code = 1234;

    uint32_t len = uavcan_protocol_NodeStatus_encode(&node_status, buffer);

    // we need a static variable for the transfer ID. This is
    // incremeneted on each transfer, allowing for detection of packet
    // loss
    static uint8_t transfer_id;

    canardBroadcast(&canard,
                    UAVCAN_PROTOCOL_NODESTATUS_SIGNATURE,
                    UAVCAN_PROTOCOL_NODESTATUS_ID,
                    &transfer_id,
                    CANARD_TRANSFER_PRIORITY_LOW,
                    buffer,
                    len);
}

/*
  This function is called at 1 Hz rate from the main loop.
*/
static void process1HzTasks(uint64_t timestamp_usec)
{
    /*
      Purge transfers that are no longer transmitted. This can free up some memory
    */
    canardCleanupStaleTransfers(&canard, timestamp_usec);

    /*
      Transmit the node status message
    */
    send_NodeStatus();
}

/*
  send ESC status at 50Hz
*/
static void send_ESCStatus(void)
{
    // send a separate status packet for each ESC
    for (uint8_t i=0; i<NUM_ESCS; i++) {
        struct uavcan_equipment_esc_Status pkt;
        memset(&pkt, 0, sizeof(pkt));
        uint8_t buffer[UAVCAN_EQUIPMENT_ESC_STATUS_MAX_SIZE];

        // make up some synthetic status data
        pkt.error_count = 0;
        pkt.voltage = 16.8 - 2.0 * escs[i].throttle;
        pkt.current = 20 * escs[i].throttle;
        pkt.temperature = C_TO_KELVIN(25.0);
        pkt.rpm = 10000 * escs[i].throttle;
        pkt.power_rating_pct = 100.0 * escs[i].throttle;

        uint32_t len = uavcan_equipment_esc_Status_encode(&pkt, buffer);

        // we need a static variable for the transfer ID. This is
        // incremeneted on each transfer, allowing for detection of packet
        // loss
        static uint8_t transfer_id;

        canardBroadcast(&canard,
                        UAVCAN_EQUIPMENT_ESC_STATUS_SIGNATURE,
                        UAVCAN_EQUIPMENT_ESC_STATUS_ID,
                        &transfer_id,
                        CANARD_TRANSFER_PRIORITY_LOW,
                        buffer,
                        len);
    }
}


/*
  Transmits all frames from the TX queue, receives up to one frame.
*/
static void processTxRxOnce(SocketCANInstance *socketcan, int32_t timeout_msec)
{
    // Transmitting
    for (const CanardCANFrame* txf = NULL; (txf = canardPeekTxQueue(&canard)) != NULL;) {
        const int16_t tx_res = socketcanTransmit(socketcan, txf, 0);
        if (tx_res < 0) {         // Failure - drop the frame
            canardPopTxQueue(&canard);
        }
        else if (tx_res > 0)    // Success - just drop the frame
        {
            canardPopTxQueue(&canard);
        }
        else                    // Timeout - just exit and try again later
        {
            break;
        }
    }

    // Receiving
    CanardCANFrame rx_frame;

    const uint64_t timestamp = micros64();
    const int16_t rx_res = socketcanReceive(socketcan, &rx_frame, timeout_msec);
    if (rx_res < 0) {
        (void)fprintf(stderr, "Receive error %d, errno '%s'\n", rx_res, strerror(errno));
    }
    else if (rx_res > 0)        // Success - process the frame
    {
        canardHandleRxFrame(&canard, &rx_frame, timestamp);
    }
}

/*
  main program
 */
int main(int argc, char** argv)
{
    if (argc < 2) {
        (void)fprintf(stderr,
                      "Usage:\n"
                      "\t%s <can iface name>\n",
                      argv[0]);
        return 1;
    }

    /*
     * Initializing the CAN backend driver; in this example we're using SocketCAN
     */
    SocketCANInstance socketcan;
    const char* const can_iface_name = argv[1];
    int16_t res = socketcanInit(&socketcan, can_iface_name);
    if (res < 0) {
        (void)fprintf(stderr, "Failed to open CAN iface '%s'\n", can_iface_name);
        return 1;
    }

    /*
     Initializing the Libcanard instance.
     */
    canardInit(&canard,
               memory_pool,
               sizeof(memory_pool),
               onTransferReceived,
               shouldAcceptTransfer,
               NULL);

    if (MY_NODE_ID > 0) {
        canardSetLocalNodeID(&canard, MY_NODE_ID);
    } else {
        printf("Waiting for DNA node allocation\n");
    }

    /*
      Run the main loop.
     */
    uint64_t next_1hz_service_at = micros64();
    uint64_t next_50hz_service_at = micros64();

    while (true) {
        processTxRxOnce(&socketcan, 10);

        const uint64_t ts = micros64();

        if (canardGetLocalNodeID(&canard) == CANARD_BROADCAST_NODE_ID) {
            // waiting for DNA
        }

        // see if we are still doing DNA
        if (canardGetLocalNodeID(&canard) == CANARD_BROADCAST_NODE_ID) {
            // we're still waiting for a DNA allocation of our node ID
            if (millis32() > DNA.send_next_node_id_allocation_request_at_ms) {
                request_DNA();
            }
            continue;
        }

        if (ts >= next_1hz_service_at) {
            next_1hz_service_at += 1000000ULL;
            process1HzTasks(ts);
        }
        if (ts >= next_50hz_service_at) {
            next_50hz_service_at += 1000000ULL/50U;
            send_ESCStatus();
        }
    }

    return 0;
}
