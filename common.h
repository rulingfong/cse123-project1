#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
typedef unsigned char uchar_t;

//System configuration information
struct SysConfig_t
{
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t  SysConfig;

//Command line input information
struct Cmd_t
{
    uint16_t src_id;
    uint16_t dst_id;
    char * message;
};
typedef struct Cmd_t Cmd;

//Linked list information
enum LLtype 
{
    llt_string,
    llt_frame,
    llt_integer,
    llt_head
} LLtype;

struct LLnode_t
{
    struct LLnode_t * prev;
    struct LLnode_t * next;
    enum LLtype type;

    void * value;
};
typedef struct LLnode_t LLnode;

#define MAX_FRAME_SIZE 64
#define FRAME_HEAD_SIZE (sizeof(unsigned char) * 5 + sizeof(unsigned int))
//TODO: You should change this!
//Remember, your frame can be AT MOST 64 bytes!
#define FRAME_PAYLOAD_SIZE (MAX_FRAME_SIZE - FRAME_HEAD_SIZE)

// values for Flags field
#define DATA 1
#define ACK 2
#define NAK 3

#define MAX_SEQ 256

struct Frame_t
{
    unsigned char src; // sender id
    unsigned char dst; // receiver id
    unsigned char Flags; // whether the frame is an ACK or carries data
    unsigned char SeqNum; // sequence number of this frame
    unsigned char AckNum; // ack of received frame
    char data[FRAME_PAYLOAD_SIZE];
    unsigned int fcs; // crc
};
typedef struct Frame_t Frame;

#define MAX_TER 128

//Receiver and sender data structures
#define RWS 1 /* receive window size */
struct Receiver_t
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_framelist_head
    // 4) recv_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode * input_framelist_head;
    
    int recv_id;
    // unsigned char LAF; // largest acceptable frame
    // unsigned char LFR; // last frame received
    unsigned char NFE[MAX_TER]; // next frame expected
};


struct sendQ_slot {
    struct timeval  endtime;
    struct timeval startime;
    Frame *frame;
};

// SWS sliding window size
// LAR - LFS <= SWS
#define SWS 8
struct Sender_t
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;    
    LLnode * input_cmdlist_head;
    LLnode * input_framelist_head;
    int send_id;

    // sliding windows relative variables
    unsigned char LAR[MAX_TER]; // last ack received
    unsigned char LFS[MAX_TER]; // last frame sent
    unsigned char SeqNum[MAX_TER];
    struct sendQ_slot sendQ[MAX_TER][SWS];

};

enum SendFrame_DstType 
{
    ReceiverDst,
    SenderDst
} SendFrame_DstType ;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;

//Declare global variables here
//DO NOT CHANGE: 
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender * glb_senders_array;
Receiver * glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;
#endif 
