/*
 ============================================================================
 Name        : mqtt-iperf-client.c
 Author      : brandon
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"
#include <arpa/inet.h>
#include "iperf.h"
#include "iperf_api.h"
#include "iperf_tcp.h"
#include "timer.h"
#include "cjson.h"
#include "units.h"
#include "tcp_window_size.h"
#include "iperf_util.h"
#include "iperf_locale.h"
#include "version.h"
#include "pthread.h"
#include "MQTTClient.h"

/*Define MQTT properties and callbacks*/
#define ADDRESS "tcp://10.10.40.96:1883"
#define CLIENTID "iperfClient"
#define TOPIC "mqtt-iperf-test"
#define QOS 1
#define TIMEOUT 10000L
#define TYPESTREAM "msg_new_stream"
#define TYPEINTERVAL "msg_interval"

volatile MQTTClient_deliveryToken deliveredToken;
MQTTClient client;
char* localhost;
char* msg_src;

void delivered(void *context, MQTTClient_deliveryToken dt);
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void connlost(void *context, char *cause);
void sendmqttmsg(cJSON *msg);

/* Define iperf props and callbacks */
#define TESTDURATION 5
#define TESTINTERVAL 1
#define NSTREAMS 2
char *host;
int port;

void iperf_reporter_callback(struct iperf_test *test);
void iperf_on_test_start(struct iperf_test *test);
void iperf_on_connect(struct iperf_test *test);
void iperf_on_new_stream(struct iperf_stream *sp);
void iperf_on_test_finish(struct iperf_test *test);
static void iperf_print_intermediate(struct iperf_test *test);
static void print_interval_results(struct iperf_test *test, struct iperf_stream *sp, cJSON *json_interval_streams);

/* Global defs */
#define DEBUG 1

/* This converts an IPv6 string address from IPv4-mapped format into regular
** old IPv4 format, which is easier on the eyes of network veterans.
**
** If the v6 address is not v4-mapped it is left alone.
*/
static void
mapped_v4_to_regular_v4(char *str)
{
    char *prefix = "::ffff:";
    int prefix_len;

    prefix_len = strlen(prefix);
    if (strncmp(str, prefix, prefix_len) == 0) {
	int str_len = strlen(str);
	memmove(str, str + prefix_len, str_len - prefix_len + 1);
    }
}

/* MQTT Callbacks */
void delivered(void *context, MQTTClient_deliveryToken dt)
{
	printf("Message with token value %d delivery confirmed\n", dt);
	deliveredToken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
	return 1;
}

void connlost(void *context, char *cause)
{

}

void sendmqttmsg(cJSON *msg)
{
	int socket = cJSON_GetObjectItem(msg, "socket")->valueint;
	printf("socket: %d\n", socket);
	//printf("msg: %s\n", cJSON_Print(msg));
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;

	char* message = cJSON_Print(msg);

	pubmsg.payload = message;
	pubmsg.payloadlen = strlen(message);
	pubmsg.qos = QOS;
	pubmsg.retained = 0;
	deliveredToken = 0;
	MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
}

/* IPERF Callbacks */

void connect_msg(struct iperf_stream *sp)
{
	char ipl[INET6_ADDRSTRLEN], ipr[INET6_ADDRSTRLEN];
	int lport, rport;

	if(getsockdomain(sp->socket) == AF_INET)
	{
		inet_ntop(AF_INET, (void*)&((struct sockaddr_in*)&sp->local_addr)->sin_addr, ipl, sizeof(ipl));
		mapped_v4_to_regular_v4(ipl);
		inet_ntop(AF_INET, (void *)&((struct sockaddr_in *)&sp->remote_addr)->sin_addr, ipr, sizeof(ipr));
		mapped_v4_to_regular_v4(ipr);
		lport = ntohs(((struct sockaddr_in *)&sp->local_addr)->sin_port);
		rport = ntohs(((struct sockaddr_in *)&sp->remote_addr)->sin_port);
	}else{
		inet_ntop(AF_INET6, (void *)&((struct sockaddr_in6*)&sp->local_addr)->sin6_addr, ipl, sizeof(ipl));
		mapped_v4_to_regular_v4(ipl);
		inet_ntop(AF_INET6, (void *)&((struct sockaddr_in6*)&sp->remote_addr)->sin6_addr, ipr, sizeof(ipr));
		mapped_v4_to_regular_v4(ipr);
		lport = ntohs(((struct sockaddr_in6*)&sp->local_addr)->sin6_port);
		rport = ntohs(((struct sockaddr_in6*)&sp->remote_addr)->sin6_port);
	}

	localhost = malloc(sizeof(ipl));

	sprintf(localhost, "%s", ipl);

	cJSON *streamInfo = iperf_json_printf("msg_type: %s socket: %d local_host: %s local_port: %d remote_host: %s remote_port: %d",TYPESTREAM, (int64_t)sp->socket, ipl, (int64_t)lport, ipr, (int64_t)rport);
	cJSON_AddItemToArray(sp->test->json_connected, streamInfo);
	sendmqttmsg(streamInfo);
}

