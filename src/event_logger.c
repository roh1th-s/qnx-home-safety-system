/*
 * event_logger.c
 *
 *  Created on: 20-Nov-2025
 *      Author: Manjari
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>

#define MAX_MSG_LEN 128

typedef struct {
    uint16_t type;
    char text[MAX_MSG_LEN];
} event_msg_t;

typedef struct {
    uint16_t status;
} event_reply_t;

int main() {

    // Attach a named channel
    name_attach_t *attach = name_attach(NULL, "event_logger", 0);
    if (!attach) {
        perror("name_attach");
        return -1;
    }

    printf("Event Logger Server started. Name: /event_logger\n");

    FILE *logfile = fopen("/home/qnxuser/home_safety.log", "a");
    if (!logfile) {
        perror("fopen");
        return -1;
    }


    while (1) {
        event_msg_t msg;
        event_reply_t reply;
        memset(&msg, 0, sizeof(msg));

        int rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1) {
            perror("MsgReceive");
            continue;
        }

        if (rcvid == 0) {
            continue; // system pulse, ignore
        }

        // Log the event
        fprintf(logfile, "EVENT: %s\n", msg.text);
        fflush(logfile);

        printf("Logged: %s\n", msg.text);

        // Send reply
        reply.status = 0;
        MsgReply(rcvid, 0, &reply, sizeof(reply));
    }

    fclose(logfile);
    name_detach(attach, 0);
    return 0;
}
