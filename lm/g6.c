#include "g6.h"

#include <string.h>

#include "callbacks.h"
#include "lm.h"
#include "platform.h"

uint8_t lm_g6_node_id = LM_G6_NODE_UNSET;
// lm_g6_sum will overflow, this is intentional
uint8_t lm_g6_sum = 0;
uint8_t lm_g6_ibuf[64];
uint8_t lm_g6_obuf[64];
uint8_t lm_g6_obuf_ptr = 0;

#define lm_g6_write(data)                    \
    do {                                     \
        lm_g6_obuf[lm_g6_obuf_ptr++] = data; \
        lm_g6_sum += data;                   \
    } while (0)

typedef enum {
    lm_g6_state_waiting = 0,
    lm_g6_state_got_sync,
    lm_g6_state_read_nbytes,
    lm_g6_state_overflow,
    lm_g6_state_read_body,
    lm_g6_state_sum,
} lm_g6_state_t;
lm_g6_state_t lm_g6_state;

uint8_t lm_g6_read() {
    while (!lm_platform_serial_available())
        ;
    uint8_t data = lm_platform_serial_read_one();
    if (data == LM_G6_SYNC) {
        lm_g6_state = lm_g6_state_got_sync;
        return data;
    }

    if (data == LM_G6_MARK) data = lm_platform_serial_read_one() + 1;
    lm_g6_sum += data;
    return data;
}

void lm_g6_write_hw(uint8_t data) {
    if (data == LM_G6_MARK || data == LM_G6_SYNC) {
        lm_platform_serial_write_one(LM_G6_MARK);
        data--;
    }
    lm_platform_serial_write_one(data);
}

