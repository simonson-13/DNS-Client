#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close

#define MAXDNSSIZE 65535
#define MAXRECORDS 50
#define MAXDOMAINSIZE 260
#define TIMEOUT 1000000

/*
PUT

-WALL

BACK

INTO

THE 

MAKE

FILE
*/

struct question {
	int length;
	char name[MAXDOMAINSIZE];
	uint16_t type;
	uint16_t class;
}Question;

struct resourceRecord {
	int length;
	char name[MAXDOMAINSIZE];
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t dataLength;
	char data[MAXDOMAINSIZE];
}ResourceRecord;

struct message {
	int length;
	char message[MAXDNSSIZE];
	uint16_t id;
	uint16_t flags;
	uint16_t numOfQuestions;
	uint16_t numOfAnswerRecords;
	uint16_t numOfAuthorityRecords;
	uint16_t numOfAdditionalRecords;

	struct question *questions[MAXRECORDS];
	struct resourceRecord *answerRecords[MAXRECORDS];
	struct resourceRecord *authorityRecords[MAXRECORDS];
	struct resourceRecord *additionalRecords[MAXRECORDS];
}Message;

int idCount = 1;

void parseBytes(char *iterator, char *inBuffer, uint16_t length) {
	for(int i = 0; i < length; i++) {
		inBuffer[i] = iterator[i];
	}
}

void parseIP(char *iterator, char *inBuffer, int DNSFormatWanted) {
	//if DNS format is not wanted
	if(DNSFormatWanted == 0) {
		// printf("first int of address: %u\n", *(uint8_t *)&iterator[0]);
		sprintf(inBuffer, "%u.%u.%u.%u", *(uint8_t *)&iterator[0], *(uint8_t *)&iterator[1], *(uint8_t *)&iterator[2], *(uint8_t *)&iterator[3]);

		// for(int i = 0; i < 4; i++) {
		// 	inBuffer[i*2] = iterator[i];
		// 	inBuffer[i*2 + 1] = '.';
		// }
		// inBuffer[7] = '\0'; //x.x.x.x\0

	//if DNS format is wanted
	} else if(DNSFormatWanted == 1){
		//to delete
		sprintf(inBuffer, "%u.%u.%u.%u", *(uint8_t *)&iterator[0], *(uint8_t *)&iterator[1], *(uint8_t *)&iterator[2], *(uint8_t *)&iterator[3]);
		// for(int i = 0; i < 4; i++) 
		// 	inBuffer[i] = iterator[i];
		// inBuffer[4] = '\0'; //xxxx\0
	}
	// printf("parsedIP is: %s\n", inBuffer);
}

int parseName(char *response, char *iterator, char *inBuffer, uint16_t dataLength, int DNSFormatWanted, int flag) {
	int compressionNotDetected = 1;
	// printf("Entered parseName function\n");
	// read in bytes into inBuffer
	for(int i = 0; i < dataLength; i++) {
		inBuffer[i] = iterator[i];
		if(i == dataLength-1 && inBuffer[i] != '\0') {
			inBuffer[i+1] = '\0';
		}
	}
	// printf("data is: %s\n", inBuffer);
	// for(int i = 0; i < 10; i++) printf("%x ", inBuffer[i]);
	// strncpy(inBuffer, iterator, dataLength);
	// if(inBuffer[dataLength-1] != '\0')
	// 	inBuffer[dataLength] = '\0';
	// for(int i = 0; i < 10; i++) printf("%x ", iterator[i]);
	uint16_t offset;
	int index = 0;
	while(inBuffer[index] != '\0') {
		// printf("inBuffer[index] is: %u\nflag is: %d\n", inBuffer[index], flag);
		if(*(uint8_t *)&inBuffer[index] >= 192 && flag == 0) {
			compressionNotDetected = 0;
			// printf("offset.first: %02x\n", ntohs(*(uint16_t *)&inBuffer[index]));
			offset = ntohs(*(uint16_t *)&inBuffer[index]) & (uint16_t)16383;
			// printf("offset is: %u\n", offset);
			sprintf(&inBuffer[index], "%s", response + offset);
		} else {
			// for(int i = 0; i < 10; i++) printf("%x ", inBuffer[i]);
			uint8_t temp = inBuffer[index];
			// printf("temp is: %u\n", temp);
			if(DNSFormatWanted == 0) 
				inBuffer[index] = '.';
			index += (temp + 1);
		}
	}
	// printf("in parseName function, data is: %s\n", inBuffer);
	//performing left shift of all values in inbuffer
	if(DNSFormatWanted == 0) {
		int length = strlen(inBuffer);
		for(int i = 0; i < length; i++) 
			inBuffer[i] = inBuffer[i+1];
	}

	
	// // printf("the parsed name is: %s\n", inBuffer);
	return compressionNotDetected;
}

