#include "sender.h"
#include "crc.h"

void init_sender(Sender * sender, int id)
{
    //TODO: You should fill in this function as necessary
    sender->send_id = id;
    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;

    // sender->SeqNum = 0;
    // sender->LAR = 0;
    // sender->LFS = 0;
    memset(sender->SeqNum, 0, sizeof(sender->SeqNum));
    memset(sender->LAR, 0, sizeof(sender->LAR));
    memset(sender->LFS, 0, sizeof(sender->LFS));
}

struct timeval * sender_get_next_expiring_timeval(Sender * sender)
{
    //TODO: You should fill in this function so that it returns the next timeout that should occur
    return NULL;
}

int sendQ_full(Sender *sender, unsigned char i){
    return (sender->LFS[i] + 1) % SWS==sender->LAR[i];
}

int sendQ_empty(Sender *sender, unsigned char i){
    return sender->LFS[i] == sender->LAR[i];
}

// void print_frame(Frame * inframe) {
//     fprintf(stderr, "**dst:[RECV_%d]\nsqe_num:[%d]\ndata:[%s]\n", inframe->dst, inframe->SeqNum, inframe->data);
// }

void print_queue(Sender *sender, unsigned char i){
    int max = SWS;
    int start = sender->LAR[i];
    int end = sender->LFS[i];
    fprintf(stderr, "**LAR:%d\n",start);
    fprintf(stderr, "**LFS:%d\n",end);
    while(start!=end){
        Frame *ff = sender->sendQ[i][start].frame;
        char seq_num = ff->SeqNum;
        fprintf(stderr, "**seq_num:%d\n", seq_num);
        start+=1;
        start%=max;
    }
}


void handle_incoming_acks(Sender * sender,
                          LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is corrupted
    //    4) Check whether the frame is for this sender
    //    5) Do sliding window protocol for sender/receiver pair   
    int incoming_msgs_length = ll_get_length(sender->input_framelist_head);
    while (incoming_msgs_length > 0) {
        LLnode * ll_inmsg_node = ll_pop_node(&sender->input_framelist_head);
        incoming_msgs_length = ll_get_length(sender->input_framelist_head);
        char * raw_char_buf = (char *) ll_inmsg_node->value;
        Frame * inframe = convert_char_to_frame(raw_char_buf);
        // fprintf(stderr, "**received packet from receiver\n");
        free(raw_char_buf);
        // deal with crupted ack
        if (inframe->fcs != cal_crc(inframe->data, strlen(inframe->data))) {
            continue;
        }
        // print_frame(inframe);
        if (inframe->dst == sender->send_id) {
            //fprintf(stderr, "<SEND_%d>:[receive ack from <RECV_%d>]\n", sender->send_id ,inframe->src);
            // do something to handle the ack
            if (inframe->Flags == ACK) {
                // deal with ack
                // dequeue
                // fprintf(stderr, "**received ack for sqe_num:%d\n", inframe->AckNum);
                // print_queue(sender);
                int start = sender->LAR[inframe->src];
                int end = sender->LFS[inframe->src];

                int pop_end = -1;

                while(start!=end){

                    Frame *frame_in_queue = sender->sendQ[inframe->src][start].frame;
                    if(!frame_in_queue){
                        break;
                    }
                    char seq_num = frame_in_queue->SeqNum;

                    if(seq_num==inframe->AckNum){
                        pop_end = start;
                        break;
                    }
                    start = (1 + start) % SWS;
                }
                // fprintf(stderr, "pop end:%d\n", pop_end);
                if(pop_end!=-1){
                    start = sender->LAR[inframe->src];
                    end = (pop_end+1) % SWS;

                    while(start!=end){

                        Frame *frame_in_queue = sender->sendQ[inframe->src][start].frame;
                        sender->sendQ[inframe->src][start].frame = NULL;
                        sender->LAR[inframe->src] = (sender->LAR[inframe->src] + 1) % SWS;
                        // sender->RWS-=1;

                        free(frame_in_queue);

                        start = (1 + start) % SWS;
                    }
                }
                // fprintf(stderr, "after pop:\n");
                // print_queue(sender);
            }
            else if (inframe->Flags == NAK) {
                // deal with nak
                //
                int is_send_start = 0;

                int max = SWS;
                int start = sender->LAR[inframe->src];
                int end = sender->LFS[inframe->src];

                while(start!=end){

                    Frame *frame_in_queue = sender->sendQ[inframe->src][start].frame;
                    char seq_num = frame_in_queue->SeqNum;
                    if(seq_num == inframe->AckNum){
                        is_send_start = 1;
                    }
                    if(is_send_start){
                        char * outgoing_charbuf = convert_frame_to_char(frame_in_queue);
                        ll_append_node(outgoing_frames_head_ptr,
                                    outgoing_charbuf);
                    }
                    start+=1;
                    start%=max;
                }
            }
        }
    }
}


