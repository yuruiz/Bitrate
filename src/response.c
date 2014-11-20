#include <sys/time.h>
#include "parse.h"
#include "conn.h"
#include "bitrate.h"

int sendRequset(conn_node *node, req_status *reqStatus){
    char header[MAXLINE];
    char req[MAXLINE];
    char reqOrigin[MAXLINE];
    char buf[MAXLINE];
    req_t *reqRecord;
    int n;

    memset(header, 0, MAXLINE);
    memset(req, 0, MAXLINE);
    memset(reqOrigin, 0, MAXLINE);
    memset(buf, 0, MAXLINE);

    while ((n = httpreadline(node->clientfd, buf, MAXLINE)) > 0) {
        strcat(header, buf);
//        printf("%s", buf);

        if(!strcmp(buf,"\r\n")){
            break;
        }

        memset(buf, 0, MAXLINE);
    }

    if (n < 0) {
        return -1;
    } else if (n == 0) {
        reqStatus->connclose = true;
    }

    reqRecord = malloc(sizeof(req_t));
    switch (reqStatus->reqtype) {
        case MANIFEST:
            printf("Building manifest request\n");
            *(reqStatus->resloc) = 0;
            sprintf(req,"GET %sbig_buck_bunny_nolist.f4m %s",reqStatus->uri, reqStatus->version);
            sprintf(reqOrigin,"GET %sbig_buck_bunny.f4m %s",reqStatus->uri, reqStatus->version);
            reqRecord->reqtype = OTHER;
            break;
        case VIDEO:
            printf("Building video request\n");
            *(reqStatus->resloc) = 0;
            sprintf(req,"GET %s%dSeg%d-Frag%d %s",reqStatus->uri, getBitrate(), reqStatus->seg, reqStatus->frag ,reqStatus->version);
            reqRecord->reqtype = VIDEO;
            reqRecord->bitrate = getBitrate();
            struct timeval curT;
            gettimeofday(&curT, NULL);
            reqRecord->timeStamp = curT.tv_sec * 1000 + curT.tv_usec / 1000;
            memset(reqRecord->chunkname, 0, MINLINE);
            sprintf(reqRecord->chunkname, "Seg%d-Frag%d", reqStatus->seg, reqStatus->frag);
            break;
        case OTHER:
            sprintf(req, "GET %s %s", reqStatus->uri, reqStatus->version);
            reqRecord->reqtype = OTHER;
            break;
        default:
            break;
    }


    if (reqStatus->reqtype == MANIFEST) {
        if(strstr(reqOrigin,"\r\n")==NULL){
            strcat(reqOrigin,"\r\n");
        }
        strcat(reqOrigin, header);
        size_t reqlen = strlen(reqOrigin);
        if (write(node->serverfd, reqOrigin, reqlen) != reqlen) {
            printf("sending request error\n");
            return -1;
        }

        req_t *reqR = malloc(sizeof(req_t));
        reqR->reqtype = MANIFEST;
        enqueue(node->reqq, (void *) reqR);
    }


    if(strstr(req,"\r\n")==NULL){
        strcat(req,"\r\n");
    }

    strcat(req,header);

    int reqlen = strlen(req);

    if (write(node->serverfd, req, reqlen) != reqlen) {
        printf("sending request error\n");
        return -1;
    }

    enqueue(node->reqq, (void *) reqRecord);

    return 0;

}