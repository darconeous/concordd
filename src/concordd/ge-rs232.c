#include "ge-rs232.h"
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>

#if __AVR__
#include <avr/pgmspace.h>
static char int_to_hex_digit(uint8_t x) {
	return pgm_read_byte_near(PSTR(
	"0123456789ABCDEF") + (x & 0xF));
}
#else
static char int_to_hex_digit(uint8_t x) {
	return "0123456789ABCDEF"[x & 0xF];
}
#endif

static char
hex_digit_to_int(char c) {
	switch(c) {
	case '0': return 0; break;
	case '1': return 1; break;
	case '2': return 2; break;
	case '3': return 3; break;
	case '4': return 4; break;
	case '5': return 5; break;
	case '6': return 6; break;
	case '7': return 7; break;
	case '8': return 8; break;
	case '9': return 9; break;
	case 'A':
	case 'a': return 10; break;
	case 'B':
	case 'b': return 11; break;
	case 'C':
	case 'c': return 12; break;
	case 'D':
	case 'd': return 13; break;
	case 'E':
	case 'e': return 14; break;
	case 'F':
	case 'f': return 15; break;
	}
	return 0;
}

ge_rs232_t
ge_rs232_init(ge_rs232_t self) {
	bzero((void*)self,sizeof(*self));
	self->last_response = GE_RS232_ACK;
	return self;
}

ge_rs232_status_t
ge_rs232_process(ge_rs232_t self) {
	// Does nothing for now
	return GE_RS232_STATUS_OK;
}

ge_rs232_status_t
ge_rs232_receive_byte(ge_rs232_t self, uint8_t byte) {
	ge_rs232_status_t ret = GE_RS232_STATUS_OK;

	if(byte == GE_RS232_START_OF_MESSAGE) {
		self->reading_message = true;
		self->current_byte = 0;
		self->message_len = 255;
		self->nibble_buffer = 0;
		self->buffer_sum = 0;
	} else if(byte == GE_RS232_ACK && !self->last_response) {
        syslog(LOG_DEBUG,"<ACK>");
		self->last_response = GE_RS232_ACK;
		if(self->got_response)
			self->got_response(self->response_context,self,true);
	} else if(byte == GE_RS232_NAK && !self->last_response) {
        syslog(LOG_DEBUG,"<NAK>");
		self->last_response = GE_RS232_NAK;
		if(self->got_response)
			self->got_response(self->response_context,self,false);
	} else if(self->reading_message) {
		if(!self->nibble_buffer) {
			self->nibble_buffer = byte;
			goto bail;
		}
		uint8_t value = (hex_digit_to_int(self->nibble_buffer)<<4)+hex_digit_to_int(byte);
		self->nibble_buffer = 0;
		if(self->message_len==255) {
			if(value>GE_RS232_MAX_MESSAGE_SIZE) {
				ret = GE_RS232_STATUS_MESSAGE_TOO_BIG;
				self->reading_message = false;
				goto bail;
			}
			if(value<2) {
				ret = GE_RS232_STATUS_MESSAGE_TOO_SMALL;
				self->reading_message = false;
				goto bail;
			}
			self->message_len = value;
			self->current_byte = 0;
		} else {
			self->buffer[self->current_byte++] = value;
		}
		if(self->current_byte>=self->message_len) {
			self->reading_message = false;
			if(self->buffer_sum == value) {
                static const char ack = GE_RS232_ACK;
				self->send_bytes(self->context,&ack,1,self);
				ret = self->received_message(self->context,self->buffer,self->message_len-1,self);
			} else {
                static const char nak = GE_RS232_NAK;
				fprintf(stderr,"[Bad checksum: calculated:0x%02X != indicated:0x%02X]\n",self->buffer_sum,value);
				self->send_bytes(self->context,&nak,1,self);
				ret = GE_RS232_STATUS_BAD_CHECKSUM;
			}
		} else {
			self->buffer_sum += value;
		}
	} else {
		// Just some junk byte we don't know what to do with.
		ret = GE_RS232_STATUS_JUNK;
	}
bail:
	return ret;
}

