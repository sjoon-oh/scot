#pragma once

// Default Envar
// ID starts with 0, monotonically, at initialization..
#define SCOT_ENVVAR_NID         "HARTEBEEST_NID"
#define SCOT_ENVVAR_QSZ         "SCOT_QSIZE"

// Hartebeest Keys
#define HBKEY_PD                "pd-scot"

#define HBKEY_MR_RPLI           "mr-rpli"
#define HBKEY_MR_CHKR           "mr-chkr"
#define HBKEY_MR_RPLY           "mr-rply"
#define HBKEY_MR_RCVR           "mr-rcvr"

#define HBKEY_QP_RPLI           "qp-rpli"
#define HBKEY_QP_CHKR           "qp-chkr"
#define HBKEY_QP_RPLY           "qp-rply"
#define HBKEY_QP_RCVR           "qp-rcvr"

#define HBKEY_SCQ_RPLI          "scq-rpli"
#define HBKEY_SCQ_CHKR          "scq-chkr"
#define HBKEY_SCQ_RPLY          "scq-rply"
#define HBKEY_SCQ_RCVR          "scq-rcvr"

#define HBKEY_RCQ_RPLI          "rcq-rpli"
#define HBKEY_RCQ_CHKR          "rcq-chkr"
#define HBKEY_RCQ_RPLY          "rcq-rply"
#define HBKEY_RCQ_RCVR          "rcq-rcvr"

// SCOT Log area
#define SCOT_BUFFER_SZ          2147483647
#define SCOT_BUFFER_ALIGNS      (SCOT_BUFFER_SZ >> 5)

#define SCOT_LOGALIGN_T         uint32_t
#define SCOT_LOGHEADER_RESERVED 4

// SCOT slots
#define SCOT_SLOT_COUNTS        512
#define SCOT_ALIGN_COUNTS       (SCOT_BUFFER_ALIGNS - SCOT_LOGHEADER_RESERVED)  

// SCOT Message Values
#define SCOT_LOGENTRY_CANARY    36

#define SCOT_MSGTYPE_ACK        0x01
#define SCOT_MSGTYPE_PURE       0x02
#define SCOT_MSGTYPE_WAIT       0x04
#define SCOT_MSGTYPE_HDRONLY    0x08

#define SCOT_HT_SIZE            (1 << 20)
#define SCOT_HT_NBUCKET         (SCOT_HT_SIZE >> 3)

// Worker Signals
#define SCOT_WRKR_HALT          0x0000
#define SCOT_WRKR_RUN           0x0001
#define SCOT_WRKR_PAUSE         0x0002