void handle_input_cmds(Sender * sender,
                       LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling input cmd
    //    1) Dequeue the Cmd from sender->input_cmdlist_head
    //    2) Convert to Frame
    //    3) Set up the frame according to the sliding window protocol
    //    4) Compute CRC and add CRC to Frame

    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    
        
    //Recheck the command queue length to see if stdin_thread dumped a command on us
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    while (input_cmd_length > 0)
    {
        LLnode * lh = sender->input_cmdlist_head;
        Cmd * lh_cmd = (Cmd *)lh->value;
        if (sendQ_full(sender, lh_cmd->dst_id)) {
            break;
        }
        //Pop a node off and update the input_cmd_length
        LLnode * ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        //Cast to Cmd type and free up the memory for the node
        Cmd * outgoing_cmd = (Cmd *) ll_input_cmd_node->value;
        // if (sendQ_full(sender, outgoing_cmd->dst_id)){
        //     ll_append_node(&sender->input_cmdlist_head, outgoing_cmd);
        //     break;
        // }
        free(ll_input_cmd_node);
            

        //DUMMY CODE: Add the raw char buf to the outgoing_frames list
        //NOTE: You should not blindly send this message out!
        //      Ask yourself: Is this message actually going to the right receiver (recall that default behavior of send is to broadcast to all receivers)?
        //                    Does the receiver have enough space in in it's input queue to handle this message?
        //                    Were the previous messages sent to this receiver ACTUALLY delivered to the receiver?
        int msg_length = strlen(outgoing_cmd->message);
        if (msg_length > MAX_FRAME_SIZE)
        {
            //Do something about messages that exceed the frame size
            printf("<SEND_%d>: sending messages of length greater than %d is not implemented\n", sender->send_id, MAX_FRAME_SIZE);
        }
        else
        {
            //This is probably ONLY one step you want
            Frame * outgoing_frame = (Frame *) malloc (sizeof(Frame));
            strcpy(outgoing_frame->data, outgoing_cmd->message);
            outgoing_frame->Flags = DATA;
            outgoing_frame->src = outgoing_cmd->src_id;
            outgoing_frame->dst = outgoing_cmd->dst_id;
            outgoing_frame->SeqNum = sender->SeqNum[outgoing_cmd->dst_id];
            outgoing_frame->fcs = cal_crc(outgoing_frame->data, strlen(outgoing_frame->data));
            
            // printf("fcs_cal--> %d\nfcs_send-->%d", fcs, outgoing_frame->fcs);
           

            //Convert the message to the outgoing_charbuf
            char * outgoing_charbuf = convert_frame_to_char(outgoing_frame);
            ll_append_node(outgoing_frames_head_ptr,
                           outgoing_charbuf);
            
            // not free frame until it is acked
            // free(outgoing_frame);

            struct timeval current_time;
            gettimeofday(&current_time,NULL);


            sender->sendQ[outgoing_cmd->dst_id][sender->LFS[outgoing_cmd->dst_id]].startime.tv_sec = current_time.tv_sec;       /* Seconds.  */
            sender->sendQ[outgoing_cmd->dst_id][sender->LFS[outgoing_cmd->dst_id]].startime.tv_usec = current_time.tv_usec; 	/* Microseconds.  */
            sender->sendQ[outgoing_cmd->dst_id][sender->LFS[outgoing_cmd->dst_id]].endtime.tv_sec = current_time.tv_sec;
            sender->sendQ[outgoing_cmd->dst_id][sender->LFS[outgoing_cmd->dst_id]].endtime.tv_usec = current_time.tv_usec + 100;
            // fprintf(stderr ,"\nbefore inqueue------\n");
            // print_frame(outgoing_frame);
            // fprintf(stderr ,"------\n");

            sender->sendQ[outgoing_cmd->dst_id][sender->LFS[outgoing_cmd->dst_id]].frame = outgoing_frame;

            sender->LFS[outgoing_cmd->dst_id] = (1 + sender->LFS[outgoing_cmd->dst_id]) % SWS;
            sender->SeqNum[outgoing_cmd->dst_id] = (1 + sender->SeqNum[outgoing_cmd->dst_id]) % MAX_SEQ;

             //At this point, we don't need the outgoing_cmd
            free(outgoing_cmd->message);
            free(outgoing_cmd);

        }
    }   
}