ge_rs232_status_t
ge_rs232_ready_to_send(ge_rs232_t self) {
	ge_rs232_status_t ret = GE_RS232_STATUS_WAIT;
	time_t curr_time = time(&curr_time);
	if(self->last_response == GE_RS232_ACK) {
		ret = GE_RS232_STATUS_OK;
	} else if(self->last_response == GE_RS232_NAK) {
		ret = GE_RS232_STATUS_NAK;
	} else if(curr_time-self->last_sent > 1) {
		ret = GE_RS232_STATUS_TIMEOUT;
	}
	return ret;
}

ge_rs232_status_t
ge_rs232_resend_last_message(ge_rs232_t self) {
	return ge_rs232_send_message(self,self->output_buffer,self->output_buffer_len);
}

ge_rs232_status_t
ge_rs232_send_message(ge_rs232_t self, const uint8_t* data, uint8_t len) {
	ge_rs232_status_t ret = GE_RS232_STATUS_OK;
	uint8_t checksum = len+1;
    uint8_t buffer[GE_RS232_MAX_MESSAGE_SIZE+7];
    uint8_t *buffer_ptr = buffer;

	if(len>GE_RS232_MAX_MESSAGE_SIZE) {
		ret = GE_RS232_STATUS_MESSAGE_TOO_BIG;
		goto bail;
	}

    if (len == 0) {
        ret = GE_RS232_STATUS_ERROR;
        goto bail;
    }

    syslog(LOG_DEBUG,
        "[OUTFRAME] %d bytes, Type:%d",
        len, data[0]);

	if (data != self->output_buffer) {
		memcpy(self->output_buffer,data,len);
		self->output_buffer_len = len;
		self->output_attempt_count = 0;
	}

	self->output_attempt_count++;

//    if(self->last_response == 0) {
//        ret = self->send_byte(self->context,0x0D,self);
//        if(ret) goto bail;
//    }

	self->last_response = 0;

    *buffer_ptr++ = GE_RS232_START_OF_MESSAGE;
    // Checksum has the length+1, which is what we want to write out.
    *buffer_ptr++ = int_to_hex_digit(checksum>>4);
    *buffer_ptr++ = int_to_hex_digit(checksum);

	// Write out the data.
	for(;len;len--,data++) {
		checksum += *data;
        *buffer_ptr++ = int_to_hex_digit((*data)>>4);
		*buffer_ptr++ = int_to_hex_digit(*data);
	}

	// Now write out the checksum.
    *buffer_ptr++ = int_to_hex_digit(checksum>>4);
    *buffer_ptr++ = int_to_hex_digit(checksum);

    // Send it out
	ret = self->send_bytes(self->context,buffer,buffer_ptr-buffer,self);
	if(ret) goto bail;

	self->last_sent = time(&self->last_sent);
bail:
	return ret;
}

ge_queue_t
ge_queue_init(ge_queue_t qinterface, ge_rs232_t interface) {
	memset((void*)qinterface,sizeof(*qinterface),0);
	qinterface->interface = interface;
	return qinterface;
}

static void
ge_queue_got_response(void* context,struct ge_rs232_s* instance, bool didAck) {
	ge_queue_t qinterface = context;
	ge_rs232_status_t status = ge_rs232_ready_to_send(qinterface->interface);
	struct ge_message_s *message = &qinterface->queue[qinterface->head];

	if(status==GE_RS232_STATUS_OK || message->attempts>=3) {
		if(NULL!=message->finished)
			message->finished(
				message->context,
				status
			);
		qinterface->head = (qinterface->head+1)&(GE_QUEUE_MAX_MESSAGES-1);
	}

	qinterface->interface->got_response = NULL;
	qinterface->interface->response_context = NULL;

}

bool ge_queue_is_empty(ge_queue_t qinterface) {
    return qinterface->head == qinterface->tail;
}

