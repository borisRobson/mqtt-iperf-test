/*
 ============================================================================
 Name        : pi-client.c
 Author      : brandon
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "string.h"
#include "signal.h"
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <arpa/inet.h>
#include <linux/net.h>


#include "iperf.h"
#include "iperf_api.h"

#include "MQTTClient.h"

#include "configParser.h"

struct iperf_test *client_test;

struct thread_data{
	struct iperf_test* test;
};
struct thread_data client_data;
void* ThreadRun(void* arg);

void startClient();
int startTest = 0;
int run = 1;

#define FINISHED 0
#define START 1

void getLocalHost();

#define DEBUG 1

MQTTClient client;
volatile MQTTClient_deliveryToken deliveredtoken;
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void connlost(void *context, char *cause);
void delivered(void *context, MQTTClient_deliveryToken dt);
char* clientID;
char* ControlTopic;
char* StreamTopic;
char* IntervalTopic;
char* FinishedTopic;
char* ErrorTopic;

pthread_t client_thread;

char *localhost;

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
	char* payloadptr;

	printf("Message Arrived\n");
	printf("\tTopic: %s\n",topicName);

	payloadptr = message->payload;
	cJSON* json = cJSON_Parse(payloadptr);

	char* src = cJSON_GetObjectItem(json, "src")->valuestring;

	if(strcmp(topicName, ControlTopic) == 0)
	{
		char* msg = cJSON_GetObjectItem(json, "msg")->valuestring;
		printf("\t%s: %s\n", src, msg);
		free(msg);
		startTest = 1;
	}
	else if(strcmp(topicName, ErrorTopic) == 0)
	{
		char *cause = cJSON_GetObjectItem(json, "cause")->valuestring;
		printf("\tError from: %s\n\t cause: %s\n", src, cause);
		free(cause);
	}

	free(src);
	MQTTClient_freeMessage(&message);
	MQTTClient_free(topicName);

	return 1;
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("\tcause: %s\n", cause);
}

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

void MQTT_Publish(cJSON *output, int topic)
{
	cJSON_AddItemToObject(output, "host", cJSON_CreateString(localhost));

	char* msg = cJSON_Print(output);

	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token = 0;

	pubmsg.payload = msg;
	pubmsg.payloadlen = strlen(msg);
	pubmsg.qos = 1;
	pubmsg.retained = 0;
	
	switch(topic)
	{
		case START:
			MQTTClient_publishMessage(client, StreamTopic, &pubmsg, &token);
			break;
		case FINISHED:
			MQTTClient_publishMessage(client, FinishedTopic, &pubmsg, &token);
			break;		
	}
	

	while(deliveredtoken != token){;}

	free(msg);
}

void getLocalHost()
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[NI_MAXHOST];
	char eth[4];
	sprintf(eth, "eth0");

	if(getifaddrs(&ifaddr) == -1)
	{
		perror("getifaddrs");
		raise(SIGINT);
	}

	for(ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++)
	{
		if(ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;

		if(family == AF_INET)
		{
			s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if(s != 0)
			{
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				raise(SIGINT);
			}

			if(strlen(ifa->ifa_name) >= 3)
			{
				localhost = malloc(sizeof(host));
				sprintf(localhost, "%s",host );
			}
		}
	}

	freeifaddrs(ifaddr);

}

void sigintHandler()
{
	printf("\n%s\n", __FUNCTION__);

	MQTTClient_disconnect(client, 10000);
	MQTTClient_destroy(&client);
	iperf_free_test(client_test);

	exit(0);
}

int initMQTT(cJSON *config)
{
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	int rc;
	cJSON *mqtt = cJSON_GetObjectItem(config, "mqtt");
	char* broker = cJSON_GetObjectItem(mqtt, "Broker")->valuestring;
	clientID = cJSON_GetObjectItem(mqtt,"ClientID")->valuestring;
	ControlTopic = cJSON_GetObjectItem(mqtt, "ControlTopic")->valuestring;
	StreamTopic = cJSON_GetObjectItem(mqtt, "StreamTopic")->valuestring;
	IntervalTopic = cJSON_GetObjectItem(mqtt, "IntervalTopic")->valuestring;
	FinishedTopic = cJSON_GetObjectItem(mqtt, "FinishedTopic")->valuestring;
	ErrorTopic = cJSON_GetObjectItem(mqtt, "ErrorTopic")->valuestring;
	int QoS = atoi(cJSON_GetObjectItem(mqtt, "QoS")->valuestring);
	//int Timeout = atoi(cJSON_GetObjectItem(mqtt, "Timeout")->valuestring);

	MQTTClient_create(&client, broker, clientID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;

	MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

	if((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
	{
		printf("main() ERROR: Could not connect to broker\n");
		raise(SIGINT);
	}

	printf("client: %s subscribing to topics: \n"
			"\t%s\n"
			"\t%s\n", clientID, ControlTopic, ErrorTopic);
	MQTTClient_subscribe(client, ControlTopic, QoS);
	MQTTClient_subscribe(client, ErrorTopic, QoS);

	//free(mqtt);
	//free(broker);

	return rc;
}

int initIPERF(cJSON *config)
{
	cJSON *clientConfig = cJSON_GetObjectItem(config, "iperf-client");
	char *host = cJSON_GetObjectItem(clientConfig, "targetHost")->valuestring;
	int port = atoi(cJSON_GetObjectItem(clientConfig, "targetPort")->valuestring);
	int nstreams = atoi(cJSON_GetObjectItem(clientConfig, "nstreams")->valuestring);
	int bandwidth = atoi(cJSON_GetObjectItem(clientConfig, "bandwidth")->valuestring);
	int duration = atoi(cJSON_GetObjectItem(clientConfig, "duration")->valuestring);
	int interval = atoi(cJSON_GetObjectItem(clientConfig, "interval")->valuestring);

	client_test = iperf_new_test();
	iperf_defaults(client_test);
	iperf_set_test_role(client_test, 'c');
	iperf_set_test_server_hostname(client_test, host);
	iperf_set_test_server_port(client_test, port);
	iperf_set_test_num_streams(client_test, nstreams);
	iperf_set_test_rate(client_test, (bandwidth * MB));
	iperf_set_test_duration(client_test, duration);
	iperf_set_test_reporter_interval(client_test, interval);
	iperf_set_test_json_output(client_test, 1);
	//iperf_set_verbose(client_test, 1);
	//iperf_set_test_get_server_output(client_test, 1);

	if(client_test == NULL)
		return 0;
		
	MQTT_Publish(config, START);

	return 1;
}
void startClient()
{
	pthread_attr_t attr;
	int rc;
	void *status;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	rc = pthread_create(&client_thread, &attr, ThreadRun, (void*)&client_data);
	if(rc)
	{
		printf("ERROR; rc from pthread_create() is %d\n", rc);
		raise(SIGINT);
	}

	pthread_attr_destroy(&attr);
	rc = pthread_join(client_thread, &status);
	if(rc)
	{
		printf("ERROR; rc from pthread_join() is %d\n", rc);
		raise(SIGINT);
	}
	run = 0;
}

void* ThreadRun(void* arg)
{
	struct thread_data *t_data;
	t_data = (struct thread_data*)arg;

	if(iperf_run_client(t_data->test) < 0)
	{
		printf( " error - %s\n",   iperf_strerror( i_errno ) );
	}
	pthread_exit((void*)0);
}

void iperf_on_new_stream(struct iperf_stream *sp)
{
	if(DEBUG)
		printf("New Stream\n");
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

int iperf_json_finish(struct iperf_test *test)
{
	cJSON* sum = cJSON_GetObjectItem(test->json_end, "sum_sent");

	MQTT_Publish(sum, FINISHED);

    if (test->title)
	cJSON_AddStringToObject(test->json_top, "title", test->title);

    if (test->json_server_output) {
	cJSON_AddItemToObject(test->json_top, "server_output_json", test->json_server_output);
    }
    if (test->server_output_text) {
	cJSON_AddStringToObject(test->json_top, "server_output_text", test->server_output_text);
    }
    test->json_output_string = cJSON_Print(test->json_top);
    if (test->json_output_string == NULL)
        return -1;
    fprintf(test->outfile, "%s\n", test->json_output_string);

    iflush(test);
    cJSON_Delete(test->json_top);
    test->json_top = test->json_start = test->json_connected = test->json_intervals = test->json_server_output = test->json_end = NULL;

    return 0;
}

int main(void) {
	cJSON *config = readConfigFile();

	getLocalHost();
	
	if(initMQTT(config) != MQTTCLIENT_SUCCESS)
		raise(SIGINT);

	if(initIPERF(config) == 0)
		raise(SIGINT);	

	client_data.test = client_test;

	while(run == 1)
	{
		if(startTest == 1)
			startClient();
		sleep(1);
	}

	printf("client finished\n");

	raise(SIGINT);

	return EXIT_SUCCESS;
}

