#include <stdint.h>

extern uint8_t lm_g6_node_id;

void lm_g6_tick(void);
void lm_g6_write(uint8_t data);

#define LM_G6_SYNC 0xE0
#define LM_G6_MARK 0xD0

#define LM_G6_NODE_MASTER 0x00
#define LM_G6_NODE_BROADCAST 0xFF
#define LM_G6_NODE_UNSET LM_G6_NODE_BROADCAST

// These values are inherited from the JAMMA spec
#define LM_G6_VERSION_CMD 0x13   // 1.3
#define LM_G6_VERSION_G6 0x20    // 2.0
#define LM_G6_VERSION_COMM 0x10  // 1.0

#define LM_G6_STATUS_OK 0x01
#define LM_G6_STATUS_UKCOM 0x02
#define LM_G6_STATUS_SUM 0x03
#define LM_G6_STATUS_OVERFLOW 0x04
#define LM_G6_STATUS_UNKNOWN 0xFF

#define LM_G6_REPORT_OK 0x01
#define LM_G6_REPORT_PARAM_NODATA 0x02
#define LM_G6_REPORT_PARAM_INVALID 0x03
#define LM_G6_REPORT_BUSY 0x04

#define LM_G6_FEATURE_PAD 0x00
#define LM_G6_FEATURE_EOF 0x00
#define LM_G6_FEATURE_NOTE_CHANNEL 0x01
#define LM_G6_FEATURE_LIGHT_CHANNEL 0x02
#define LM_G6_FEATURE_CONTROL_CHANNEL 0x03
#define LM_G6_FEATURE_OFFSET 0x04

#define LM_G6_CMD_RESET_CHECK 0xD9

// Mandatory G6 commands
#define LM_G6_CMD_RESET 0xF0
#define LM_G6_CMD_ASSIGN_ADDR 0xF1

#define LM_G6_CMD_READ_ID 0x10
#define LM_G6_CMD_GET_CMD_VERSION 0x11
#define LM_G6_CMD_GET_G6_VERSION 0x12
#define LM_G6_CMD_GET_COMM_VERSION 0x13
#define LM_G6_CMD_GET_FEATURES 0x14

#define LM_G6_CMD_REQUEST_RETRANSMIT 0x2F

// Graphene control and debug commands
#define LM_G6_CMD_GRAPHENE_PING 0x60
#define LM_G6_CMD_GRAPHENE_GET_SENSE 0x61
#define LM_G6_CMD_GRAPHENE_INCR 0x62
#define LM_G6_CMD_GRAPHENE_CNTR 0x63
// Graphene main commands
#define LM_G6_CMD_GRAPHENE_DOWN 0x70
#define LM_G6_CMD_GRAPHENE_UP 0x71
#define LM_G6_CMD_GRAPHENE_LIGHT 0x72
#define LM_G6_CMD_GRAPHENE_CONTROL 0x73