void handle_timedout_frames(Sender * sender,
                            LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling timed out datagrams
    //    1) Iterate through the sliding window protocol information you maintain for each receiver
    //    2) Locate frames that are timed out and add them to the outgoing frames
    //    3) Update the next timeout field on the outgoing frames
    for (unsigned char i = 0; i < glb_receivers_array_length; i++){
    if(sendQ_empty(sender, i)){
        continue ;
    }
    struct timeval  curr_timeval;

    int start = sender->LAR[i];
    int end = sender->LFS[i];

    while(start!=end){
    	gettimeofday(&curr_timeval,NULL);
    	long last_time = timeval_usecdiff(&curr_timeval,&(sender->sendQ[i][start].endtime));

		if(last_time<0){
		        Frame *f = sender->sendQ[i][start].frame;

		        char * outgoing_charbuf = convert_frame_to_char(f);
		        ll_append_node(outgoing_frames_head_ptr, outgoing_charbuf);

		        struct timeval current_time;
		        gettimeofday(&current_time,NULL);

		        sender->sendQ[i][start].startime.tv_sec = current_time.tv_sec;
		        sender->sendQ[i][start].startime.tv_usec = current_time.tv_usec;
		        sender->sendQ[i][start].endtime.tv_sec = current_time.tv_sec;
		        sender->sendQ[i][start].endtime.tv_usec = current_time.tv_usec+100;
		}
        start = (1 + start) % SWS;
    }}
}


void * run_sender(void * input_sender)
{    
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender * sender = (Sender *) input_sender;    
    LLnode * outgoing_frames_head;
    struct timeval * expiring_timeval;
    long sleep_usec_time, sleep_sec_time;
    
    //This incomplete sender thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up
    //2. Grab the mutex protecting the input_cmd/inframe queues
    //3. Dequeues messages from the input queue and adds them to the outgoing_frames list
    //4. Releases the lock
    //5. Sends out the messages

    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);

    while(1)
    {    
        outgoing_frames_head = NULL;

        //Get the current time
        gettimeofday(&curr_timeval, 
                     NULL);

        //time_spec is a data structure used to specify when the thread should wake up
        //The time is specified as an ABSOLUTE (meaning, conceptually, you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        //Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);

        //Perform full on timeout
        if (expiring_timeval == NULL)
        {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        }
        else
        {
            //Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval,
                                               expiring_timeval);

            //Sleep if the difference is positive
            if (sleep_usec_time > 0)
            {
                sleep_sec_time = sleep_usec_time/1000000;
                sleep_usec_time = sleep_usec_time % 1000000;   
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time*1000;
            }   
        }

        //Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        
        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames or input commands should go 
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        //Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);
        
        //Nothing (cmd nor incoming frame) has arrived, so do a timed wait on the sender's condition variable (releases lock)
        //A signal on the condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 &&
            inframe_queue_length == 0)
        {
            
            pthread_cond_timedwait(&sender->buffer_cv, 
                                   &sender->buffer_mutex,
                                   &time_spec);
        }
        //Implement this
        handle_incoming_acks(sender,
                             &outgoing_frames_head);

        //Implement this
        handle_input_cmds(sender,
                          &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);


        //Implement this
        handle_timedout_frames(sender,
                               &outgoing_frames_head);

        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        
        while(ll_outgoing_frame_length > 0)
        {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *)  ll_outframe_node->value;

            //Don't worry about freeing the char_buf, the following function does that
            send_msg_to_receivers(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