ge_rs232_status_t
ge_queue_update(ge_queue_t qinterface) {
	ge_rs232_status_t status = 0;
    struct ge_message_s *message;

	if(qinterface->head==qinterface->tail)
		goto bail;	// Empty.

	if(ge_rs232_ready_to_send(qinterface->interface)==GE_RS232_STATUS_WAIT)
		goto bail;	// Busy.

	if(	ge_rs232_ready_to_send(qinterface->interface) == GE_RS232_STATUS_TIMEOUT
		&& qinterface->interface->got_response == &ge_queue_got_response
	) {
		(*qinterface->interface->got_response)(qinterface->interface->response_context,qinterface->interface,0);
	}

	// RELEASE THE KRAKEN!
    message = &qinterface->queue[qinterface->head];
	message->attempts++;

	qinterface->interface->got_response = &ge_queue_got_response;
	qinterface->interface->response_context = qinterface;

	status = ge_rs232_send_message(qinterface->interface, message->msg, message->msg_len);

bail:
	return status;
}

ge_rs232_status_t ge_queue_message(
	ge_queue_t qinterface,
	const uint8_t* data,
	uint8_t len,
	void (*finished)(void* context,ge_rs232_status_t status),
	void* context
) {
	ge_rs232_status_t status = 0;
	struct ge_message_s *message = &qinterface->queue[qinterface->tail];

	// Stuff is added to the tail and removed from the head.
	// Tail is always empty.

	if((qinterface->tail-qinterface->head&(GE_QUEUE_MAX_MESSAGES-1)) == (GE_QUEUE_MAX_MESSAGES-1)) {
		// Queue is full!
		status = GE_RS232_STATUS_QUEUE_FULL;
	}

	message->context = context;
	message->finished = finished;
	memcpy(message->msg,data,len);
	message->msg_len = len;
	message->attempts = 0;

	qinterface->tail = (qinterface->tail+1)&(GE_QUEUE_MAX_MESSAGES-1);

	ge_queue_update(qinterface);

bail:
	return status;
}

