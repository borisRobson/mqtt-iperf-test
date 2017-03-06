/*
 * initComponents.h
 *
 *  Created on: Feb 28, 2017
 *      Author: standby
 */

#ifndef SRC_INITCOMPONENTS_H_
#define SRC_INITCOMPONENTS_H_

#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <linux/if_link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <linux/net.h>
#include <pthread.h>
#include <ctype.h>
#include "signal.h"

#include "iperf.h"
#include "iperf_api.h"

#include "MQTTClient.h"

char* localhost;

char* getLocalHost();
void initIperfClient(cJSON* config, struct iperf_test* client);
void initIperfServer(cJSON *config, struct iperf_test* server);

#endif /* SRC_INITCOMPONENTS_H_ */
