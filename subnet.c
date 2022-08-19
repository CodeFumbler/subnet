//
// Subnet Calculator
// v0.1
//
// Copyright (c) 2017 Jason Lich 
//

// TO COMPILE AND TEST: gcc subnet.c -o subnet && ./subnet 
// TO INSTALL INTO BIN: sudo cp ./subnet /usr/local/bin/subnet && sudo chown root:wheel /usr/local/bin/subnet && sudo chmod 755 /usr/local/bin/subnet
// SYMLINK: cd /usr/local/bin/ && sudo ln -s subnet sn && cd $OLDPWD

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char *__progname;

int return_code = 0;
int only_print_network = 0;
int consolidate_networks = 0;

struct network *first_network;
struct network *current_network;
struct network *previous_network;

// define valid option arguments
const char *help_args[] = { "/?", "-?", "--?", "/h", "-h", "--h", "/help", "-help", "--help" };
const char *only_print_network_args[] = { "/n", "-n", "--n" };
const char *consolidate_networks_args[] = { "/c", "-c", "--c" };

// count option arguments elements
const int help_args_count = sizeof(help_args)/sizeof(help_args[0]);
const int only_print_network_args_count = sizeof(only_print_network_args)/sizeof(only_print_network_args[0]);
const int consolidate_networks_args_count = sizeof(consolidate_networks_args)/sizeof(consolidate_networks_args[0]);

struct network
{
	char     *curr_argument;
	char     *next_argument;
	
	char     network_str[16];
	uint32_t network_int;
	
	char     subnet_str[16];
	uint32_t subnet_int;
	
	char     first_useable_str[16];
	uint32_t first_useable_int;
	
	char     last_useable_str[16];
	uint32_t last_useable_int;
	
	char     broadcast_str[16];
	uint32_t broadcast_int;
	
	char     wildcard_str[16];
	uint32_t wildcard_int;
	
	char     cidr_str[16];
	uint32_t cidr_int;
	
	char     next_network_str[16];
	uint32_t next_network_int;
	
	char     prev_network_str[16];
	uint32_t prev_network_int;
	
};

void print_usage()
{
	printf("usage: %s [-c|-n] (network/bits|network mask) ...\n", __progname);
}

char int_to_ip_str( uint32_t value, char *buffer )
{
	uint8_t octet[4];
	
	for (int i=0; i<4; i++)
	{
		octet[i] = value % 256;
		value -= octet[i];
		value /= 256;
	}
	
	if (value > 0)
	{
		return 0;
	}
	
	if (buffer != NULL)
	{
		snprintf(buffer, 16, "%d.%d.%d.%d", octet[3], octet[2], octet[1], octet[0]);
	}
	
	return 1;
}

