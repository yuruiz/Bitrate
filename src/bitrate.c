#include "bitrate.h"
#include "proxy.h"
#include "log.h"

static int bitrate = 10;

static double throughput = 0;

static int bitrates[4] = {10, 100, 500, 1000};

int getBitrate(){
    return bitrate;
}

void updateBitrate(long long t1, long long t2, int len, int req_bitrate, char* chunkname, char* server_ip) {

    double alpha = getAlpha();

    if (t2 < t1) {
        return;
    }

    double t = (double) (((double) len) / ((double) (t2 - t1))) * 8000;

    throughput = alpha * t + (1 - alpha) * throughput;

    if (t2 == t1) {
        bitrate = bitrates[3];
    }else{
        int i;
        for (i = 0; i < 4; ++i) {
            if (throughput < bitrates[i] * 1.5 * 1000) {
                if (i > 0) {
                    i--;
                }
                break;
            }
        }

        bitrate = bitrates[i];
    }

    printf("Bit rate now %d\n", bitrate);
    printf("The alpha is %f\n", alpha);
    printf("The average throughput is %f\n", throughput);
    printf("The current throughput is %f\n", t);

    logging((float) (t2 - t1) / 1000, (float) t / 1000, (float) throughput / 1000, req_bitrate, server_ip, chunkname);

    return;

}