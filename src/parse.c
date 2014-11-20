#include "parse.h"
#include "conn.h"

int parse_uri(char* buf, req_status* req) {

    int temport = 0;
    int status = 0;
    char host[MAXLINE], method[MAXLINE], uri[MAXLINE];

    memset(req->uri, 0, MAXLINE);

    if (sscanf(buf, "%s %s %s", method, uri, req->version) != 3) {
        return -1;
    }

    if(!strcmp(method,"GET"))
        req->method = GET;
    else if(!strcmp(method,"HEAD"))
        req->method = HEAD;
    else if(!strcmp(method,"POST"))
        req->method = POST;
    else{
        req->method = NOT_SUPPORT;
    }


    if (strstr(uri, "http://")) {
        if (sscanf(uri, "http://%8192[^:]:%i%8192[^\n]", host, &temport, req->uri) == 3) {status = 1;}
        else if (sscanf(uri, "http://%8192[^/]%8192[^\n]", host, req->uri) == 2) {status = 2;}
        else if (sscanf(uri, "http://%8192[^:]:%i[^\n]", host, &temport) == 2) {status = 3;}
        else if (sscanf(uri, "http://%8192[^/]", host) == 1) {status = 4;}
    }
    else {
        strcpy(req->uri, uri);
    }

    req->resloc = strrchr(req->uri, '/');

    if (req->resloc == NULL) {
        printf("uri parsing error!!\n");
        printf("%s\n", req->uri);
        return -1;
    }

    req->resloc++;

    if (strstr(req->resloc, ".f4m")) {
        req->reqtype = MANIFEST;
    } else if (sscanf(req->resloc, "%dSeg%d-Frag%d", &req->bitrate, &req->seg, &req->frag) == 3) {
        req->reqtype = VIDEO;
    } else{
        req->reqtype = OTHER;
    }

    return status;
}


int parseServerHd(conn_node* node, res_status *resStatus){

    char linebuf[MAXLINE];
    int linesize = 0, hdsize = 0;
    memset(linebuf, 0, MAXLINE);

    while((linesize = httpreadline(node->serverfd, linebuf, MAXLINE)) > 0) {
        hdsize += linesize;
        printf("%s", linebuf);

        if (hdsize > MAXLINE) {
            printf("Header too long to fit in buffer\n");
            return -1;
        }

        strcat(resStatus->buf, linebuf);
        if (strcmp(linebuf, "\r\n") == 0) {
            break;
        }

        if (strstr(linebuf, "Content-Length")) {
            sscanf(linebuf, "Content-Length: %d", &resStatus->contentlen);
            if (resStatus->content != NULL) {
                printf("Fatal error in server header parse, the content hasbeen allocated\n");
                return -1;
            }

            resStatus->content = calloc(resStatus->contentlen,sizeof(char));
        }

        memset(linebuf, 0, MAXLINE);
    }

    if (linesize <= 0) {
        printf("Reading response error\n");
        return -1;
    }

    return hdsize;
}