/**
 * @file magic_ipc_utils.c
 * @brief IPC Protocol Utility Functions
 */

#include "magic_ipc_protocol.h"
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

static uint32_t g_sequence_counter = 0;

/*===========================================================================
 * Message Header Initialization
 *===========================================================================*/

void ipc_init_header(ipc_msg_header_t *header, uint16_t msg_type, 
                     uint16_t msg_length, uint32_t link_id)
{
    memset(header, 0, sizeof(ipc_msg_header_t));
    header->msg_type = msg_type;
    header->msg_length = msg_length;
    header->sequence_num = __sync_fetch_and_add(&g_sequence_counter, 1);
    header->timestamp = (uint32_t)time(NULL);
    header->link_id = link_id;
}

/*===========================================================================
 * Send Message
 *===========================================================================*/

int ipc_send_message(int sockfd, const void *msg, size_t msg_len)
{
    const char *buf = (const char *)msg;
    size_t total_sent = 0;
    
    while (total_sent < msg_len) {
        ssize_t sent = send(sockfd, buf + total_sent, msg_len - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (sent == 0) return -1;  // Connection closed
        total_sent += sent;
    }
    
    return 0;
}

/*===========================================================================
 * Receive Message
 *===========================================================================*/

int ipc_recv_message(int sockfd, void *msg_buf, size_t buf_size)
{
    ipc_msg_header_t *header = (ipc_msg_header_t *)msg_buf;
    
    // 1. 先接收消息头
    ssize_t received = recv(sockfd, msg_buf, sizeof(ipc_msg_header_t), MSG_WAITALL);
    if (received < 0) {
        if (errno == EINTR) return 0;
        return -1;
    }
    if (received == 0) return -1;  // Connection closed
    if (received != sizeof(ipc_msg_header_t)) return -1;
    
    // 2. 验证消息长度
    if (header->msg_length > buf_size) {
        return -1;
    }
    
    // 3. 接收剩余的消息体
    size_t remaining = header->msg_length - sizeof(ipc_msg_header_t);
    if (remaining > 0) {
        char *body_buf = (char *)msg_buf + sizeof(ipc_msg_header_t);
        received = recv(sockfd, body_buf, remaining, MSG_WAITALL);
        if (received != (ssize_t)remaining) {
            return -1;
        }
    }
    
    return header->msg_length;
}

/*===========================================================================
 * String Conversion Functions
 *===========================================================================*/

const char* ipc_msg_type_to_string(ipc_msg_type_t msg_type)
{
    switch (msg_type) {
        case MSG_REGISTER_REQUEST:      return "REGISTER_REQUEST";
        case MSG_REGISTER_RESPONSE:     return "REGISTER_RESPONSE";
        case MSG_HEARTBEAT:             return "HEARTBEAT";
        case MSG_HEARTBEAT_ACK:         return "HEARTBEAT_ACK";
        case MSG_UNREGISTER:            return "UNREGISTER";
        case MSG_LINK_UP:               return "LINK_UP";
        case MSG_LINK_DOWN:             return "LINK_DOWN";
        case MSG_LINK_DEGRADED:         return "LINK_DEGRADED";
        case MSG_LINK_RESTORED:         return "LINK_RESTORED";
        case MSG_CAPABILITY_CHANGE:     return "CAPABILITY_CHANGE";
        case MSG_ALLOCATE_REQUEST:      return "ALLOCATE_REQUEST";
        case MSG_ALLOCATE_RESPONSE:     return "ALLOCATE_RESPONSE";
        case MSG_RELEASE_REQUEST:       return "RELEASE_REQUEST";
        case MSG_RELEASE_RESPONSE:      return "RELEASE_RESPONSE";
        case MSG_SUSPEND_REQUEST:       return "SUSPEND_REQUEST";
        case MSG_RESUME_REQUEST:        return "RESUME_REQUEST";
        case MSG_STATS_REQUEST:         return "STATS_REQUEST";
        case MSG_STATS_RESPONSE:        return "STATS_RESPONSE";
        case MSG_ERROR:                 return "ERROR";
        default:                        return "UNKNOWN";
    }
}

const char* ipc_link_type_to_string(ipc_link_type_t link_type)
{
    switch (link_type) {
        case LINK_TYPE_SATCOM:      return "SATCOM";
        case LINK_TYPE_CELLULAR:    return "CELLULAR";
        case LINK_TYPE_WIFI:        return "WIFI";
        default:                    return "UNKNOWN";
    }
}

const char* ipc_link_state_to_string(ipc_link_state_t state)
{
    switch (state) {
        case LINK_STATE_UNAVAILABLE:    return "UNAVAILABLE";
        case LINK_STATE_AVAILABLE:      return "AVAILABLE";
        case LINK_STATE_ACTIVATING:     return "ACTIVATING";
        case LINK_STATE_ACTIVE:         return "ACTIVE";
        case LINK_STATE_SUSPENDED:      return "SUSPENDED";
        case LINK_STATE_ERROR:          return "ERROR";
        default:                        return "UNKNOWN";
    }
}