void printRecord(char *ip, struct resourceRecord *record) {
	if(record->type != 1 && record->type != 2 && record->type != 5) {
		return;
	}

	//ip
	char *ns_address = ip;

	//domain
	char domain[MAXDOMAINSIZE];
	parseName(NULL, record->name, domain, strlen(record->name) + 1, 0, 1);
	domain[strlen(domain) + 1] = '\0';
	domain[strlen(domain)] = '.';

	//type 
	char type[6];
	if(record->type == 1) sprintf(type, "%s", "A");
	else if(record->type == 2) sprintf(type, "%s", "NS");
	else if(record->type == 5) sprintf(type, "%s", "CNAME");

	//data
	char data[MAXDOMAINSIZE];
	if(record->type == 1) {
		sprintf(data, "%s", record->data);
	} else if(record->type == 2 || record->type == 5) {
		parseName(NULL, record->data, data, strlen(record->data), 0, 1);
		data[strlen(data) + 1] = '\0';
		data[strlen(data)] = '.';
	} else {
		return;
	}

	printf("%s: %s %s %s\n", ns_address, domain, type, data);
	fflush(stdout);
}

void parseMessage(char *ip, struct message *parsedResponse, int doNotPrint) {
	char *response = parsedResponse->message;

	//parsing header
	parsedResponse->id = ntohs(*(uint16_t *)&response[0]);						//id
	parsedResponse->flags = ntohs(*(uint16_t *)&response[2]);					//flags
	parsedResponse->numOfQuestions = ntohs(*(uint16_t *)&response[4]);			//num questions
	parsedResponse->numOfAnswerRecords = ntohs(*(uint16_t *)&response[6]);		//num answers
	parsedResponse->numOfAuthorityRecords = ntohs(*(uint16_t *)&response[8]);	//num authority
	parsedResponse->numOfAdditionalRecords = ntohs(*(uint16_t *)&response[10]);	//num additonal

	//parsing body
	char *iterator = response + 12;
	//reading in all questions
	for(int i = 0; i < parsedResponse->numOfQuestions; i++) {
		parsedResponse->questions[i] = malloc(sizeof(Question));
		int length = 0;

		parseName(response, iterator, parsedResponse->questions[i]->name, strlen(iterator), 1, 0);
		length += (strlen(iterator) + 1);

		parsedResponse->questions[i]->type = ntohs(*(uint16_t *)&iterator[length]);
		length += 2;

		parsedResponse->questions[i]->class = ntohs(*(uint16_t *)&iterator[length]);
		length += 2;
		
		parsedResponse->questions[i]->length = length;
		iterator += length;
	}

	for(int j = 0; j < 3; j++) {
		int max;
		struct resourceRecord **currRecords;
		switch(j) {
			case 0:
				//answer records
				max = parsedResponse->numOfAnswerRecords;
				currRecords = parsedResponse->answerRecords;
				break;
			case 1:
				//authority records
				max = parsedResponse->numOfAuthorityRecords;
				currRecords = parsedResponse->authorityRecords;
				break;
			case 2:
				//additional records
				max = parsedResponse->numOfAdditionalRecords;
				currRecords = parsedResponse->additionalRecords;
				break;
			default:
				//impossible occurance
				;
		}

		for(int i = 0; i < max; i++) {
			currRecords[i] = malloc(sizeof(ResourceRecord));
			int length = 0;
			
			int addOn = parseName(response, iterator, currRecords[i]->name, strlen(iterator), 1, 0);
			length += (strlen(iterator) + addOn);

			currRecords[i]->type = ntohs(*(uint16_t *)&iterator[length]);
			length += 2;

			currRecords[i]->class = ntohs(*(uint16_t *)&iterator[length]);
			length += 2;

			currRecords[i]->ttl = ntohl(*(uint32_t *)&iterator[length]);
			length += 4;

			currRecords[i]->dataLength = ntohs(*(uint16_t *)&iterator[length]);
			length += 2;

			if(currRecords[i]->type == 1) parseIP(iterator+length, currRecords[i]->data, 0);
			else if(currRecords[i]->type == 2 || currRecords[i]->type == 5) parseName(response, iterator+length, currRecords[i]->data, currRecords[i]->dataLength, 1, 0);
			else parseBytes(iterator+length, currRecords[i]->data, currRecords[i]->dataLength);

			length += currRecords[i]->dataLength;
			iterator += length;

			if(doNotPrint == 0) {
				printRecord(ip, currRecords[i]);
				fflush(stdout);
			}
		}
		fflush(stdout);
	}
}

