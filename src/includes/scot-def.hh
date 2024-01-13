#pragma once

#define __DEBUG__X

// Default Envar
// ID starts with 0, monotonically, at initialization..
#define SCOT_ENVVAR_NID         "HARTEBEEST_NID"
#define SCOT_ENVVAR_QSZ         "SCOT_QSIZE"
#define SCOT_CONFPATH           "SCOT_CONF"

#define SCOT_MAX_QSIZE          9

// Hartebeest Keys
#define HBKEY_PD                "pd-scot"

#define HBKEY_MR_RPLI           "mr-rpli"
#define HBKEY_MR_CHKR           "mr-chkr"
#define HBKEY_MR_RPLY           "mr-rply"
#define HBKEY_MR_RCVR           "mr-rcvr"
#define HBKEY_MR_HBTR           "mr-hbtr"   // Heartbeat

#define HBKEY_QP_RPLI           "qp-rpli"
#define HBKEY_QP_CHKR           "qp-chkr"
#define HBKEY_QP_RPLY           "qp-rply"
#define HBKEY_QP_RCVR           "qp-rcvr"
#define HBKEY_QP_HBTR           "qp-hbtr"

#define HBKEY_SCQ_RPLI          "scq-rpli"
#define HBKEY_SCQ_CHKR          "scq-chkr"
#define HBKEY_SCQ_RPLY          "scq-rply"
#define HBKEY_SCQ_RCVR          "scq-rcvr"
#define HBKEY_SCQ_HBTR          "scq-hbtr"

#define HBKEY_RCQ_RPLI          "rcq-rpli"
#define HBKEY_RCQ_CHKR          "rcq-chkr"
#define HBKEY_RCQ_RPLY          "rcq-rply"
#define HBKEY_RCQ_RCVR          "rcq-rcvr"
#define HBKEY_RCQ_HBTR          "rcq-hbtr"

// SCOT Log area
#define SCOT_BUFFER_SZ          2147483647
#define SCOT_BUFFER_ALIGNS      (SCOT_BUFFER_SZ >> 5)

#define SCOT_LOGALIGN_T         uint32_t
#define SCOT_LOG_FINEGRAINED_T  uint8_t
#define SCOT_LOGHEADER_RESERVED 4

// SCOT slots
#define SCOT_SLOT_COUNTS        512
#define SCOT_ALIGN_COUNTS       (SCOT_BUFFER_ALIGNS - SCOT_LOGHEADER_RESERVED)  

// SCOT Message Values
#define SCOT_LOGENTRY_CANARY    36

#define SCOT_MSGTYPE_NONE       0x00
#define SCOT_MSGTYPE_ACK        0x01
#define SCOT_MSGTYPE_PURE       0x02
#define SCOT_MSGTYPE_WAIT       0x04
#define SCOT_MSGTYPE_HDRONLY    0x08
#define SCOT_MSGTYPE_COMMPREV   0x10

#define SCOT_HT_SIZE            (1 << 20)
#define SCOT_HT_NBUCKET         (SCOT_HT_SIZE >> 3)

// Worker Signals
#define SCOT_WRKR_HALT          0x0000
#define SCOT_WRKR_RUN           0x0001
#define SCOT_WRKR_PAUSE         0x0002

#define SCOT_SLOT_RESET         1

#define SCOT_TIMESTAMP_RECORDS  10000000

// SCOT Heartbeat Scoreboard
#define SCOT_SCB_FINEGRAINED_T  uint8_t
#define SCOT_SCB_WIDE_T         uint16_t

// For records
#define SCOT_OPTIMIZATION_START
#define SCOT_OPTIMIZATION_END