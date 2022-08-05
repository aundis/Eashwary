#include "pqueue.h"

typedef struct {
	struct threadqueue *queue;
	struct threadqueue *t_queue;
	int no_clients;
	int scaling;
	int please_exit;
	int scr_fail_count;
	int orientation;
	int retina;
    char *packagepath;
	uint64_t pid;
	instruments_client_t instr;
	int srvSocket;
	struct sockaddr_in srvSocketAddr;
	int cliSocket;
	struct sockaddr_in cliSocketAddr;
	int input_enabled;
	rfbScreenInfoPtr screen;
	int processScreen;
    struct threadqueue *k_queue;
	char *helper_packagepath;
	uint64_t helper_pid;
} pServerState_t;

typedef struct {
	int leftClicked;
	int rightClicked;
	time_t down_time;
	int down_x;
	int down_y;
} pClientState_t;

typedef struct {
    char *buffer;
    int offset;
} pImgBuffer_t;