const char *ge_rs232_text_token_lookup[256] = {
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	[0x0c]="#",
	":",
	"/",
	"?",
	".",
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	" ",
	"'",
	"-",
	"_",
	"*",
	"AC POWER ",
	"ACCESS ",
	"ACCOUNT ",
	"ALARM ",
	"ALL ",
	"ARM ",
	"ARMING ",
	"AREA ",
	"ATTIC ",
	"AUTO ",
	"AUXILIARY ",
	"AWAY ",
	"BACK ",
	"BATTERY ",
	"BEDROOM ",
	"BEEPS ",
	"BOTTOM ",
	"BREEZEWAY ",
	"BASEMENT ",
	"BATHROOM ",
	"BUS ",
	"BYPASS ",
	"BYPASSED ",
	"CABINET ",
	"CANCELED ",
	"CARPET ",
	"CHIME ",
	"CLOSET ",
	"CLOSING ",
	"CODE ",
	"CONTROL ",
	"CPU ",
	"DEGREES ",
	"DEN ",
	"DESK ",
	"DELAY ",
	"DELETE ",
	"DINING ",
	"DIRECT ",
	"DOOR ",
	"DOWN ",
	"DOWNLOAD ",
	"DOWNSTAIRS ",
	"DRAWER ",
	"DISPLAY ",
	"DURESS ",
	"EAST ",
	"ENERGY SAVER ",
	"ENTER ",
	"ENTRY ",
	"ERROR ",
	"EXIT ",
	"FAIL ",
	"FAILURE ",
	"FAMILY ",
	"FEATURES ",
	"FIRE ",
	"FIRST ",
	"FLOOR ",
	"FORCE ",
	"FORMAT ",
	"FREEZE ",
	"FRONT ",
	"FURNACE ",
	"GARAGE ",
	"GALLERY ",
	"GOODBYE ",
	"GROUP ",
	"HALL ",
	"HEAT ",
	"HELLO ",
	"HELP ",
	"HIGH ",
	"HOURLY ",
	"HOUSE ",
	"IMMEDIATE ",
	"IN SERVICE ",
	"INTERIOR ",
	"INTRUSION ",
	"INVALID ",
	"IS ",
	[0x81] = "KEY ",
	[0x82] = "KITCHEN ",
	[0x83] = "LAUNDRY ",
	[0x84] = "LEARN ",
	[0x85] = "LEFT ",
	[0x86] = "LIBRARY ",
	[0x87] = "LEVEL ",
	[0x88] = "LIGHT ",
	[0x89] = "LIGHTS ",
	[0x8A] = "LIVING ",
	[0x8B] = "LOW ",
	[0x8C] = "MAIN ",
	[0x8D] = "MASTER ",
	[0x8E] = "MEDICAL",
	[0x8F] = "MEMORY ",
	[0x90] = "MIN ",
	"MODE ",
	"MOTION ",
	"NIGHT ",
	"NORTH ",
	"NOT ",
	"NUMBER ",
	"OFF ",
	"OFFICE ",
	"OK ",
	"ON ",
	"OPEN ",
	"OPENING ",
	"PANIC ",
	"PARTITION ",
	"PATIO ",
	"PHONE ",
	"POLICE ",
	"POOL ",
	"PORCH ",
	"PRESS ",
	"QUIET ",
	"QUICK ",
	"RECEIVER ",
	"REAR ",
	"REPORT ",
	"REMOTE ",
	"RESTORE ",
	"RIGHT ",
	"ROOM ",
	"SCHEDULE ",
	"SCRIPT ",
	"SEC ",
	"SECOND ",
	"SET ",
	"SENSOR ",
	"SHOCK ",
	"SIDE ",
	"SIREN ",
	"SLIDING ",
	"SMOKE ",
	"Sn ", // ???
	"SOUND ",
	"SOUTH ",
	"SPECIAL ",
	"STAIRS ",
	"START ",
	"STATUS ",
	"STAY ",
	"STOP ",
	"SUPERVISORY ",
	"SYSTEM ",
	"TAMPER ",
	"TEMPERATURE ",
	"TEMPORARY ",
	"TEST ",
	"TIME ",
	"TIMEOUT ",
	"TOUCHPAD ",
	"TRIP ",
	"TROUBLE ",
	"UNBYPASS ",
	"UNIT ",
	"UP ",
	"VERIFY ",
	"VIOLATION ",
	"WARNING ",
	"WEST ",
	"WINDOW ",
	"MENU ",
	"RETURN ",
	"POUND ",
	"HOME ",
	[0xF9]="\n",	// Carriage Return
	[0xFA]=" ",		// "pseudo space", whatever the hell that means.
	[0xFB]="\n",	// Another Carriage Return?
	[0xFD]="\b",	// Backspace...?
	[0xFE]="",		// Indicates that the next token should blink.
};

#define GE_TEXT_BLINK_TOKEN		0xFE

const char*
ge_text_to_ascii_one_line(const uint8_t * bytes, uint8_t len) {
	static char ret[1024];
	bool blink_next_token = false;
	ret[0] = 0;
	// TODO: Optimize!
	while(len--) {
		uint8_t code = *bytes++;
		if (code == GE_TEXT_BLINK_TOKEN) {
			if (blink_next_token == false) {
				strlcat(ret,"<",sizeof(ret));
			}
			blink_next_token = true;
			if (len--) {
				code = *bytes++;
			} else {
				continue;
			}
		} else {
			if (blink_next_token == true) {
				if (isspace(ret[strlen(ret)-1])) {
					ret[strlen(ret)-1] = 0;
					strlcat(ret,"> ",sizeof(ret));
				} else {
					strlcat(ret,">",sizeof(ret));
				}
			}
			blink_next_token = false;
		}
		const char* str = ge_rs232_text_token_lookup[code];
		if (str) {
			if (str[0]=='\n') {
				if (len) {
					strlcat(ret,isspace(ret[strlen(ret)-1])?"| ":" | ",sizeof(ret));
				}
			} else if (str[0]=='\b') {
				// Backspace
				if (ret[0]) {
					ret[strlen(ret)-1] = 0;
				}
			} else {
				strlcat(ret,str,sizeof(ret));
			}
		} else {
			strlcat(ret,"?",sizeof(ret));
		}
	}
	if (blink_next_token == true) {
		if (isspace(ret[strlen(ret)-1])) {
			ret[strlen(ret)-1] = 0;
			strlcat(ret,"> ",sizeof(ret));
		} else {
			strlcat(ret,">",sizeof(ret));
		}
	}
	// Remove trailing whitespace.
	for(len=strlen(ret);len && isspace(ret[len-1]);len--) {
		ret[len-1] = 0;
	}
	return ret;
}

