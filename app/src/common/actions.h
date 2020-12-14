/*******************************************************************************
*   (c) 2019 Zondax GmbH
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/
#pragma once

#include <stdint.h>
#include "crypto.h"
#include "tx.h"
#include "apdu_codes.h"
#include <os_io_seproxyhal.h>
#include "coin.h"
#include "app_main.h"
#include "../nvdata.h"
#include "view.h"

typedef struct {
    address_kind_e kind;
    uint8_t len;
} address_state_t;

extern address_state_t address_state;

typedef struct {
    key_type_e kind;
    uint8_t len;
} key_state_t;

extern key_state_t key_state;

__Z_INLINE zxerr_t init_tx() {
    tx_reset_state();
    const uint8_t *message = tx_get_buffer() + CRYPTO_BLOB_SKIP_BYTES;
    const uint16_t messageLength = tx_get_buffer_length() - CRYPTO_BLOB_SKIP_BYTES;
    zxerr_t err;
    err = crypto_extracttx_sapling(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }
    err = crypto_hash_messagebuffer(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    return err;

}

__Z_INLINE uint8_t key_exchange() {
    tx_reset_state();

    const uint8_t *message = tx_get_buffer() + CRYPTO_BLOB_SKIP_BYTES;
    const uint16_t messageLength = tx_get_buffer_length() - CRYPTO_BLOB_SKIP_BYTES;
    const uint8_t replyLen = crypto_key_exchange(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);

    return replyLen;

}

__Z_INLINE zxerr_t check_and_sign_tx() {
    // Take "ownership" of the memory used by the transaction parser
    tx_reset_state();
    const uint8_t *message = tx_get_buffer() + CRYPTO_BLOB_SKIP_BYTES;
    const uint16_t messageLength = tx_get_buffer_length() - CRYPTO_BLOB_SKIP_BYTES;
    zxerr_t err;
    err = crypto_check_prevouts(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }

    err = crypto_check_sequence(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }

    err = crypto_check_outputs(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }

    err = crypto_check_joinsplits(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }
    //todo: the valuebalance sometimes fails, maybe bug in emulator? Add check later when it is fixed.
    err = crypto_check_valuebalance(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    /*
    if(err != zxerr_ok){
        return 0;
    }
    */
    err = crypto_checkspend_sapling(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }

    err = crypto_checkoutput_sapling(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }

    err = crypto_checkencryptions_sapling(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }

    set_state(STATE_VERIFIED_ALL_TXDATA);

    err = crypto_sign_and_check_transparent(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }
    err = crypto_signspends_sapling(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if (err != zxerr_ok){
        return err;
    }

    err = crypto_hash_messagebuffer(G_io_apdu_buffer, IO_APDU_BUFFER_SIZE - 3, message, messageLength);
    if(err != zxerr_ok){
        return err;
    }

    return zxerr_ok;
}

__Z_INLINE void app_reject() {
    tx_reset_state();
    transaction_reset();
    view_idle_show(0, NULL);
    set_code(G_io_apdu_buffer, 0, APDU_CODE_COMMAND_NOT_ALLOWED);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

__Z_INLINE void app_reply_key() {
    set_code(G_io_apdu_buffer, key_state.len, APDU_CODE_OK);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, key_state.len + 2);
}

__Z_INLINE void app_reply_address() {
    set_code(G_io_apdu_buffer, address_state.len, APDU_CODE_OK);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, address_state.len + 2);
}

__Z_INLINE void app_reply_error() {
    view_idle_show(0, NULL);
    set_code(G_io_apdu_buffer, 0, APDU_CODE_DATA_INVALID);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

__Z_INLINE void app_reply_hash() {
    if(get_state() == STATE_PROCESSED_INPUTS) {
        view_message_show("Zcash", "Step [1/5]");
    }else{
        view_message_show("Zcash", "Step [2/5]");
    }
    UX_WAIT_DISPLAYED();
    set_code(G_io_apdu_buffer, 32, APDU_CODE_OK);
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 32 + 2);
}
