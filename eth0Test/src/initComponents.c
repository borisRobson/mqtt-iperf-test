/*
 * initComponents.c
 *
 *  Created on: Feb 28, 2017
 *      Author: standby
 */

#include "initComponents.h"

#define MB (1024 * 1024)

char* getLocalHost()
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

	/*
	 * Walk through linked list, maintaining head pointer so we
	 * can free list later
	 */

	for(ifa = ifaddr, n = 0; ifa != NULL ; ifa = ifa->ifa_next, n++)
	{
		if(ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;

		/* For AF_INET* interface addresses display the address */

		if(family == AF_INET || family == AF_INET6)
		{
			s = getnameinfo(ifa->ifa_addr,
					(family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)
							,host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if(s != 0)
			{
				printf("getnameinfo() failed: %s\n", gai_strerror(s));
				raise(SIGINT);
			}

			if(strlen(ifa->ifa_name) >=3  && family == AF_INET )
			{
				localhost = malloc(sizeof(host));
				sprintf(localhost, "%s", host);
				printf("setting localhost: %s\n", localhost);
			}
		}
	}

	freeifaddrs(ifaddr);

	return localhost;
}



void initIperfClient(cJSON* config, struct iperf_test *client)
{
	/*
	 * Get config params from json data
	 */
	cJSON *clientConfig = cJSON_GetObjectItem(config, "iperf-client");
	char *host = cJSON_GetObjectItem(clientConfig, "targetHost")->valuestring;
	int port = atoi(cJSON_GetObjectItem(clientConfig, "targetPort")->valuestring);
	int nstreams = atoi(cJSON_GetObjectItem(clientConfig, "nstreams")->valuestring);
	int bandwidth = atoi(cJSON_GetObjectItem(clientConfig, "bandwidth")->valuestring);
	int duration = atoi(cJSON_GetObjectItem(clientConfig, "duration")->valuestring);
	int interval = atoi(cJSON_GetObjectItem(clientConfig, "interval")->valuestring);

	iperf_defaults(client);
	iperf_set_test_role(client, 'c');
	iperf_set_test_server_hostname(client, host);
	iperf_set_test_server_port(client, port);
	iperf_set_test_num_streams(client, nstreams);
	iperf_set_test_rate(client, (bandwidth * MB));
	iperf_set_test_duration(client, duration);
	iperf_set_test_reporter_interval(client, interval);
	iperf_set_test_json_output(client, 1);
	iperf_set_verbose(client, 1);
}


void initIperfServer(cJSON *config, struct iperf_test* server)
{
	cJSON *serverConfig = cJSON_GetObjectItem(config, "iperf-server");
	int port = atoi(cJSON_GetObjectItem(serverConfig, "s-port")->valuestring);

	iperf_defaults(server);
	iperf_set_test_role(server, 's');
	iperf_set_test_server_port(server, port);
	iperf_set_test_json_output(server, 1);
	iperf_set_test_reporter_interval(server, 1);
	iperf_set_test_json_output(server, 1);
	iperf_set_verbose(server, 1);
}
