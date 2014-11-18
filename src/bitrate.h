#ifndef BITRATE_H
#define BITRATE_H

int getBitrate();
void updateBitrate(long long t1, long long t2, int len, int req_bitrate,char* chunkname, char* server_ip);

#endif