const char*
ge_text_to_ascii(const uint8_t * bytes, uint8_t len) {
	static char ret[1024];
	ret[0] = 0;
	bool blink_next_token = false;
	// TODO: Optimize!
	while(len--) {
		uint8_t code = *bytes++;
		if (code == GE_TEXT_BLINK_TOKEN) {
			if (blink_next_token == false) {
				strlcat(ret,"<",sizeof(ret));
			}
			blink_next_token = true;
			if (len--) {
				code = *bytes++;
			} else {
				continue;
			}
		} else {
			if (blink_next_token == true) {
				if (isspace(ret[strlen(ret)-1])) {
					ret[strlen(ret)-1] = 0;
					strlcat(ret,"> ",sizeof(ret));
				} else {
					strlcat(ret,">",sizeof(ret));
				}
			}
			blink_next_token = false;
		}
		const char* str = ge_rs232_text_token_lookup[code];
		if(str) {
			if(str[0]=='\b') {
				// Backspace
				if(ret[0])
					ret[strlen(ret)-1] = 0;
			} else {
				strlcat(ret,str,sizeof(ret));
			}
		} else {
			strlcat(ret,"?",sizeof(ret));
		}
	}
	if (blink_next_token == true) {
		if (isspace(ret[strlen(ret)-1])) {
			ret[strlen(ret)-1] = 0;
			strlcat(ret,"> ",sizeof(ret));
		} else {
			strlcat(ret,">",sizeof(ret));
		}
	}
	// Remove trailing whitespace.
	for(len=strlen(ret);len && isspace(ret[len-1]);len--) {
		ret[len-1] = 0;
	}
	return ret;
}

const char* ge_user_to_cstr(char* dest, int user) {
    static char static_string[48];

    if (dest == NULL) {
        dest = static_string;
    }

    if (user >= 230 && user <= 237) {
        snprintf(dest, sizeof(static_string), "P%d-MASTER-CODE", user - 230);
    } else if (user >= 238 && user <= 245) {
        snprintf(dest, sizeof(static_string), "P%d-DURESS-CODE", user - 238);
    } else if (user == 246) {
        strlcpy(dest, "SYSTEM-CODE", sizeof(static_string));
    } else if (user == 247) {
        strlcpy(dest, "INSTALLER", sizeof(static_string));
    } else if (user == 248) {
        strlcpy(dest, "DEALER", sizeof(static_string));
    } else if (user == 249) {
        strlcpy(dest, "AVM-CODE", sizeof(static_string));
    } else if (user == 250) {
        strlcpy(dest, "QUICK-ARM", sizeof(static_string));
    } else if (user == 251) {
        strlcpy(dest, "KEY-SWITCH", sizeof(static_string));
    } else if (user == 252) {
        strlcpy(dest, "SYSTEM", sizeof(static_string));
    } else if (user == 255) {
        strlcpy(dest, "AUTOMATION", sizeof(static_string));
    } else if (user == 65535) {
        strlcpy(dest, "SYSTEM/KEY-SWITCH", sizeof(static_string));
    } else {
        snprintf(dest, sizeof(static_string), "USER-%d", user);
    }

    return dest;
}