void query(char *ip, uint16_t type, char *domain, int id, struct message *parsedResponse, int useRecursion, int doNotPrint, int *timedOut) {
	struct addrinfo addrCriteria;
	memset(&addrCriteria, 0, sizeof(addrCriteria));
	addrCriteria.ai_family = AF_UNSPEC;
	addrCriteria.ai_socktype = SOCK_DGRAM;
	addrCriteria.ai_protocol = IPPROTO_UDP;

	//structure to recieve messages
	struct sockaddr_storage fromAddr;
	socklen_t fromAddrLen = sizeof(fromAddr);

	//get addresses
	struct addrinfo *servAddr;
	int retVal = getaddrinfo(ip, "53", &addrCriteria, &servAddr);
	if(retVal != 0) {
		printf("ERROR: getting address - failed\n");
		exit(1);
	}

	//create a datagram/UDP socket
	int sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
	if(sock < 0) {
		printf("ERROR: connecting to socket - failed\n");
		exit(1);
	}

    //set the socket to have the timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT; //1000 ms = 1000000 ns
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

	//create message buffer
	int requestLength = (12 + strlen(domain) + 1 + 2 + 2);
	char *request = malloc(requestLength);
	
	//fill out header of request
	*(uint16_t *)&request[0] = htons(id);		//id

	if(useRecursion == 0)						//flags
		*(uint16_t *)&request[2] = htons(0x0000);
	else 
		*(uint16_t *)&request[2] = htons(0x0180);

	*(uint16_t *)&request[4] = htons(0x0001);	//other attributes
	*(uint16_t *)&request[6] = htons(0x0000);
	*(uint16_t *)&request[8] = htons(0x0000);
	*(uint16_t *)&request[10] = htons(0x0000);

	//fill out query body
	sprintf(&request[12], "%s", domain);							//domain
	*(uint16_t *)&request[12 + strlen(domain) + 1] = htons(type);	//type
	*(uint16_t *)&request[12 + strlen(domain) + 1 + 2] = htons(1);	//class

	//querying...
	ssize_t bytesSent = sendto(sock, request, requestLength, 0, servAddr->ai_addr, servAddr->ai_addrlen);
	if(bytesSent < 0) {
		printf("ERROR: sending message through socket - failed\n");
		exit(1);
	} else if(bytesSent != requestLength) {
		printf("ERROR: sent unexpected number of bytes through socket - failed\n");
		exit(1);
	}

	free(request);

	//reading in response...
	int bytesRecieved = recvfrom(sock, parsedResponse->message, MAXDNSSIZE, 0, (struct sockaddr *)&fromAddr, &fromAddrLen);
	if(bytesRecieved < 0) {
		// printf("TIMEOUT OCCURED\n");
		*timedOut = 1;
		return;
	}

	//parse response
	parsedResponse->length = bytesRecieved;
	parseMessage(ip, parsedResponse, doNotPrint);

	//print records
	//...

}
// void DNStoDottedFormat(char *input, char *result) {
	