void lm_g6_tick(void) {
    uint8_t dest_node;
    uint8_t nbytes;
    uint8_t read_ptr;
    uint8_t set_low_sense = 0;

    lm_platform_serial_select(lm_select_rx);

    lm_g6_state = lm_g6_state_waiting;
    lm_g6_sum = 0;

    // On paper using an FSM here may seem contrived. As per spec, we must
    // treat any E0 byte as the start of a new packet, and discard existing
    // processing we had done. The cleanest way to do this is to create a
    // 1-to-1 correlation between FSM ticks, and bytes read, such that any E0
    // byte will return us to the lm_g6_state_got_sync state.

    while (1) {
        switch (lm_g6_state) {
            case lm_g6_state_waiting:
                if (!lm_platform_serial_available()) return;
                if (lm_platform_serial_read_one() != LM_G6_SYNC) return;
                lm_g6_state = lm_g6_state_got_sync;
                break;

            case lm_g6_state_got_sync:
                lm_g6_state = lm_g6_state_read_nbytes;
                dest_node = lm_g6_read();
                if (lm_g6_state == lm_g6_state_got_sync) break;

                if (dest_node == LM_G6_NODE_MASTER) return;
                if (dest_node == LM_G6_NODE_BROADCAST) break;
                if (dest_node != lm_g6_node_id) return;

                break;

            case lm_g6_state_read_nbytes:
                nbytes = lm_g6_read();
                if (lm_g6_state == lm_g6_state_got_sync) break;

                if (nbytes - 1 > sizeof lm_g6_ibuf) {
                    if (dest_node == LM_G6_NODE_BROADCAST)
                        return;
                    else
                        lm_g6_state = lm_g6_state_overflow;
                    break;
                }

                read_ptr = 0;
                lm_g6_state = lm_g6_state_read_body;
                break;

            case lm_g6_state_overflow:
                lm_platform_serial_select(lm_select_tx);

                lm_platform_serial_write_one(LM_G6_SYNC);
                lm_platform_serial_write_one(LM_G6_NODE_MASTER);
                // None of these are E0 or D0 so we can save a few cycles buy
                // not using lm_g6_write_hw
                lm_platform_serial_write_one(2);
                lm_platform_serial_write_one(LM_G6_STATUS_OVERFLOW);
                lm_platform_serial_write_one(LM_G6_NODE_MASTER + 2 + LM_G6_STATUS_OVERFLOW);

                lm_platform_serial_select(lm_select_rx);
                return;

            case lm_g6_state_read_body:
                if (read_ptr + 1 < nbytes - 1)  // nbytes includes sum
                    lm_g6_ibuf[read_ptr++] = lm_g6_read();
                else
                    lm_g6_state = lm_g6_state_sum;
                break;

            case lm_g6_state_sum: {
                uint8_t sum = lm_g6_sum;
                uint8_t rsum = lm_g6_read();
                if (lm_g6_state == lm_g6_state_got_sync) break;

                if (sum != rsum) {
                    lm_platform_serial_select(lm_select_tx);

                    // Corrupted packet!
                    lm_platform_serial_write_one(LM_G6_SYNC);
                    lm_platform_serial_write_one(LM_G6_NODE_MASTER);
                    // lm_platform_serial_write_one(2);
                    // lm_platform_serial_write_one(LM_G6_STATUS_SUM);
                    // lm_g6_sum = LM_G6_NODE_MASTER + 2 + LM_G6_STATUS_SUM;

                    lm_platform_serial_write_one(1 + read_ptr + 4);
                    lm_platform_serial_write_one(LM_G6_STATUS_SUM);

                    lm_platform_serial_write_one(read_ptr);
                    for (uint8_t i = 0; i < read_ptr; i++)
                        lm_platform_serial_write_one(lm_g6_ibuf[i]);
                    lm_platform_serial_write_one(0x00);
                    lm_platform_serial_write_one(sum);
                    lm_platform_serial_write_one(rsum);
                    lm_g6_sum = LM_G6_NODE_MASTER + sum + rsum + 4 + LM_G6_STATUS_SUM;

                    lm_platform_serial_write_one(lm_g6_sum);

                    lm_platform_serial_select(lm_select_rx);
                    return;
                }
                goto lm_g6_process_cmd;
            } break;
        }
    }
lm_g6_process_cmd:;
    // We've finished reading the entire packet, so don't have any chances to
    // see another E0. We can return to normal flat programming (yay!).

    uint8_t status = LM_G6_STATUS_OK;
    lm_g6_obuf_ptr = lm_g6_sum = 0;

    for (uint8_t ptr = 0; ptr < read_ptr;) {
        switch (lm_g6_ibuf[ptr++]) {
            // Transport configuration commands
            case LM_G6_CMD_RESET:

                if (lm_g6_ibuf[ptr++] == LM_G6_CMD_RESET_CHECK) {
                    lm_g6_node_id = LM_G6_NODE_UNSET;
                    lm_platform_sense_set(lm_sense_high);
                    lm_platform_reset();
                }
                // If we see a reset, don't process anything else!
                return;
            case LM_G6_CMD_ASSIGN_ADDR:
                // Not for us!
                if (lm_platform_sense_get() == lm_sense_high) return;
                // We already got our address
                if (lm_g6_node_id != LM_G6_NODE_UNSET) break;

                lm_g6_node_id = lm_g6_ibuf[ptr++];
                set_low_sense = 1;

                lm_g6_write(LM_G6_REPORT_OK);
                break;
            // Configuration queries
            case LM_G6_CMD_READ_ID: {
                uint8_t len = strlen(lm_platform_name);

                lm_platform_serial_select(lm_select_tx);

                // Rather than using lm_g6_write, which writes to a buffer, we
                // directly write out our output. This means we can shrink the
                // buffer size substantially, as this is the only significantly
                // large packet we support.
                lm_platform_serial_write_one(LM_G6_SYNC);
                lm_platform_serial_write_one(LM_G6_NODE_MASTER);
                lm_g6_write_hw(len + 4);
                lm_g6_write_hw(status);
                lm_g6_write_hw(LM_G6_REPORT_OK);
                lm_g6_sum = LM_G6_NODE_MASTER + len + 4 + status + LM_G6_REPORT_OK;

                for (int i = 0; i <= len; i++) {
                    lm_g6_write_hw(lm_platform_name[i]);
                    lm_g6_sum += lm_platform_name[i];
                }
                lm_g6_write_hw(lm_g6_sum);

                lm_platform_serial_select(lm_select_rx);
                // ! WARNING: Processing a read ID will immediatly terminate the processing of other
                // ! packets. Read ID requests should always be sent on their own.
                return;
            } break;
            case LM_G6_CMD_GET_CMD_VERSION:
                lm_g6_write(LM_G6_REPORT_OK);
                lm_g6_write(LM_G6_VERSION_CMD);
                break;
            case LM_G6_CMD_GET_G6_VERSION:
                lm_g6_write(LM_G6_REPORT_OK);
                lm_g6_write(LM_G6_VERSION_G6);
                break;
            case LM_G6_CMD_GET_COMM_VERSION:
                lm_g6_write(LM_G6_REPORT_OK);
                lm_g6_write(LM_G6_VERSION_COMM);
                break;

            case LM_G6_CMD_GET_FEATURES:
                lm_g6_write(LM_G6_REPORT_OK);
                lm_platform_features();
                lm_g6_write(LM_G6_FEATURE_EOF);
                lm_g6_write(LM_G6_FEATURE_EOF);
                lm_g6_write(LM_G6_FEATURE_EOF);
                break;

            // Main graphene commands
            case LM_G6_CMD_GRAPHENE_PING:
                // Used to measure bus speed
                lm_g6_write(LM_G6_REPORT_OK);
                break;
            case LM_G6_CMD_GRAPHENE_GET_SENSE:
                lm_g6_write(LM_G6_REPORT_OK);
                lm_g6_write(lm_platform_sense_get() == lm_sense_high ? 1 : 0);
                break;

            case LM_G6_CMD_GRAPHENE_CONTROL:
                if (lm_do_control(*((uint32_t*)&(lm_g6_ibuf[ptr])), lm_g6_ibuf[ptr + 4],
                                  lm_g6_ibuf[ptr + 5], lm_g6_ibuf[ptr + 6]))
                    lm_g6_write(LM_G6_REPORT_OK);
                else
                    lm_g6_write(LM_G6_REPORT_PARAM_INVALID);
                ptr += 6;
                break;

            // Action commands have no response to avoid clugging the bus!
            case LM_G6_CMD_GRAPHENE_DOWN:
                lm_do_note_down(*((uint32_t*)&(lm_g6_ibuf[ptr])), lm_g6_ibuf[ptr + 4],
                                lm_g6_ibuf[ptr + 5], lm_g6_ibuf[ptr + 6]);
                ptr += 6;
                break;
            case LM_G6_CMD_GRAPHENE_UP:
                lm_do_note_up(*((uint32_t*)&(lm_g6_ibuf[ptr])), lm_g6_ibuf[ptr + 4],
                              lm_g6_ibuf[ptr + 5], lm_g6_ibuf[ptr + 6]);
                ptr += 6;
                break;
            case LM_G6_CMD_GRAPHENE_LIGHT:
                lm_do_light(*((uint32_t*)&(lm_g6_ibuf[ptr])), lm_g6_ibuf[ptr + 4],
                            lm_g6_ibuf[ptr + 5], lm_g6_ibuf[ptr + 6]);
                ptr += 6;
                break;

            // Unknown command
            default:
                // Don't respond to broadcast packets!
                if (dest_node == LM_G6_NODE_BROADCAST) return;
                lm_g6_write(LM_G6_STATUS_UKCOM);
                break;
        }
    }

    // We had no response to send!
    if (lm_g6_obuf_ptr == 0) return;

    // delay(5);

    lm_platform_serial_select(lm_select_tx);

    lm_platform_serial_write_one(LM_G6_SYNC);
    lm_platform_serial_write_one(LM_G6_NODE_MASTER);
    lm_g6_write_hw(lm_g6_obuf_ptr + 2);
    lm_g6_write_hw(status);
    for (uint8_t i = 0; i < lm_g6_obuf_ptr; i++) lm_g6_write_hw(lm_g6_obuf[i]);
    lm_g6_write_hw(LM_G6_NODE_MASTER + lm_g6_obuf_ptr + 2 + status + lm_g6_sum);

    lm_platform_serial_select(lm_select_rx);

    if (set_low_sense) lm_platform_sense_set(lm_sense_low);
}