const char*
ge_specific_alarm_to_cstr(char* dest, int code)
{
    static char static_string[48];

    if (dest == NULL) {
        dest = static_string;
    }

    switch (code) {
    case GE_RS232_ALARM_SPECIFIC_UNSPECIFIED:
        snprintf(dest, sizeof(static_string), "UNSPECIFIED");
        break;

    case GE_RS232_ALARM_SPECIFIC_FIRE:
        snprintf(dest, sizeof(static_string), "FIRE");
        break;

    case GE_RS232_ALARM_SPECIFIC_FIRE_PANIC:
        snprintf(dest, sizeof(static_string), "FIRE PANIC");
        break;

    case GE_RS232_ALARM_SPECIFIC_POLICE:
        snprintf(dest, sizeof(static_string), "POLICE");
        break;

    case GE_RS232_ALARM_SPECIFIC_POLICE_PANIC:
        snprintf(dest, sizeof(static_string), "POLICE PANIC");
        break;

    case GE_RS232_ALARM_SPECIFIC_AUXILIARY:
        snprintf(dest, sizeof(static_string), "AUXILIARY");
        break;

    case GE_RS232_ALARM_SPECIFIC_AUXILIARY_PANIC:
        snprintf(dest, sizeof(static_string), "AUXILIARY PANIC");
        break;

    case GE_RS232_ALARM_SPECIFIC_NO_ACTIVITY:
        snprintf(dest, sizeof(static_string), "NO ACTIVITY");
        break;

    case GE_RS232_ALARM_SPECIFIC_LOW_TEMPERATURE:
        snprintf(dest, sizeof(static_string), "LOW TEMPERATURE");
        break;

    case GE_RS232_ALARM_SPECIFIC_KEYSTROKE_VIOLATION:
        snprintf(dest, sizeof(static_string), "KEYSTROKE VIOLATION");
        break;

    case GE_RS232_ALARM_SPECIFIC_DURESS:
        snprintf(dest, sizeof(static_string), "DURESS");
        break;

    case GE_RS232_ALARM_SPECIFIC_EXIT_FAULT:
        snprintf(dest, sizeof(static_string), "EXIT FAULT");
        break;

    case GE_RS232_ALARM_SPECIFIC_CARBON_MONOXIDE:
        snprintf(dest, sizeof(static_string), "CARBON MONOXIDE");
        break;

    case GE_RS232_ALARM_SPECIFIC_LATCHKEY:
        snprintf(dest, sizeof(static_string), "LATCHKEY");
        break;

    case GE_RS232_ALARM_SPECIFIC_SIREN_TAMPER:
        snprintf(dest, sizeof(static_string), "SIREN TAMPER");
        break;

    case GE_RS232_ALARM_SPECIFIC_ENTRY_EXIT:
        snprintf(dest, sizeof(static_string), "ENTRY/EXIT");
        break;

    case GE_RS232_ALARM_SPECIFIC_PERIMETER:
        snprintf(dest, sizeof(static_string), "PERIMETER");
        break;

    case GE_RS232_ALARM_SPECIFIC_INTERIOR:
        snprintf(dest, sizeof(static_string), "INTERIOR");
        break;

    case GE_RS232_ALARM_SPECIFIC_NEAR:
        snprintf(dest, sizeof(static_string), "NEAR");
        break;

    case GE_RS232_ALARM_SPECIFIC_WATER:
        snprintf(dest, sizeof(static_string), "WATER");
        break;

    default:
        snprintf(dest, sizeof(static_string), "%d", code);
        break;
    }

    return dest;
}

const char*
ge_specific_trouble_to_cstr(char* dest, int code)
{
    static char static_string[48];

    if (dest == NULL) {
        dest = static_string;
    }

    switch (code) {
    case GE_RS232_TROUBLE_SPECIFIC_UNSPECIFIED:
        snprintf(dest, sizeof(static_string), "UNSPECIFIED");
        break;

    case GE_RS232_TROUBLE_SPECIFIC_HARDWARE:
        snprintf(dest, sizeof(static_string), "HARDWARE");
        break;

    case GE_RS232_TROUBLE_SPECIFIC_SUPERVISORY:
        snprintf(dest, sizeof(static_string), "SUPERVISORY");
        break;

    case GE_RS232_TROUBLE_SPECIFIC_LOW_BATTERY:
        snprintf(dest, sizeof(static_string), "LOW BATTERY");
        break;

    case GE_RS232_TROUBLE_SPECIFIC_TAMPER:
        snprintf(dest, sizeof(static_string), "TAMPER");
        break;

    case GE_RS232_TROUBLE_SPECIFIC_PARTIAL_OBSCURITY:
        snprintf(dest, sizeof(static_string), "PARTIAL OBSCURITY");
        break;

    default:
        snprintf(dest, sizeof(static_string), "%d", code);
        break;
    }

    return dest;
}

