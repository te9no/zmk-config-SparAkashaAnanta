// SPDX-License-Identifier: MIT
#include <pb_decode.h>
#include <zmk/studio/custom.h>
#include "modules.pb.h"
#include "modules_profile.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
static struct zmk_rpc_custom_subsystem_meta meta = {.security = ZMK_STUDIO_RPC_HANDLER_UNSECURED};
ZMK_RPC_CUSTOM_SUBSYSTEM(saa__modules, &meta, handle);
ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER(saa__modules, cagime_Response);

static bool handle(const zmk_custom_CallRequest *raw, pb_callback_t *encode) {
    cagime_Response *resp = ZMK_RPC_CUSTOM_SUBSYSTEM_RESPONSE_BUFFER_ALLOCATE(saa__modules, encode);
    *resp = (cagime_Response)cagime_Response_init_zero;
    cagime_Request req = cagime_Request_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(raw->payload.bytes, raw->payload.size);
    uint8_t modes[2];
    bool decoded = pb_decode(&stream, cagime_Request_fields, &req);
    bool write = req.which_operation == cagime_Request_set_tag;
    if (!decoded || (!write && req.which_operation != cagime_Request_get_tag) ||
        (write && (req.operation.set.lower > UINT8_MAX || req.operation.set.inner > UINT8_MAX))) {
        resp->error = -EINVAL;
        return true;
    }
    if (write) {
        modes[0] = req.operation.set.lower;
        modes[1] = req.operation.set.inner;
    }
    struct zeroshift_state state;

    resp->error = !req.right ? saa_remote_profile(write ? modes : NULL, &state)
                            : zeroshift_profile(write ? modes : NULL, &state);
    if (!resp->error) {
        resp->has_supported = true;
        resp->supported = (cagime_Modes){(BIT(6) - 1) | BIT(7) | BIT(8), BIT(0) | BIT(6)};
        resp->has_saved = resp->has_active = true;
        resp->saved = (cagime_Modes){state.saved[0], state.saved[1]};
        resp->active = (cagime_Modes){state.active[0], state.active[1]};
    }
    return true;
}