void iperf_on_new_stream(struct iperf_stream *sp)
{
	if(DEBUG)
		printf("New Stream\n");
	connect_msg(sp);
}

void iperf_on_test_start(struct iperf_test *test)
{
	if(DEBUG)
		printf("Test Starting\n");

}
void iperf_on_connect(struct iperf_test *test)
{
	if(DEBUG)
		printf("IPERF Client Connected\n");
}

void iperf_on_test_finish(struct iperf_test *test)
{
	if(DEBUG)
		printf("Test Finished\n");
}

void iperf_reporter_callback(struct iperf_test *test)
{
	iperf_print_intermediate(test);
}

static void iperf_print_intermediate(struct iperf_test *test)
{
	char ubuf[UNIT_LEN];
	char nbuf[UNIT_LEN];
	struct iperf_stream *sp = NULL;
	struct iperf_interval_results *irp;
	iperf_size_t bytes = 0;
	double bandwidth;
	int retransmits = 0;
	double start_time, end_time;
	cJSON *json_interval;
	cJSON *json_interval_streams;

	/*
	 * Create and init JSON object (parent) & array (for multiple streams)
	 */
	json_interval = cJSON_CreateObject();
	if(json_interval == NULL)
	{
		fprintf(stderr, "json_interval == NULL\n");
		return;
	}
	cJSON_AddItemToArray(test->json_intervals, json_interval);
	json_interval_streams = cJSON_CreateArray();
	if(json_interval_streams == NULL)
	{
		fprintf(stderr, "json_interval_streams == NULL");
		return;
	}
	cJSON_AddItemToObject(json_interval, "Streams", json_interval_streams);

	/*
	 * Print interval results for each stream
	 * Sum bytes sent & retransmission count
	 */
	SLIST_FOREACH(sp, &test->streams, streams)
	{
		print_interval_results(test, sp, json_interval_streams);
		/* sum up all streams */
		irp = TAILQ_LAST(&sp->result->interval_results, irlisthead);
		if(irp == NULL)
		{
			iperf_err(test, "iperf_print_intermediate error: interval_result is NULL");
			return;
		}
		bytes += irp->bytes_transferred;
		if(test->sender && test->sender_has_retransmits)
			retransmits += irp->interval_retrans;
	}

	/*
	 * If multiple streams, build sum string
	 */
	if(test->num_streams > 1)
	{
		sp = SLIST_FIRST(&test->streams); //reset to first stream
		if(sp)
		{
			irp = TAILQ_LAST(&sp->result->interval_results, irlisthead); // use 1st stream for timing info

			unit_snprintf(ubuf, UNIT_LEN, (double)bytes, 'A');
			bandwidth = (double)bytes / (double)irp->interval_duration;
			unit_snprintf(nbuf, UNIT_LEN, bandwidth, test->settings->unit_format);

			start_time = timeval_diff(&sp->result->start_time, &irp->interval_start_time);
			end_time = timeval_diff(&sp->result->start_time, &irp->interval_end_time);

			if(test->sender && test->sender_has_retransmits)
				cJSON_AddItemToObject(json_interval, "sum", iperf_json_printf("start: %f end: %f second: %f bytes: %d bits_per_second: %f retransmits: %d", (double)start_time, (double)end_time, (double)irp->interval_duration, (int64_t)bytes, bandwidth * 8, (int64_t)retransmits));
			else
				cJSON_AddItemToObject(json_interval, "sum", iperf_json_printf("start: %f end: %f second: %f bytes: %d bits_per_second: %f ", (double)start_time, (double)end_time, (double)irp->interval_duration, (int64_t)bytes, bandwidth * 8));
		}
	}
}

