/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							*
*													*
* Copyright (c) 2013, WIDE Project and NICT								*
* All rights reserved.											*
* 													*
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:						*
* 													*
* * Redistributions of source code must retain the above 						*
*   copyright notice, this list of conditions and the 							*
*   following disclaimer.										*
*    													*
* * Redistributions in binary form must reproduce the above 						*
*   copyright notice, this list of conditions and the 							*
*   following disclaimer in the documentation and/or other						*
*   materials provided with the distribution.								*
* 													*
* * Neither the name of the WIDE Project or NICT nor the 						*
*   names of its contributors may be used to endorse or 						*
*   promote products derived from this software without 						*
*   specific prior written permission of WIDE Project and 						*
*   NICT.												*
* 													*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 	*
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 	*
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.								*
*********************************************************************************************************/

/* SCTP stub functions for when SCTP is disabled */

#include "fdcore-internal.h"

/* Stub implementations for SCTP functions when SCTP is disabled */

int fd_sctp_client(int *sock, int no_ip6, uint16_t port, struct fd_list * list) {
    TRACE_DEBUG(INFO, "SCTP support is disabled");
    return ENOTSUP;
}

int fd_sctp_listen(int *sock, int no_ip6, uint16_t port, struct fd_list * list) {
    TRACE_DEBUG(INFO, "SCTP support is disabled");
    return ENOTSUP;
}

int fd_sctp_recvmeta(struct cnxctx * conn, void * buf, size_t len, int *event) {
    TRACE_DEBUG(INFO, "SCTP support is disabled");
    return ENOTSUP;
}

int fd_sctp_sendstrv(struct cnxctx * conn, uint16_t strid, const struct iovec * iov, int iovcnt) {
    TRACE_DEBUG(INFO, "SCTP support is disabled");
    return ENOTSUP;
}

int fd_sctp_get_str_info(int sock, uint16_t *in, uint16_t *out, sSS *primary) {
    TRACE_DEBUG(INFO, "SCTP support is disabled");
    return ENOTSUP;
}

int fd_sctp_get_remote_ep(int sock, sSS *ss) {
    TRACE_DEBUG(INFO, "SCTP support is disabled");
    return ENOTSUP;
}

int fd_sctp_create_bind_server(int *sock, int family, struct fd_list * list, uint16_t port) {
    TRACE_DEBUG(INFO, "SCTP support is disabled");
    return ENOTSUP;
}

/* SCTP 3436 stub functions */
int fd_sctp3436_init(void) {
    TRACE_DEBUG(INFO, "SCTP 3436 support is disabled");
    return 0;
}

void fd_sctp3436_destroy(void) {
    TRACE_DEBUG(INFO, "SCTP 3436 support is disabled");
}

int fd_sctp3436_startthreads(void) {
    TRACE_DEBUG(INFO, "SCTP 3436 support is disabled");
    return 0;
}

int fd_sctp3436_stopthreads(void) {
    TRACE_DEBUG(INFO, "SCTP 3436 support is disabled");
    return 0;
}

int fd_sctp3436_waitthreadsterm(void) {
    TRACE_DEBUG(INFO, "SCTP 3436 support is disabled");
    return 0;
}

int fd_sctp3436_handshake_others(struct cnxctx * conn, int mode_server, int algo, char * priority, void * alt_creds) {
    TRACE_DEBUG(INFO, "SCTP 3436 support is disabled");
    return ENOTSUP;
}

int fd_sctp3436_gnutls_deinit_others(struct cnxctx * conn) {
    TRACE_DEBUG(INFO, "SCTP 3436 support is disabled");
    return 0;
}

int fd_sctp3436_bye(struct cnxctx * conn) {
    TRACE_DEBUG(INFO, "SCTP 3436 support is disabled");
    return 0;
}