const char*
ge_specific_partition_to_cstr(char* dest, int code)
{
    static char static_string[48];

    if (dest == NULL) {
        dest = static_string;
    }

    switch (code) {
    case GE_RS232_PARTITION_SPECIFIC_SCHEDULE_ON:
        snprintf(dest, sizeof(static_string), "SCHEDULE-ON");
        break;

    case GE_RS232_PARTITION_SPECIFIC_SCHEDULE_OFF:
        snprintf(dest, sizeof(static_string), "SCHEDULE-OFF");
        break;

    case GE_RS232_PARTITION_SPECIFIC_LATCHKEY_ON:
        snprintf(dest, sizeof(static_string), "LATCHKEY-ON");
        break;

    case GE_RS232_PARTITION_SPECIFIC_LATCHKEY_OFF:
        snprintf(dest, sizeof(static_string), "LATCHKEY-OFF");
        break;

    case GE_RS232_PARTITION_SPECIFIC_SMOKE_DET_RESET:
        snprintf(dest, sizeof(static_string), "SMOKE DETECTOR RESET");
        break;

    case GE_RS232_PARTITION_SPECIFIC_MANUAL_FORCE_ARM:
        snprintf(dest, sizeof(static_string), "MANUAL FORCE ARM");
        break;

    case GE_RS232_PARTITION_SPECIFIC_AUTO_FORCE_ARM:
        snprintf(dest, sizeof(static_string), "AUTO FORCE ARM");
        break;

    case GE_RS232_PARTITION_SPECIFIC_ARM_PROTEST_BEGIN:
        snprintf(dest, sizeof(static_string), "ARM PROTEST BEGIN");
        break;

    case GE_RS232_PARTITION_SPECIFIC_ARM_PROTEST_END:
        snprintf(dest, sizeof(static_string), "ARM PROTEST END");
        break;

    default:
        snprintf(dest, sizeof(static_string), "%d", code);
        break;
    }

    return dest;
}

const char*
ge_specific_system_trouble_to_cstr(char* dest, int code)
{
    static char static_string[48];

    if (dest == NULL) {
        dest = static_string;
    }

    switch (code) {
    case GE_RS232_SYSTEM_TROUBLE_SPECIFIC_MAIN_AC_FAIL:
        snprintf(dest, sizeof(static_string), "MAIN AC FAIL");
        break;
    case GE_RS232_SYSTEM_TROUBLE_SPECIFIC_MAIN_BATTERY_LOW:
        snprintf(dest, sizeof(static_string), "MAIN BATTERY LOW");
        break;
    case GE_RS232_SYSTEM_TROUBLE_SPECIFIC_BUS_SHUTDOWN:
        snprintf(dest, sizeof(static_string), "BUS SHUTDOWN");
        break;
    case GE_RS232_SYSTEM_TROUBLE_SPECIFIC_BUS_LOW_POWER_MODE:
        snprintf(dest, sizeof(static_string), "BUS LOW POWER MODE");
        break;
    case GE_RS232_SYSTEM_TROUBLE_SPECIFIC_PHONE_LINE_1_FAIL:
        snprintf(dest, sizeof(static_string), "PHONE LINE 1 FAIL");
        break;
    case GE_RS232_SYSTEM_TROUBLE_SPECIFIC_WATCHDOG_RESET:
        snprintf(dest, sizeof(static_string), "WATCHDOG RESET");
        break;
    case GE_RS232_SYSTEM_TROUBLE_SPECIFIC_BUS_DEVICE_FAIL:
        snprintf(dest, sizeof(static_string), "BUS DEVICE FAIL");
        break;
    case GE_RS232_SYSTEM_TROUBLE_SPECIFIC_BUS_RECEIVER_FAIL:
        snprintf(dest, sizeof(static_string), "BUS RECEIVER FAIL");
        break;

    default:
        snprintf(dest, sizeof(static_string), "%d", code);
        break;
    }

    return dest;
}