uint32_t popcnt32( uint32_t value )
{
	uint32_t result = value;
	result = result - ((result >> 1) & 0x55555555);
	result = (result & 0x33333333) + ((result >> 2) & 0x33333333);
	result = (((result + (result >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
	return result;
}

int get_ip_int_and_subnet_int_from_ip_with_cidr_str(char *curr_argument, uint32_t *ip_buffer, uint32_t *sn_buffer)
{
	if ( !curr_argument )
	{
		return 0;
	}
	
	uint32_t ip_dec = 0;
	uint32_t sn_dec = 0;
	uint8_t  cidr_dec = 0;
	uint8_t  octet_dec = 0;
	
	char *current_char = curr_argument;
	
	char *slash = strstr(curr_argument, "/");
	
	if (!slash)
	{
		return 0;
	}
	
	while(*current_char)
	{
		if (current_char < slash)
		{
			if (*current_char >= 48 && *current_char <= 57) // number
			{
				if ((octet_dec * 10 + (*current_char-48)) > 255)
				{
					return 0;
				}
			
				octet_dec *= 10;
				octet_dec += (*current_char-48);
			}
			else if (*current_char == 46) // dot
			{
				ip_dec *= 256;
				ip_dec += octet_dec;
				octet_dec = 0;
			
				if (!*(current_char+1))
				{
					return 0;
				}
				
				if (*(current_char+1)=='/')
				{
					return 0;
				}
			}
			else
			{
				return 0;
			}
		}
		else if (current_char == slash)
		{
			ip_dec *= 256;
			ip_dec += octet_dec;
			
			if (!*(current_char+1))
			{
				return 0;
			}
		}
		else if (current_char > slash)
		{
			if (*current_char >= 48 && *current_char <= 57) // number
			{
				if ((cidr_dec * 10 + (*current_char-48)) > 32)
				{
					return 0;
				}
				
				cidr_dec *= 10;
				cidr_dec += (*current_char-48);
			}
			else
			{
				return 0;
			}
		}
		
		current_char++;
	}
	
	sn_dec = 0xFFFFFFFF;
	
	for (int i=0; i<cidr_dec; i++)
	{
		sn_dec >>= 1;
	}
	
	sn_dec = ~sn_dec;
	
	if ( ! (sn_dec == 0 || (~sn_dec & ~sn_dec + 1) == 0) )
	{
		return 0;
	}
	
	if (ip_buffer != NULL)
	{
		*ip_buffer = ip_dec;
	}
	
	if (sn_buffer != NULL)
	{
		*sn_buffer = sn_dec;
	}
	
	return 1;
}

int get_ip_int_and_subnet_int_from_ip_and_subnet_str(char *curr_argument, char *next_argument, uint32_t *ip_buffer, uint32_t *sn_buffer)
{
		if ( ! (curr_argument && next_argument) )
	{
		return 0;
	}
	
	uint32_t ip_dec = 0;
	uint32_t sn_dec = 0;
	uint8_t  octet_dec = 0;
	
	char *current_char;
	
	current_char = curr_argument;
	
	while (*current_char)
	{
		if (*current_char >= 48 && *current_char <= 57) // number
		{
			if ((octet_dec * 10 + (*current_char-48)) > 255)
			{
				return 0;
			}
			
			octet_dec *= 10;
			octet_dec += (*current_char-48);
		}
		else if (*current_char == 46) // dot
		{
			ip_dec *= 256;
			ip_dec += octet_dec;
			octet_dec = 0;
			
			if (!*(current_char+1))
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
		
		current_char++;
	}
	
	ip_dec *= 256;
	ip_dec += octet_dec;
	
	octet_dec = 0;
	
	current_char = next_argument;
	
	while (*current_char)
	{
		if (*current_char >= 48 && *current_char <= 57) // number
		{
			if ((octet_dec * 10 + (*current_char-48)) > 255)
			{
				return 0;
			}
			
			octet_dec *= 10;
			octet_dec += (*current_char-48);
		}
		else if (*current_char == 46) // dot
		{
			sn_dec *= 256;
			sn_dec += octet_dec;
			octet_dec = 0;
			
			if (!*(current_char+1))
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
		
		current_char++;
	}
	
	sn_dec *= 256;
	sn_dec += octet_dec;
	
	if ( ! (sn_dec == 0 || (~sn_dec & ~sn_dec + 1) == 0) )
	{
		return 0;
	}
	
	if (ip_buffer != NULL)
	{
		*ip_buffer = ip_dec;
	}
	
	if (sn_buffer != NULL)
	{
		*sn_buffer = sn_dec;
	}
	
	return 1;
}

int is_redundant_network( uint32_t input_ip_int, uint32_t input_sn_int )
{
	struct network *current_current_network = current_network;
	struct network *current_network = first_network;
	
	while (current_network->curr_argument)
	{
		if (current_network == current_current_network)
		{
			return 0;
		}
		
		if (current_network->network_int <= input_ip_int && (current_network->network_int^~current_network->subnet_int) >= (input_ip_int^~input_sn_int))
		{
			return 1;
		}
		
		current_network++;
	}
	
	return 0;
}

int is_duplicate_network( uint32_t input_ip_int, uint32_t input_sn_int )
{
	struct network *current_network = first_network;
	
	while (current_network->curr_argument)
	{
		if (current_network->network_int == input_ip_int && current_network->subnet_int == input_sn_int)
		{
			return 1;
		}
		
		current_network++;
	}
	
	return 0;
}

// sort networks first by mask, then by number, in new list_of_networks
int sort_network_by_subnet_asc_comparator(const void * a, const void * b)
{
	struct network *network_a = (struct network *)a;
	struct network *network_b = (struct network *)b;
	
	if (network_a->subnet_int < network_b->subnet_int)
	{
		return -1;
	}
	if (network_a->subnet_int > network_b->subnet_int)
	{
		return 1;
	}
	if (network_a->network_int < network_b->network_int)
	{
		return -1;
	}
	if (network_a->network_int > network_b->network_int)
	{
		return 1;
	}
	return 0;
}

int is_help_arg( char *curr_argument )
{
	for (int i=0; i<help_args_count; i++)
	{
		if (strcmp(help_args[i], curr_argument) == 0)
		{
			return 1;
		}
	}
	return 0;
}

int is_only_print_network_arg( char *curr_argument )
{
	for (int i=0; i<only_print_network_args_count; i++)
	{
		if (strcmp(only_print_network_args[i], curr_argument) == 0)
		{
			return 1;
		}
	}
	return 0;
}

int is_consolidate_networks_arg( char *curr_argument )
{
	for (int i=0; i<consolidate_networks_args_count; i++)
	{
		if (strcmp(consolidate_networks_args[i], curr_argument) == 0)
		{
			return 1;
		}
	}
	return 0;
}


int main( int argc, char *argv[] )
{
	if ( argc < 2 )
	{
		printf("error: No arguments\n");
		print_usage();
		return 1;
	}
	
	int list_of_networks_count = 0;
	
	struct network list_of_networks[argc];
	
	first_network = list_of_networks;
	
	current_network = first_network;
	
	// begin loop args
	for(int i=1; i < argc; i++)
	{
		char *curr_argument = argv[i];
		char *next_argument = (i+1<argc) ? argv[i+1] : NULL;
		
		if (isblank(*curr_argument))
		{
			printf("error: empty argument at position %d\n", i);
			return_code++;
			continue;
		}
		
		// look for help_args in curr_argument
		if (is_help_arg(curr_argument) > 0)
		{
			print_usage();
			return 0;
		}
		
		// look for only_print_network_args in curr_argument
		if (is_only_print_network_arg(curr_argument) > 0)
		{
			only_print_network++;
			continue;
		}
		
		// look for consolidate_networks_args in curr_argument
		if (is_consolidate_networks_arg(curr_argument) > 0)
		{
			consolidate_networks++;
			continue;
		}		
		
		// init vars for ip ints
		uint32_t input_ip_int, input_sn_int;
		
		// get ip integers from input, create output strings, and add to current_network list
		if (get_ip_int_and_subnet_int_from_ip_with_cidr_str(curr_argument, &input_ip_int, &input_sn_int) || (get_ip_int_and_subnet_int_from_ip_and_subnet_str(curr_argument, next_argument, &input_ip_int, &input_sn_int) && i++))
		{
			if (is_duplicate_network(input_ip_int&input_sn_int,input_sn_int))
			{
				continue;
			}
			
			current_network->curr_argument     = curr_argument;
			current_network->next_argument     = next_argument;
			
			current_network->network_int        = input_ip_int&input_sn_int;
			current_network->subnet_int         = input_sn_int;
			current_network->first_useable_int  = (~input_sn_int==0) ? input_ip_int : (input_ip_int&input_sn_int)+1;
			current_network->last_useable_int   = (~input_sn_int==0) ? input_ip_int : (input_ip_int&input_sn_int)^~input_sn_int-1;
			current_network->broadcast_int      = (input_ip_int&input_sn_int)^~input_sn_int;
			current_network->wildcard_int       = ~input_sn_int;
			current_network->prev_network_int   = ((input_ip_int&input_sn_int)-1)&input_sn_int;
			current_network->next_network_int   = (((input_ip_int&input_sn_int)^~input_sn_int)+1)&input_sn_int;
			current_network->cidr_int           = popcnt32(input_sn_int);
			
			int_to_ip_str(current_network->network_int       , current_network->network_str       );
			int_to_ip_str(current_network->subnet_int        , current_network->subnet_str        );
			int_to_ip_str(current_network->first_useable_int , current_network->first_useable_str );
			int_to_ip_str(current_network->last_useable_int  , current_network->last_useable_str  );
			int_to_ip_str(current_network->broadcast_int     , current_network->broadcast_str     );
			int_to_ip_str(current_network->wildcard_int      , current_network->wildcard_str      );
			int_to_ip_str(current_network->prev_network_int  , current_network->prev_network_str  );
			int_to_ip_str(current_network->next_network_int  , current_network->next_network_str  );
			snprintf     (current_network->cidr_str, 16, "%u", current_network->cidr_int          );
			
			current_network++;
			
			list_of_networks_count++;
			
			continue;
		}
		
		// if nothing continues above then this argument isn't going to cut it
		printf("error: invalid argument at position %d: %s\n", i, curr_argument);
		return_code++;
		
	// end loop args
	}
	
	// reset to beginning of array
	current_network = first_network;
	
	// if error, then print help, and return main to stop going any furter
	if (return_code > 0)
	{
		print_usage();
		return return_code;
	}
	
	// this should never happen, but if it does we will stop
	if (list_of_networks_count <= 0)
	{
		printf("error: No network\n");
		print_usage();
		return 1;
	}

	
	// add a new line to make the output stand out
	printf("\n");
	
	// sort networks first by mask, then by number, in new list_of_networks
	qsort((void*)list_of_networks, list_of_networks_count, sizeof(*list_of_networks), sort_network_by_subnet_asc_comparator);
	
	// start with the first network in the list
	current_network = first_network;
	
	//while (current_network->curr_argument)
	for(int i=0; i < list_of_networks_count; i++)
	{
		if (consolidate_networks && is_redundant_network(current_network->network_int,current_network->subnet_int))
		{
			current_network++;
			continue;
		}
		
		printf("Network:   \t%s/%s\n", current_network->network_str, current_network->cidr_str);
		
		if (!only_print_network)
		{
			printf("First:     \t%s\n",    current_network->first_useable_str);
			printf("Last:      \t%s\n",    current_network->last_useable_str);
			printf("Broadcast: \t%s\n",    current_network->broadcast_str);
			printf("Subnet:    \t%s\n",    current_network->subnet_str);
			printf("Wildcard:  \t%s\n",    current_network->wildcard_str);
			printf("Previous:   \t%s/%s\n", current_network->prev_network_str, current_network->cidr_str);
			printf("Next:   \t%s/%s\n", current_network->next_network_str, current_network->cidr_str);
		}
		
		printf("\n");
		
		current_network++;
	}
	
	
	return return_code;
}
