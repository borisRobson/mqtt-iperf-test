/*
 * configParser.c
 *
 *  Created on: Mar 2, 2017
 *      Author: standby
 */

#include "configParser.h"

#include "configParser.h"

cJSON* readConfigFile()
{
	FILE *fp;
	char* line;
	char* config;
	cJSON *json = NULL;

	line = malloc(64 * sizeof(char));
	config = malloc(512 * sizeof(char));

	fp = fopen("/home/pi/pi-netTest/src/config.json", "r");

	while(!feof(fp))
	{
		fgets(line,64, fp);
		strcat(config, line);
	}

	json = cJSON_Parse(config);

	free(line);
	free(config);

	if(json == NULL)
	{
		printf("Error parsing json\n");
	}

	return json;
}
