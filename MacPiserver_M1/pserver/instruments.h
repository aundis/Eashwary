/*
 * instruments.h
 * com.apple.instruments.remoteserver header file.
 * 
 */

#ifndef __INSTRUMENTS_H
#define __INSTRUMENTS_H

#include <libimobiledevice/instruments.h>
#include <libimobiledevice/service.h>
#include <endianness.h>
#include <thread.h>

#define INSTRUMENTS_MAX_CMD_STR 2048
#define INSTRUMENTS_SEGMENTATION_THRESHOLD 65504

#define INSTRUMENTS_MAGIC "\x79\x5b\x3d\x1f\x20\x00\x00\x00\x00\x00\x01\x00"
#define INSTRUMENTS_MAGIC_LEN (12)

#define INSTRUMENTS_MAGIC2 "\xf0\x01\x00\x00\x00\x00\x00\x00"
#define INSTRUMENTS_MAGIC2_LEN (8)

#define INSTRUMENTS_MAGIC27 "\xf0\x07\x00\x00\x00\x00\x00\x00"
#define INSTRUMENTS_MAGIC26 "\xf0\x06\x00\x00\x00\x00\x00\x00"
#define INSTRUMENTS_MAGIC23 "\xf0\x03\x00\x00\x00\x00\x00\x00"



typedef struct {
	char magic[INSTRUMENTS_MAGIC_LEN];
	uint32_t length;
	uint32_t packet_num;
	uint32_t msg_type;
	uint32_t channel;
	uint32_t empty;
	uint32_t msg_type2;
	uint32_t payload_offset;
	uint32_t length2;
	uint32_t empty2;
} InstrumentsPacket;

#define InstrumentsPacket_to_LE(x) \
        (x)->length        = htole32((x)->length); \
        (x)->packet_num    = htole32((x)->packet_num); \
        (x)->msg_type      = htole32((x)->msg_type); \
        (x)->channel       = htole32((x)->channel); \
        (x)->empty         = htole32((x)->empty); \
        (x)->msg_type2     = htole32((x)->msg_type2); \
        (x)->payload_offset= htole32((x)->payload_offset); \
        (x)->length2       = htole32((x)->length2); \
        (x)->empty2        = htole32((x)->empty2);

#define InstrumentsPacket_from_LE(x) \
        (x)->length        = le32toh((x)->length); \
        (x)->packet_num    = le32toh((x)->packet_num); \
        (x)->msg_type      = le32toh((x)->msg_type); \
        (x)->channel       = le32toh((x)->channel); \
        (x)->empty         = le32toh((x)->empty); \
        (x)->msg_type2     = le32toh((x)->msg_type2); \
        (x)->payload_offset= le32toh((x)->payload_offset); \
        (x)->length2       = le32toh((x)->length2); \
        (x)->empty2        = le32toh((x)->empty2);

struct instruments_client_private {
	service_client_t parent;
	InstrumentsPacket *instruments_packet;
	int file_handle;
	int lock;
	mutex_t mutex;
	int free_parent;
	char *uia_result_path;
	char *uia_script;
	char *app_path;
	char *package_name;
	uint64_t pid;
	int exiting;
	pthread_t worker;
};

typedef struct {
	char magic[INSTRUMENTS_MAGIC2_LEN];
	uint32_t length;
	uint32_t empty;
	uint32_t nl;
	uint32_t channel_magic;
	uint32_t requested_channel;
	uint32_t nl2;
	uint32_t bplist_magic;
	uint32_t bplist_size;
} ChannelRequestPacket;

#define ChannelRequestPacket_to_LE(x) \
        (x)->length            = htole32((x)->length); \
        (x)->empty             = htole32((x)->empty); \
        (x)->nl                = htole32((x)->nl); \
        (x)->channel_magic     = htole32((x)->channel_magic); \
        (x)->requested_channel = htole32((x)->requested_channel); \
        (x)->nl2               = htole32((x)->nl2); \
        (x)->bplist_magic      = htole32((x)->bplist_magic); \
        (x)->bplist_size       = htole32((x)->bplist_size);

#define ChannelRequestPacket_from_LE(x) \
        (x)->length            = le32toh((x)->length); \
        (x)->empty             = le32toh((x)->empty); \
        (x)->nl                = le32toh((x)->nl); \
        (x)->channel_magic     = le32toh((x)->channel_magic); \
        (x)->requested_channel = le32toh((x)->requested_channel); \
        (x)->nl2               = le32toh((x)->nl2); \
        (x)->bplist_magic      = le32toh((x)->bplist_magic); \
        (x)->bplist_size       = le32toh((x)->bplist_size);

typedef struct {
	char magic[INSTRUMENTS_MAGIC2_LEN];
	uint32_t length;
	uint32_t empty;
	uint32_t nl;
	uint32_t bplist_magic;
	uint32_t bplist_size;
} RequestPacket;

#define RequestPacket_to_LE(x) \
        (x)->length            = htole32((x)->length); \
        (x)->empty             = htole32((x)->empty); \
        (x)->nl                = htole32((x)->nl); \
        (x)->bplist_magic      = htole32((x)->bplist_magic); \
        (x)->bplist_size       = htole32((x)->bplist_size);

#define RequestPacket_from_LE(x) \
        (x)->length            = le32toh((x)->length); \
        (x)->empty             = le32toh((x)->empty); \
        (x)->nl                = le32toh((x)->nl); \
        (x)->bplist_magic      = le32toh((x)->bplist_magic); \
        (x)->bplist_size       = le32toh((x)->bplist_size);

typedef struct {
	uint32_t nl;
	uint32_t bplist_magic;
	uint32_t bplist_size;
} BPLPacket;

#define BPLPacket_to_LE(x) \
        (x)->nl                = htole32((x)->nl); \
        (x)->bplist_magic      = htole32((x)->bplist_magic); \
        (x)->bplist_size       = htole32((x)->bplist_size);

#define BPLPacket_from_LE(x) \
        (x)->nl                = le32toh((x)->nl); \
        (x)->bplist_magic      = le32toh((x)->bplist_magic); \
        (x)->bplist_size       = le32toh((x)->bplist_size);

typedef struct {
	uint32_t msg_type;
	uint32_t payload_offset;
	uint32_t length;
	uint32_t empty;
} InstrumentsResponse;

#define InstrumentsResponse_to_LE(x) \
        (x)->msg_type      = htole32((x)->msg_type); \
        (x)->payload_offset= htole32((x)->payload_offset); \
        (x)->length        = htole32((x)->length); \
        (x)->empty         = htole32((x)->empty);

#define InstrumentsResponse_from_LE(x) \
        (x)->msg_type      = le32toh((x)->msg_type); \
        (x)->payload_offset= le32toh((x)->payload_offset); \
        (x)->length        = le32toh((x)->length); \
        (x)->empty         = le32toh((x)->empty);

instruments_error_t instruments_client_new_with_service_client(service_client_t service_client, instruments_client_t *client);
#endif
