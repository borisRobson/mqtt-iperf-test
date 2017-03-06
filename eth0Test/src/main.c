
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "signal.h"

#include "iperf.h"
#include "iperf_api.h"

#include "MQTTClient.h"

#include "configparser.h"
#include "initComponents.h"


#define DEBUG 1

void iperf_on_new_stream(struct iperf_stream *sp);
void iperf_on_test_start(struct iperf_test *test);
void iperf_on_connect(struct iperf_test *test);
void iperf_on_test_finish(struct iperf_test *test);
void startClient();

struct thread_data{
	struct iperf_test *test;
};

struct thread_data client_data, server_data;
pthread_t server_thread, client_thread;

volatile int startTest = 0;
volatile int run = 1;

#define FINISHED 0
#define START 1

struct iperf_test *client_test, *server_test;

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

char *localhost;
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
	char* payloadptr;

	printf("Message Arrived\n");
	printf("\tTopic: %s\n",topicName);

	payloadptr = message->payload;
	cJSON* json = cJSON_Parse(payloadptr);

	char* src = cJSON_GetObjectItem(json, "src")->valuestring;

	if(strcmp(topicName, "/netTest/control") == 0)
	{
		char* msg = cJSON_GetObjectItem(json, "msg")->valuestring;
		printf("\t%s: %s\n", src, msg);
		free(msg);
		startTest = 1;
	}
	else if(strcmp(topicName, "/netTest/error") == 0)
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

void *ThreadRun(void *arg)
{
	struct thread_data *t_data;
	t_data = (struct thread_data*)arg;

	if(t_data->test->sender == 0)
	{
		int consecutive_errors = 0;
		if(iperf_run_server(t_data->test) < 0){
			fprintf(stderr, "error: %s\n", iperf_strerror(i_errno));
			++consecutive_errors;
			if(consecutive_errors >=5){
				fprintf(stderr, "too many errors, exiting\n");
			}
		}else
			consecutive_errors = 0;
		iperf_reset_test(server_test);
	}
	else
	{
		if(iperf_run_client(t_data->test) < 0)
		{
			printf( " error - %s\n",   iperf_strerror( i_errno ) );
		}
	}
	pthread_exit((void*)0);
}

void sigintHandler()
{
	printf("\n%s\n", __FUNCTION__);
	MQTTClient_disconnect(client, 10000);
	MQTTClient_destroy(&client);
	iperf_free_test(client_test);
	iperf_free_test(server_test);
	exit(0);
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

void iperf_on_test_finish(struct iperf_test *test)
{
	if(DEBUG)
		printf("Test Finished\n");
}

int iperf_json_finish(struct iperf_test *test)
{
	if(test->sender == 1)
	{
		cJSON* sum = cJSON_GetObjectItem(test->json_end, "sum_sent");
		MQTT_Publish(sum, FINISHED);
	}

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

    //iflush(test);
    cJSON_Delete(test->json_top);
    test->json_top = test->json_start = test->json_connected = test->json_intervals = test->json_server_output = test->json_end = NULL;

    return 0;
}

void startClient()
{
	pthread_attr_t attr;
	int rc;
	void *status;
	//pthread_t client_thread;

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
	rc = pthread_join(server_thread, &status);
	if(rc)
	{
		printf("ERROR; rc from pthread_join() is %d\n", rc);
		raise(SIGINT);
	}
	run = 0;
	startTest = 0;
}

int main()
{
	printf("%s\n", __FUNCTION__);
	signal(SIGINT, sigintHandler);

	cJSON* config = readConfigFile();

	char* host = getLocalHost();
	localhost = host;

	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	int mqtt_rc;

	cJSON *mqtt = cJSON_GetObjectItem(config, "mqtt");
	char* broker = cJSON_GetObjectItem(mqtt, "broker")->valuestring;
	clientID = cJSON_GetObjectItem(mqtt, "ClientID")->valuestring;
	ControlTopic = cJSON_GetObjectItem(mqtt, "ControlTopic")->valuestring;
	StreamTopic = cJSON_GetObjectItem(mqtt, "StreamTopic")->valuestring;
	IntervalTopic = cJSON_GetObjectItem(mqtt, "IntervalTopic")->valuestring;
	FinishedTopic = cJSON_GetObjectItem(mqtt, "FinishedTopic")->valuestring;
	ErrorTopic = cJSON_GetObjectItem(mqtt, "ErrorTopic")->valuestring;
	int QoS = atoi(cJSON_GetObjectItem(mqtt, "QoS")->valuestring);

	if(MQTTCLIENT_SUCCESS != MQTTClient_create(&client, broker, clientID, MQTTCLIENT_PERSISTENCE_NONE, NULL))
	{
		printf("Failed to create MQTT Client\n");
		raise(SIGINT);
	}
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;

	MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);

	if((mqtt_rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
	{
		printf("MQTT Client failed to connect, rc: %d\n", mqtt_rc);
		raise(SIGINT);
	}

	printf("client: %s subscribing to topics: \n"
			"\t%s\n"
			"\t%s\n", clientID, ControlTopic, ErrorTopic);
	MQTTClient_subscribe(client, ControlTopic, QoS);
	MQTTClient_subscribe(client, ErrorTopic, QoS);

	client_test = iperf_new_test();
	initIperfClient(config, client_test);

	server_test = iperf_new_test();
	initIperfServer(config, server_test);

	client_data.test = client_test;
	server_data.test = server_test;

	MQTT_Publish(config, START);

	pthread_attr_t attr;
	int rc;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	rc = pthread_create(&server_thread, &attr, ThreadRun, (void*)&server_data);
	if(rc)
	{
		printf("ERROR; rc from pthread_create() is %d\n", rc);
		raise(SIGINT);
	}
	pthread_attr_destroy(&attr);

	while(run == 1)
	{
		if(startTest == 1)
			startClient();
		sleep(1);
	}

	printf("client finished\n");

	raise(SIGINT);

	return 0;
}
