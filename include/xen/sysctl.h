/******************************************************************************
 * sysctl.h
 *
 * System management operations. For use by node control stack.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2002-2006, K Fraser
 */

#ifndef __XEN_PUBLIC_SYSCTL_H__
#define __XEN_PUBLIC_SYSCTL_H__

#include "xen.h"
#include "physdev.h"
/*
 * XEN_SYSCTL_LIVEPATCH_op
 *
 * Refer to the docs/unstable/misc/livepatch.markdown
 * for the design details of this hypercall.
 *
 * There are four sub-ops:
 *  XEN_SYSCTL_LIVEPATCH_UPLOAD (0)
 *  XEN_SYSCTL_LIVEPATCH_GET (1)
 *  XEN_SYSCTL_LIVEPATCH_LIST (2)
 *  XEN_SYSCTL_LIVEPATCH_ACTION (3)
 *
 * The normal sequence of sub-ops is to:
 *  1) XEN_SYSCTL_LIVEPATCH_UPLOAD to upload the payload. If errors STOP.
 *  2) XEN_SYSCTL_LIVEPATCH_GET to check the `->rc`. If -XEN_EAGAIN spin.
 *     If zero go to next step.
 *  3) XEN_SYSCTL_LIVEPATCH_ACTION with LIVEPATCH_ACTION_APPLY to apply the patch.
 *  4) XEN_SYSCTL_LIVEPATCH_GET to check the `->rc`. If in -XEN_EAGAIN spin.
 *     If zero exit with success.
 */

#define LIVEPATCH_PAYLOAD_VERSION 1
/*
 * Structure describing an ELF payload. Uniquely identifies the
 * payload. Should be human readable.
 * Recommended length is upto XEN_LIVEPATCH_NAME_SIZE.
 * Includes the NUL terminator.
 */
#define XEN_LIVEPATCH_NAME_SIZE 128
struct xen_livepatch_name {
    guest_handle_64_t name;                 /* IN: pointer to name. */
    uint16_t size;                          /* IN: size of name. May be upto
                                               XEN_LIVEPATCH_NAME_SIZE. */
    uint16_t pad[3];                        /* IN: MUST be zero. */
};
typedef struct xen_livepatch_name xen_livepatch_name_t;

/*
 * Upload a payload to the hypervisor. The payload is verified
 * against basic checks and if there are any issues the proper return code
 * will be returned. The payload is not applied at this time - that is
 * controlled by XEN_SYSCTL_LIVEPATCH_ACTION.
 *
 * The return value is zero if the payload was succesfully uploaded.
 * Otherwise an EXX return value is provided. Duplicate `name` are not
 * supported.
 *
 * The payload at this point is verified against basic checks.
 *
 * The `payload` is the ELF payload as mentioned in the `Payload format`
 * section in the Live Patch design document.
 */
#define XEN_SYSCTL_LIVEPATCH_UPLOAD 0
struct xen_sysctl_livepatch_upload {
    xen_livepatch_name_t name;              /* IN, name of the patch. */
    uint64_t size;                          /* IN, size of the ELF file. */
    guest_handle_64_t payload;              /* IN, the ELF file. */
};
typedef struct xen_sysctl_livepatch_upload xen_sysctl_livepatch_upload_t;

/*
 * Retrieve an status of an specific payload.
 *
 * Upon completion the `struct xen_livepatch_status` is updated.
 *
 * The return value is zero on success and XEN_EXX on failure. This operation
 * is synchronous and does not require preemption.
 */
#define XEN_SYSCTL_LIVEPATCH_GET 1

struct xen_livepatch_status {
#define LIVEPATCH_STATE_CHECKED      1
#define LIVEPATCH_STATE_APPLIED      2
    uint32_t state;                /* OUT: LIVEPATCH_STATE_*. */
    int32_t rc;                    /* OUT: 0 if no error, otherwise -XEN_EXX. */
};
typedef struct xen_livepatch_status xen_livepatch_status_t;

struct xen_sysctl_livepatch_get {
    xen_livepatch_name_t name;              /* IN, name of the payload. */
    xen_livepatch_status_t status;          /* IN/OUT, state of it. */
};
typedef struct xen_sysctl_livepatch_get xen_sysctl_livepatch_get_t;

/*
 * Retrieve an array of abbreviated status and names of payloads that are
 * loaded in the hypervisor.
 *
 * If the hypercall returns an positive number, it is the number (up to `nr`)
 * of the payloads returned, along with `nr` updated with the number of remaining
 * payloads, `version` updated (it may be the same across hypercalls. If it
 * varies the data is stale and further calls could fail). The `status`,
 * `name`, and `len`' are updated at their designed index value (`idx`) with
 * the returned value of data.
 *
 * If the hypercall returns E2BIG the `nr` is too big and should be
 * lowered. The upper limit of `nr` is left to the implemention.
 *
 * Note that due to the asynchronous nature of hypercalls the domain might have
 * added or removed the number of payloads making this information stale. It is
 * the responsibility of the toolstack to use the `version` field to check
 * between each invocation. if the version differs it should discard the stale
 * data and start from scratch. It is OK for the toolstack to use the new
 * `version` field.
 */
