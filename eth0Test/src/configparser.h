/*
 * configparser.h
 *
 *  Created on: Feb 28, 2017
 *      Author: standby
 */

#ifndef SRC_CONFIGPARSER_H_
#define SRC_CONFIGPARSER_H_

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

#include "iperf.h"
#include "iperf_api.h"

cJSON* readConfigFile();

#endif /* SRC_CONFIGPARSER_H_ */
