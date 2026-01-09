/**
 * @file magic_lmi_utils.c
 * @brief LMI 辅助函数实现
 */

#include "magic_lmi.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

/* ========================================================================
 * 枚举转字符串函数实现
 * ======================================================================== */

const char* lmi_link_type_to_string(lmi_link_type_e type) {
    switch (type) {
        case LMI_LINK_TYPE_SATELLITE:   return "SATELLITE";
        case LMI_LINK_TYPE_CELLULAR:    return "CELLULAR";
        case LMI_LINK_TYPE_GATELINK:    return "GATELINK";
        case LMI_LINK_TYPE_VHF:         return "VHF";
        case LMI_LINK_TYPE_HFDL:        return "HFDL";
        default:                        return "UNKNOWN";
    }
}

const char* lmi_link_state_to_string(lmi_link_state_e state) {
    switch (state) {
        case LMI_STATE_UNAVAILABLE:     return "UNAVAILABLE";
        case LMI_STATE_AVAILABLE:       return "AVAILABLE";
        case LMI_STATE_ACTIVATING:      return "ACTIVATING";
        case LMI_STATE_ACTIVE:          return "ACTIVE";
        case LMI_STATE_SUSPENDING:      return "SUSPENDING";
        case LMI_STATE_SUSPENDED:       return "SUSPENDED";
        case LMI_STATE_ERROR:           return "ERROR";
        default:                        return "UNKNOWN";
    }
}

const char* lmi_event_type_to_string(lmi_event_type_e type) {
    switch (type) {
        case LMI_EVENT_LINK_UP:             return "LINK_UP";
        case LMI_EVENT_LINK_DOWN:           return "LINK_DOWN";
        case LMI_EVENT_CAPABILITY_CHANGE:   return "CAPABILITY_CHANGE";
        case LMI_EVENT_HANDOVER_RECOMMEND:  return "HANDOVER_RECOMMEND";
        case LMI_EVENT_RESOURCE_EXHAUSTED:  return "RESOURCE_EXHAUSTED";
        case LMI_EVENT_ERROR:               return "ERROR";
        default:                            return "UNKNOWN";
    }
}

const char* lmi_error_to_string(int error_code) {
    switch (error_code) {
        case LMI_SUCCESS:               return "Success";
        case LMI_ERR_INVALID_PARAM:     return "Invalid parameter";
        case LMI_ERR_NOT_INITIALIZED:   return "DLM not initialized";
        case LMI_ERR_LINK_NOT_FOUND:    return "Link not found";
        case LMI_ERR_RESOURCE_BUSY:     return "Resource busy";
        case LMI_ERR_RESOURCE_UNAVAIL:  return "Resource unavailable";
        case LMI_ERR_TIMEOUT:           return "Operation timeout";
        case LMI_ERR_HARDWARE_FAILURE:  return "Hardware failure";
        case LMI_ERR_INSUFFICIENT_BW:   return "Insufficient bandwidth";
        case LMI_ERR_AUTH_FAILED:       return "Authentication failed";
        case LMI_ERR_NETWORK_ERROR:     return "Network error";
        case LMI_ERR_NOT_SUPPORTED:     return "Feature not supported";
        case LMI_ERR_INTERNAL:          return "Internal error";
        default:                        return "Unknown error";
    }
}

/* ========================================================================
 * 会话 ID 生成
 * ======================================================================== */

static uint32_t session_id_counter = 0;

lmi_session_id_t lmi_generate_session_id(void) {
    return __sync_add_and_fetch(&session_id_counter, 1);
}

/* ========================================================================
 * 时间戳获取
 * ======================================================================== */

uint64_t lmi_get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