// }
void dottedToDNSFormat(char *input, char *result) {
	char *tempInput = malloc(strlen(input) + 1);
	sprintf(tempInput, "%s", input);
	result[0] = '\0';

	int index = 0;
	char *token = strtok(tempInput, ".");
	while(token != NULL) {
		//set length
		uint8_t tokenLength = (uint8_t)(strlen(token));
		*(uint8_t *)&result[index++] = tokenLength;
		//set token
		sprintf(result+index, "%s", token);
		index += (tokenLength);

		//get next token
		token = strtok(NULL, ".");
	}
	free(tempInput);
}

void getDomains(char *ip, struct message *parsedResponse, struct resourceRecord **records, int numRecords, char *domains[MAXDOMAINSIZE][2]) {
    for(int i = 0; i < numRecords; i++) {
		sprintf(domains[i][0], "%s", records[i]->data); //note: we want it in DNS format

        //first see if this domains ip address is already in the additional records
        int found = 0;
        for(int j = 0; j < parsedResponse->numOfAdditionalRecords; j++) {
            if(strcmp(domains[i][0], parsedResponse->additionalRecords[j]->name) == 0 && parsedResponse->additionalRecords[j]->type == 1) {
                sprintf(domains[i][1], "%s", parsedResponse->additionalRecords[j]->data);
                found = 1;
                break;
            }
        }
        if(found == 1) continue; //skip the querying if we found this domain in the additional records

		int *timedOut = malloc(sizeof(int));
		*timedOut = 0;
		//query for the ip address of this name server
		struct message *tempMessage = malloc(sizeof(Message));
		query("8.8.8.8\0", 1, domains[i][0], idCount++, tempMessage, 1, 1, timedOut); //use recursion

		//if query timed out
		if(*timedOut == 1) {
			printf("ERROR: timed out when recursively retrieving IP address of nameservers\n");
			exit(1);
		}

		//save name into index 1 in "domains" array
		if(tempMessage->numOfAnswerRecords <= 0) {
			// printf("ERROR: when getting ip address of name server USING recursion, it failed\n");
		} else {
            sprintf(domains[i][1], "%s", tempMessage->answerRecords[0]->data);
        }
		free(tempMessage);
	}
}

int getRecordByType(struct resourceRecord **currRecords, uint16_t max, uint16_t type, struct resourceRecord **desiredRecord) {
	for(int i = 0; i < max; i++) {
		if(currRecords[i]->type == type) {
			*desiredRecord = currRecords[i];
			return 1;
		}
	}
	return 0;
}

int getRecordByNameAndType(struct resourceRecord **records, char *name, uint16_t max, uint16_t type, char *desiredIP) {
	for(int i = 0; i < max; i++) {
		//check type, then name
		if(records[i]->type == type && strcmp(records[i]->name, name) == 0) {
			sprintf(desiredIP, "%s",records[i]->data);
			return 1;
		} 
	}
	return 0;
}

