/*
 * configparser.c
 *
 *  Created on: Feb 28, 2017
 *      Author: standby
 */

#include "configparser.h"

cJSON* readConfigFile()
{
	FILE *fp;
	char* line;
	char* config;
	cJSON *json = NULL;

	line = malloc(64 * sizeof(char));
	config = malloc(512 * sizeof(char));

	fp = fopen("/nvdata/tftpboot/config.json", "r");

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