#define XEN_SYSCTL_LIVEPATCH_LIST 2
struct xen_sysctl_livepatch_list {
    uint32_t version;                       /* OUT: Hypervisor stamps value.
                                               If varies between calls, we are
                                             * getting stale data. */
    uint32_t idx;                           /* IN: Index into hypervisor list. */
    uint32_t nr;                            /* IN: How many status, name, and len
                                               should fill out. Can be zero to get
                                               amount of payloads and version.
                                               OUT: How many payloads left. */
    uint32_t pad;                           /* IN: Must be zero. */
    guest_handle_64_t status;               /* OUT. Must have enough
                                               space allocate for nr of them. */
    guest_handle_64_t name;                 /* OUT: Array of names. Each member
                                               MUST XEN_LIVEPATCH_NAME_SIZE in size.
                                               Must have nr of them. */
    guest_handle_64_t len;                  /* OUT: Array of lengths of name's.
                                               Must have nr of them. */
};
typedef struct xen_sysctl_livepatch_list xen_sysctl_livepatch_list_t;

/*
 * Perform an operation on the payload structure referenced by the `name` field.
 * The operation request is asynchronous and the status should be retrieved
 * by using either XEN_SYSCTL_LIVEPATCH_GET or XEN_SYSCTL_LIVEPATCH_LIST hypercall.
 */
#define XEN_SYSCTL_LIVEPATCH_ACTION 3
struct xen_sysctl_livepatch_action {
    xen_livepatch_name_t name;              /* IN, name of the patch. */
#define LIVEPATCH_ACTION_UNLOAD       1
#define LIVEPATCH_ACTION_REVERT       2
#define LIVEPATCH_ACTION_APPLY        3
#define LIVEPATCH_ACTION_REPLACE      4
    uint32_t cmd;                           /* IN: LIVEPATCH_ACTION_*. */
    uint32_t timeout;                       /* IN: Zero if no timeout. */
                                            /* Or upper bound of time (ms) */
                                            /* for operation to take. */
};
typedef struct xen_sysctl_livepatch_action xen_sysctl_livepatch_action_t;

struct xen_sysctl_livepatch_op {
    uint32_t cmd;                           /* IN: XEN_SYSCTL_LIVEPATCH_*. */
    uint32_t pad;                           /* IN: Always zero. */
    union {
        xen_sysctl_livepatch_upload_t upload;
        xen_sysctl_livepatch_list_t list;
        xen_sysctl_livepatch_get_t get;
        xen_sysctl_livepatch_action_t action;
    } u;
};
typedef struct xen_sysctl_livepatch_op xen_sysctl_livepatch_op_t;

#define ARINC653_MAX_DOMAINS_PER_SCHEDULE   64
/*
 * This structure is used to pass a new ARINC653 schedule from a
 * privileged domain (ie dom0) to Xen.
 */
struct xen_sysctl_arinc653_schedule {
    /* major_frame holds the time for the new schedule's major frame
     * in nanoseconds. */
    uint64_t __aligned(8)     major_frame;
    /* num_sched_entries holds how many of the entries in the
     * sched_entries[] array are valid. */
    uint8_t     num_sched_entries;
    /* The sched_entries array holds the actual schedule entries. */
    struct {
        /* dom_handle must match a domain's UUID */
        uint8_t dom_handle[16];
        /* If a domain has multiple VCPUs, vcpu_id specifies which one
         * this schedule entry applies to. It should be set to 0 if
         * there is only one VCPU for the domain. */
        unsigned int vcpu_id;
        /* runtime specifies the amount of time that should be allocated
         * to this VCPU per major frame. It is specified in nanoseconds */
        uint64_t __aligned(8) runtime;
    } sched_entries[ARINC653_MAX_DOMAINS_PER_SCHEDULE];
};
typedef struct xen_sysctl_arinc653_schedule xen_sysctl_arinc653_schedule_t;

#define XEN_SYSCTL_SCHED_RATELIMIT_MAX 500000
#define XEN_SYSCTL_SCHED_RATELIMIT_MIN 100

struct xen_sysctl_credit_schedule {
    /* Length of timeslice in milliseconds */
#define XEN_SYSCTL_CSCHED_TSLICE_MAX 1000
#define XEN_SYSCTL_CSCHED_TSLICE_MIN 1
    unsigned tslice_ms;
    unsigned ratelimit_us;
    /*
     * How long we consider a vCPU to be cache-hot on the
     * CPU where it has run (max 100ms, in microseconds)
    */
#define XEN_SYSCTL_CSCHED_MGR_DLY_MAX_US (100 * 1000)
    unsigned vcpu_migr_delay_us;
};

struct xen_sysctl_credit2_schedule {
    unsigned ratelimit_us;
};

#define XEN_SYSCTL_SCHEDOP_putinfo 0
#define XEN_SYSCTL_SCHEDOP_getinfo 1
struct xen_sysctl_scheduler_op {
    uint32_t cpupool_id; /* Cpupool whose scheduler is to be targetted. */
    uint32_t sched_id;   /* XEN_SCHEDULER_* (domctl.h) */
    uint32_t cmd;        /* XEN_SYSCTL_SCHEDOP_* */
    union {
        xen_sysctl_arinc653_schedule_t *schedule;
        uint64_t :64;
        struct xen_sysctl_credit_schedule sched_credit;
        struct xen_sysctl_credit2_schedule sched_credit2;
    } u;
};
typedef struct xen_sysctl_scheduler_op xen_sysctl_scheduler_op_t;

struct xen_sysctl {
    uint32_t cmd;
#define XEN_SYSCTL_livepatch_op                  27
#define XEN_SYSCTL_scheduler_op                  19
    uint32_t interface_version; /* XEN_SYSCTL_INTERFACE_VERSION */
    union {
        struct xen_sysctl_livepatch_op      livepatch;
        struct xen_sysctl_scheduler_op      scheduler_op;
        uint8_t                             pad[128];
    } u;
};
typedef struct xen_sysctl xen_sysctl_t;

#endif /* __XEN_PUBLIC_SYSCTL_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