static void print_interval_results(struct iperf_test *test, struct iperf_stream* sp, cJSON *json_interval_streams)
{
	char ubuf[UNIT_LEN];
	char nbuf[UNIT_LEN];

	double st=0., et=0.;
	struct iperf_interval_results *irp = NULL;
	double bandwidth;

	irp = TAILQ_LAST(&sp->result->interval_results, irlisthead); /* get last entry in linked list */
	if(irp == NULL)
	{
		iperf_err(test, "print_interval_results error: interval_results is NULL");
		return;
	}

	unit_snprintf(ubuf, UNIT_LEN, (double)(irp->bytes_transferred), 'A');
	bandwidth = (double)(irp->bytes_transferred) / (double) irp->interval_duration;
	unit_snprintf(nbuf, UNIT_LEN, bandwidth, test->settings->unit_format);

	st = timeval_diff(&sp->result->start_time, &irp->interval_start_time);
	et = timeval_diff(&sp->result->start_time, &irp->interval_end_time);

	cJSON *intervalReport;
	msg_src = malloc(sizeof(localhost) + 8);
	sprintf(msg_src, "%s:%d", localhost, (int64_t)sp->socket);

	if(test->sender && test->sender_has_retransmits)
		intervalReport = iperf_json_printf("msg_src: %s msg_type: %s socket: %d start: %f end: %f seconds: %f bytes: %d bits_per_second: %d retransmits: %d",msg_src, TYPEINTERVAL,  (int64_t)sp->socket, (double)st, (double)et, (double) irp->interval_duration, (uint64_t)irp->bytes_transferred, (int)bandwidth * 8, (int64_t)irp->interval_retrans);
	else
		intervalReport = iperf_json_printf("msg_src: %s msg_type: %s socket: %d start: %f end: %f seconds: %f bytes: %d bits_per_second: %d",msg_src ,TYPEINTERVAL, (int64_t)sp->socket, (double)st, (double)et, (double) irp->interval_duration, (uint64_t)irp->bytes_transferred, (int)bandwidth * 8);

	cJSON_AddItemToArray(json_interval_streams, intervalReport);
	free(msg_src);
	sendmqttmsg(intervalReport);
}



int main(int argc, char* argv[]) {

	/* #region INIT_MQTT */

	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	int rc;

	if(MQTTCLIENT_SUCCESS != MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL))
	{
		printf("Failed to create MQTT Client\n");
		exit(-1);
	}
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;

	MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

	if((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
	{
		printf("MQTT Client failed to connect, rc: %d\n", rc);
		exit(-1);
	}
	/*# end region */

	/* region INIT_IPERF */

	char* argv0;
	struct iperf_test *test;
	host = malloc(24 * sizeof(char));

	argv0 = strchr(argv[0], '/');
	if(argv0 != (char*)0)
		++argv0;
	else
		argv0 = argv[0];

	if(argc != 3)
	{
		fprintf(stderr, "usage: %s [host] [port]\n", argv0);
		exit(EXIT_FAILURE);
	}
	host = argv[1];
	port = atoi (argv[2]);

	test = iperf_new_test();
	if(test == NULL)
	{
		fprintf(stderr, "%s: failes to create test\n", argv0);
		exit(EXIT_FAILURE);
	}
	iperf_defaults(test);
	iperf_set_verbose(test, 1);

	iperf_set_test_role(test, 'c');
	iperf_set_test_server_hostname(test, host);
	iperf_set_test_server_port(test, port);
	iperf_set_test_duration(test, TESTDURATION);
	iperf_set_test_reporter_interval(test, TESTINTERVAL);
	iperf_set_test_stats_interval(test, TESTINTERVAL);
	iperf_set_test_json_output(test, 1);
	iperf_set_test_num_streams(test, NSTREAMS);

	/* #endregion IPERF_INIT */

	/* #region RUN */

	if(iperf_run_client(test) < 0)
	{
		fprintf(stderr, "%s: error - %s\n", argv0,  iperf_strerror( i_errno ) );
		exit(EXIT_FAILURE);
	}

	if(iperf_get_test_json_output_string(test))
	{
		fprintf(iperf_get_test_outfile(test), "%zd bytes of JSON emitted\n", strlen(iperf_get_test_json_output_string(test)));
	}

	iperf_free_test(test);

	exit(EXIT_SUCCESS);
}
