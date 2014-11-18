#ifndef PARSE_H
#define PARSE_H

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "log.h"
#include "io.h"
#include "conn.h"

int parse_uri(char* buf, req_status* req);
int parseServerHd(conn_node* node, res_status *resStatus);

#endif