void deleteParsedResponse(struct message *parsedResponse) {
	//delete questions
	for(int i = 0; i < parsedResponse->numOfQuestions; i++) 
		free(parsedResponse->questions[i]);
	
	//delete answer records
	for(int i = 0; i < parsedResponse->numOfAnswerRecords; i++) 
		free(parsedResponse->answerRecords[i]);

	//delete authority records
	for(int i = 0; i < parsedResponse->numOfAuthorityRecords; i++)
		free(parsedResponse->authorityRecords[i]);

	//delete additional records
	for(int i = 0; i < parsedResponse->numOfAdditionalRecords; i++)
		free(parsedResponse->additionalRecords[i]);
	
	//delete entire message structure
	free(parsedResponse);
}


int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);
	//function variables

	//parse command line arguments
	char *bootstrap_ns_address = NULL, *tempDomain = NULL;
	for(int i = 1; i < argc; i+=2) {
		if(strcmp(argv[i], "-a") == 0) 
			bootstrap_ns_address = argv[i+1];
		else if(strcmp(argv[i], "-d") == 0)
			tempDomain = argv[i+1];
		else {
			printf("ERROR: unknown argument in command line: %s\n", argv[i]);
			return EXIT_FAILURE;
		}
	}
	if(bootstrap_ns_address == NULL || tempDomain == NULL) {
		printf("ERROR: not all required command line arguments were given\n");
		return EXIT_FAILURE;
	}

	//change domain to DNS format
	//1 for extra length byte of first token
	//1 for null terminating byte
	char *domain = malloc(strlen(tempDomain) + 1 + 1); 
	dottedToDNSFormat(tempDomain, domain);

	//timeout flag
	int *timedOut = malloc(sizeof(int));
	*timedOut = 0;

	//BEGIN ITERATIVE ALGORITHM

	//--- line 2 ---
	//query for root servers
	struct message *parsedResponse2 = malloc(sizeof(Message));
	query(bootstrap_ns_address, 2, "\0", idCount++, parsedResponse2, 0, 1, timedOut);

	//if the query timed out
	if(*timedOut == 1) {
		printf("ERROR: timed out when retrieving name servers of root\n");
		// *timedOut = 0;
		exit(1);
	}

	//get root domain servers - initilization of variables needed
	int index, max, root_ns_domains_max;
	char *root_ns_domains[MAXRECORDS][2], *local_ns_domains[MAXRECORDS][2];
	for(int i = 0; i < MAXRECORDS; i++) { //allocate memory
		root_ns_domains[i][0] = malloc(MAXDOMAINSIZE); 	//domain name memory
		root_ns_domains[i][1] = malloc(16); 			//ip address memory

		local_ns_domains[i][0] = malloc(MAXDOMAINSIZE);	//domain name memory
		local_ns_domains[i][1] = malloc(16);			//ip address memory
	}
	// char **candidate_ns_domains;

	//get root domain servers - assignment of variables needed
	index = 0;
	max = parsedResponse2->numOfAnswerRecords;
	root_ns_domains_max = parsedResponse2->numOfAnswerRecords;

	//get domains
	getDomains(bootstrap_ns_address, parsedResponse2, parsedResponse2->answerRecords, parsedResponse2->numOfAnswerRecords, root_ns_domains);//here....

	//--- line 4 ---
	char *last_ns_address = malloc(16); //max length of an string ip address //we need this in NON DNS format
	sprintf(last_ns_address, "%s", bootstrap_ns_address);

	//--- line 5 ---
	// char **candidate_ns_domains = root_ns_domains;
	int candidate_ns_domains = 0; //0 - root_ns_domains, 1 - local_ns_domains

	//--- line 6 ---
	char *curr_domain = malloc(MAXDOMAINSIZE);
	sprintf(curr_domain, "%s", domain);

	// //initialize timer
	// long ms; 	// Milliseconds		
    // time_t s;  	// Seconds
    // struct timespec spec;

	//--- line 7 ---
	while(index < max) {
		// //begin timer
		// clock_gettime(CLOCK_MONOTONIC, &spec);
		// s = spec.tv_sec;
		// ms = round(spec.tv_nsec / 1.0e6);
		int resetIndex = 0;
		//giving each domain 3 tries to give us something useful
		for(int i = 0; i < 3; i++) { //change to 3
			//--- line 8 ---
			// char *curr_ns_domain = (candidate_ns_domains == 0)? root_ns_domains[index][0] : local_ns_domains[index][0];
			char *curr_ns_address = (candidate_ns_domains == 0)? root_ns_domains[index][1] : local_ns_domains[index][1];

			//--- line 9 ---
			//README changed
			
			// // TIMEOUT check 1/2
			// clock_gettime(CLOCK_MONOTONIC, &spec);
			// if(spec.tv_sec > s || round(spec.tv_nsec / 1.0e6) - ms > TIMEOUTMS) {
			// 	continue;
			// }

			//--- line 10 --- 
			struct message *reply = malloc(sizeof(Message));
			query(curr_ns_address, 1, curr_domain, idCount++, reply, 0, 0, timedOut);

			if(*timedOut == 1) {
				free(reply);
				*timedOut = 0;
				continue;
			}

			// //TIMOUT check 2/2
			// clock_gettime(CLOCK_MONOTONIC, &spec);
			// if(spec.tv_sec > s || round(spec.tv_nsec / 1.0e6) - ms > TIMEOUTMS) {
			// 	free(reply);
			// 	continue;
			// }

			struct resourceRecord *theRecord = NULL;
			struct resourceRecord **desiredRecord = &theRecord;
			
			//--- line 11 ---
			if(getRecordByType(reply->answerRecords, reply->numOfAnswerRecords, 1, desiredRecord) == 1) {
				printRecord(curr_ns_address, *desiredRecord);

				// //--- line 12 ---
				free(reply);
				fflush(stdout);
				return EXIT_SUCCESS;

			//--- line 13 ---
			} else if(getRecordByType(reply->answerRecords, reply->numOfAnswerRecords, 5, desiredRecord) == 1) {
				//--- line 14 --- 
				sprintf(last_ns_address, "%s", bootstrap_ns_address);

				//--- line 15 ---
				candidate_ns_domains = 0; //we are now referring to the root_ns_domains
				max = root_ns_domains_max;
				// index = 0;
				resetIndex = 1;

				//--- line 16 ---
				sprintf(curr_domain, "%s", (*desiredRecord)->data);

				break; //breaking out because we dont need to retry this query

			//--- line 17 ---
			} else if(reply->numOfAuthorityRecords > 0) {
				//--- line 18 ---
				sprintf(last_ns_address, "%s", curr_ns_address);

				//--- line 19 --- 
				getDomains(last_ns_address,reply, reply->authorityRecords, reply->numOfAuthorityRecords, local_ns_domains);

				candidate_ns_domains = 1; //we are now reffering to the local_ns_domains
				max = reply->numOfAuthorityRecords;
				// index = 0;
				resetIndex = 1;
				break;
			} 
			deleteParsedResponse(reply);
			fflush(stdout);
		}

	// 	//look at next domain, since this current domain didnt give us anything useful
		if(resetIndex == 0) index++;
		else index = 0;
		fflush(stdout);
	}

	//freeing memory
	for(int i = 0; i < MAXRECORDS; i++) { //allocate memory
		free(root_ns_domains[i][0]);
		free(root_ns_domains[i][1]);
		free(local_ns_domains[i][0]);
		free(local_ns_domains[i][1]);
	}
	free(domain);//here below
	deleteParsedResponse(parsedResponse2);
	free(last_ns_address);
	free(curr_domain);

	// //if we have gotten here it means we have exhuasted all name servers
	fflush(stdout);
	return EXIT_FAILURE;
	//--- line 22 ---
}
	/*
	PUT

	-WALL

	BACK

	INTO

	THE 

	MAKE

	FILE
	*/