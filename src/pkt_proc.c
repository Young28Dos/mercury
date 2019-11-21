/*
 * pkt_proc.c
 * 
 * Copyright (c) 2019 Cisco Systems, Inc. All rights reserved.  License at 
 * https://github.com/cisco/mercury/blob/master/LICENSE 
 */

#include <string.h>
#include "extractor.h"
#include "pcap_file_io.h"
#include "json_file_io.h"
#include "packet.h"
#include "rnd_pkt_drop.h"

/*
 * packet_filter_threshold is a (somewhat arbitrary) threshold used in
 * the packet metadata filter; it will probably get eliminated soon,
 * in favor of extractor::proto_state::state, but for now it remains
 */
unsigned int packet_filter_threshold = 8;

void frame_handler_flush_pcap(void *userdata) {
    union frame_handler_context *fhc = (union frame_handler_context *)userdata;
    struct pcap_file *f = &fhc->pcap_file;
    FILE *file_ptr = f->file_ptr;
    if (file_ptr != NULL) {
        if (fflush(file_ptr) != 0) {
            perror("warning: could not flush the pcap file\n");
        }
    }
}

void frame_handler_filter_write_pcap(void *userdata,
				     struct packet_info *pi,
				     uint8_t *eth_hdr) {

    union frame_handler_context *fhc = (union frame_handler_context *)userdata;
    uint8_t *packet = eth_hdr;
    unsigned int length = pi->len;

    printf("%s: tcp_init_msg_filter: %p\n", __func__, &fhc->filter_writer.pf.tcp_init_msg_filter);

    if (packet_filter_apply(&fhc->filter_writer.pf, packet, length)) {
        pcap_file_write_packet_direct(&fhc->filter_writer.pcap_file, eth_hdr, pi->len, pi->ts.tv_sec, pi->ts.tv_nsec / 1000);
    }
}

enum status frame_handler_filter_write_pcap_init(struct frame_handler *handler,
						 const char *outfile,
						 int flags,
						 const char *packet_filter_config_string) {
    /*
     * setup output to fingerprint file or PCAP write file
     */
    handler->func = frame_handler_filter_write_pcap;
    handler->flush_func = frame_handler_flush_pcap;
    printf("%s: tcp_init_msg_filter: %p\n", __func__, handler->context.filter_writer.pf.tcp_init_msg_filter);
    enum status status = packet_filter_init(&handler->context.filter_writer.pf, packet_filter_config_string);
    if (status != status_ok) {
        printf("error: could not configure packet filter with config string \"%s\"\n", packet_filter_config_string);
        return status;
    }
    status = pcap_file_open(&handler->context.filter_writer.pcap_file, outfile, io_direction_writer, flags);
    if (status != status_ok) {
        printf("error: could not open pcap output file %s\n", outfile);
    }
    printf("%s: tcp_init_msg_filter: %p\n", __func__, handler->context.filter_writer.pf.tcp_init_msg_filter);
    return status;
}

void frame_handler_write_pcap(void *userdata,
			      struct packet_info *pi,
			      uint8_t *eth) {
    union frame_handler_context *fhc = (union frame_handler_context *)userdata;

    extern int rnd_pkt_drop_percent_accept;  /* defined in rnd_pkt_drop.c */

    if (rnd_pkt_drop_percent_accept && drop_this_packet()) {
        return;  /* random packet drop configured, and this packet got selected to be discarded */
    }
    pcap_file_write_packet_direct(&fhc->pcap_file, eth, pi->len, pi->ts.tv_sec, pi->ts.tv_nsec / 1000);

}

enum status frame_handler_write_pcap_init(struct frame_handler *handler,
				    const char *outfile,
				    int flags) {

    /*
     * setup output to fingerprint file or PCAP write file
     */
    enum status status = pcap_file_open(&handler->context.pcap_file, outfile, io_direction_writer, flags);
    if (status) {
	printf("error: could not open pcap output file %s\n", outfile);
	return status_err;
    }
    handler->func = frame_handler_write_pcap;
    handler->flush_func = frame_handler_flush_pcap;

    return status_ok;
}

void frame_handler_flush_fingerprints(void *userdata) {
    union frame_handler_context *fhc = (union frame_handler_context *)userdata;
    FILE *file_ptr = fhc->json_file.file;
    if (file_ptr != NULL) {
        if (fflush(file_ptr) != 0) {
            perror("warning: could not flush the json file\n");
        }
    }
}

void frame_handler_write_fingerprints(void *userdata,
				      struct packet_info *pi,
				      uint8_t *eth) {
    union frame_handler_context *fhc = (union frame_handler_context *)userdata;
    
    json_file_write(&fhc->json_file, eth, pi->len, pi->ts.tv_sec, pi->ts.tv_nsec / 1000);
}

enum status frame_handler_write_fingerprints_init(struct frame_handler *handler,
						  const char *outfile_name,
						  const char *mode,
						  uint64_t max_records) {

    enum status status;

    status = json_file_init(&handler->context.json_file, outfile_name, mode, max_records);
    if (status) {
	return status;
    }
    handler->func = frame_handler_write_fingerprints;
    handler->flush_func = frame_handler_flush_fingerprints;

    return status_ok;
}

void frame_handler_dump(void *ignore,
			struct packet_info *pi,
			uint8_t *eth) {
    (void)ignore;

    packet_fprintf(stdout, eth, pi->len, pi->ts.tv_sec, pi->ts.tv_nsec / 1000);
    // printf_raw_as_hex(packet, tphdr->tp_len);

}

enum status frame_handler_dump_init(struct frame_handler *handler) {

    /* note: we leave handler->context uninitialized */
    handler->func = frame_handler_dump;
    handler->flush_func = NULL;

    return status_ok;
}
				    
