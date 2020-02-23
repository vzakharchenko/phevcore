#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "phev_core.h"
#include "msg_core.h"
#include "msg_utils.h"
#include "logger.h"

const static char *APP_TAG = "PHEV_CORE";

phevMessage_t * phev_core_createMessage(const uint8_t command, const uint8_t type, const uint8_t reg, const uint8_t * data, const size_t length)
{
    LOG_V(APP_TAG,"START - createMessage");
    LOG_D(APP_TAG,"Data %d Length %d",data[0],length);
    phevMessage_t * message = malloc(sizeof(phevMessage_t));

    message->command = command;
    message->type = type;
    message->reg = reg;
    message->length = length;
    message->data = malloc(message->length);
    memcpy(message->data, data, length);
    
    LOG_D(APP_TAG,"Message Data %d",message->data[0]);
    
    LOG_V(APP_TAG,"END - createMessage");
    
    return message;
}

void phev_core_destroyMessage(phevMessage_t * message) 
{
    LOG_V(APP_TAG,"START - destroyMessage");
    
    if(message == NULL) return;

    if(message->data != NULL)
    {
        free(message->data);
    }
    free(message);
    LOG_V(APP_TAG,"END - destroyMessage");
    
}
int phev_core_validate_buffer(const uint8_t * msg, const size_t len)
{
    LOG_V(APP_TAG,"START - validateBuffer");
    
    uint8_t length = msg[1];
    uint8_t cmd = msg[0];

    for(int i = 0;i < sizeof(allowedCommands); i++)
    {
        if(cmd == allowedCommands[i])
        { 
            //HACK to handle CD / CC
            if(cmd == 0xcd) 
            {
                return 1;
            }
            if(length + 2 > len)
            {
                LOG_E(APP_TAG,"Valid command but length incorrect : command %02x length %dx expected %d",msg[0],length, len);
                return 0;  // length goes past end of message
            }
            return 1; //valid message
        }
    }
    LOG_E(APP_TAG,"Invalid command %02x length %02x",msg[0],msg[1]);
            
    LOG_V(APP_TAG,"END - validateBuffer");
    
    return 0;  // invalid command
}
uint8_t phev_core_getXOR(const uint8_t * data)
{
    if(data[2] < 2) 
    {
        return 0;
    }
    return (data[2] | (!(data[0] & 1))) & 0xfe;
    
} 
uint8_t * phev_core_unscramble(const uint8_t * data, const size_t len)
{
    LOG_V(APP_TAG,"START - unscramble");
    uint8_t * decodedData = malloc(len);
    
    if(data[2] < 2) 
    {
        LOG_D(APP_TAG,"unscramble not required");
    
        memcpy(decodedData,data,len);
        return decodedData;
    }
    const uint8_t xor = phev_core_getXOR(data);
    LOG_D(APP_TAG,"unscrambling");
    for(int i=0;i<len;i++)
    {
   
        decodedData[i] = data[i] ^ xor; 
    }
    LOG_V(APP_TAG,"END - unscramble");
    
    return decodedData;
}
int phev_core_decodeMessage(const uint8_t *data, const size_t len, phevMessage_t *msg)
{
    LOG_V(APP_TAG,"START - decodeMessage");
    
    //const uint8_t * decodedData = data;

    //decodedData = phev_core_unscramble(data, len);
    
    //LOG_BUFFER_HEXDUMP(APP_TAG,data,len,LOG_INFO);
    //LOG_BUFFER_HEXDUMP(APP_TAG,decodedData,len,LOG_INFO);

    uint8_t xor = phev_core_getXOR(data);
    
    message_t * message = msg_utils_createMsg(data, len);
    message_t * decoded = phev_core_XORInboundMessage(message,xor);
    
    const uint8_t * decodedData = decoded->data;

    if(phev_core_validate_buffer(decodedData, len) != 0)
    {

        msg->command = decodedData[0];
        msg->xor = phev_core_getXOR(data);
        //printf("Message XOR %02X %02X\n",msg->xor,data[2]);
        msg->length = msg->command != 0xcd ? decodedData[1]- 3 : 1;    
        msg->type = decodedData[2];
        msg->reg = decodedData[3];
        msg->data = malloc(msg->length);
        if(msg->length > 0) 
        {
            memcpy(msg->data, decodedData + 4, msg->length);
        } else {
            msg->data = NULL;
        }
        msg->checksum = decodedData[4 + msg->length];

        LOG_I(APP_TAG,"Command %02x Length %d type %d reg %02x",msg->command,msg->length ,msg->type,msg->reg);
        if(msg->data != NULL && msg->length > 0)
        {
            LOG_BUFFER_HEXDUMP(APP_TAG,msg->data,msg->length,LOG_INFO);
        }
        
        LOG_V(APP_TAG,"END - decodeMessage");
        
        return 1;
    } else {
        LOG_E(APP_TAG,"INVALID MESSAGE - original");
        LOG_BUFFER_HEXDUMP(APP_TAG,data,len,LOG_ERROR);
        LOG_E(APP_TAG,"INVALID MESSAGE - decoded");
        LOG_BUFFER_HEXDUMP(APP_TAG,decodedData,len,LOG_ERROR);
        
        LOG_V(APP_TAG,"END - decodeMessage");
        return 0;
    }
}
message_t * phev_core_extractMessage(const uint8_t *data, const size_t len)
{
    LOG_V(APP_TAG,"START - extractMessage");

    //const uint8_t * unscrambled = phev_core_unscramble(data,len);

    uint8_t xor = phev_core_getXOR(data);
    
    message_t * message = msg_utils_createMsg(data, len);
    message_t * decoded = phev_core_XORInboundMessage(message,xor);
    
    const uint8_t * unscrambled = decoded->data;
    if(phev_core_validate_buffer(unscrambled, len) != 0)
    {
        
        message_t * message = msg_utils_createMsg(data,unscrambled[1] + 2);

        LOG_V(APP_TAG,"END - extractMessage");
    
        return message;
    } else {
        LOG_E(APP_TAG,"Invalid Message");
        LOG_BUFFER_HEXDUMP(APP_TAG,data,len,LOG_ERROR);
        LOG_V(APP_TAG,"END - extractMessage");
        return NULL;    
    }
}

int phev_core_encodeMessage(phevMessage_t *message,uint8_t ** data)
{
    LOG_V(APP_TAG,"START - encodeMessage");

    LOG_V(APP_TAG,"encode XOR %02x",message->xor);
    
    uint8_t * d = malloc(message->length + 5);
    
    d[0] = message->command;
    d[1] = (message->length + 3);
    d[2] = message->type;
    d[3] = message->reg;
    
    if(message->length > 0 && message->data != NULL) 
    {
        
        memcpy(d + 4, message->data, message->length );
    }
    
    d[message->length + 4] = phev_core_checksum(d);

    *data = d;

    LOG_D(APP_TAG,"Created message");
    LOG_BUFFER_HEXDUMP(APP_TAG,d,d[1] +2,LOG_DEBUG);
    LOG_V(APP_TAG,"END - encodeMessage");
        
    return d[1] + 2;
}

phevMessage_t *phev_core_message(const uint8_t command, const uint8_t type, const uint8_t reg, const uint8_t *data, const size_t length)
{
    return phev_core_createMessage(command, type, reg, data, length);
}
phevMessage_t *phev_core_responseMessage(const uint8_t command, const uint8_t reg, const uint8_t *data, const size_t length)
{
    return phev_core_message(command, RESPONSE_TYPE, reg, data, length);
}
phevMessage_t *phev_core_requestMessage(const uint8_t command, const uint8_t reg, const uint8_t *data, const size_t length)
{
    return phev_core_message(command, REQUEST_TYPE, reg, data, length);
}
phevMessage_t *phev_core_commandMessage(const uint8_t reg, const uint8_t *data, const size_t length)
{
    if(phev_core_my18)
    {
        return phev_core_requestMessage(SEND_CMD, reg, data, length);    
    }
    return phev_core_requestMessage(SEND_CMD, reg, data, length);
}
phevMessage_t *phev_core_simpleRequestCommandMessage(const uint8_t reg, const uint8_t value)
{
    const uint8_t data = value;
    if(phev_core_my18)
    {
        phev_core_requestMessage(SEND_CMD, reg, &data, 1);
    }
    return phev_core_requestMessage(SEND_CMD, reg, &data, 1);
}
phevMessage_t *phev_core_simpleResponseCommandMessage(const uint8_t reg, const uint8_t value)
{
    const uint8_t data = value;
    if(phev_core_my18)
    {
        phev_core_responseMessage(SEND_CMD, reg, &data, 1);
    }
    return phev_core_responseMessage(SEND_CMD, reg, &data, 1);
}
phevMessage_t *phev_core_ackMessage(const uint8_t command, const uint8_t reg)
{
    const uint8_t data = 0;
    return phev_core_responseMessage(command, reg, &data, 1);
}
phevMessage_t *phev_core_startMessage(const uint8_t *mac)
{
    uint8_t * data = malloc(7);
    data[6] = 0;
    memcpy(data,mac, 6);
    if(phev_core_my18)
    {
        printf("MY18 PING\n");
        phev_core_requestMessage(START_SEND_MY18, 0x01, data, 7);
    }
    printf("PING\n");
    return phev_core_requestMessage(START_SEND_MY18, 0x01, data, 7);
}
message_t *phev_core_startMessageEncoded(const uint8_t *mac)
{
    phevMessage_t * start = phev_core_startMessage(mac);
    phevMessage_t * startaa = phev_core_simpleRequestCommandMessage(0xaa,0);
    message_t * message = msg_utils_concatMessages(
                                    phev_core_convertToMessage(start),
                                    phev_core_convertToMessage(startaa)
                                );
    phev_core_destroyMessage(start);
    phev_core_destroyMessage(startaa);
    return message;
}
phevMessage_t *phev_core_pingMessage(const uint8_t number)
{
    const uint8_t data = 0;
    if(phev_core_my18)
    {
        return phev_core_requestMessage(PING_SEND_CMD_MY18, number, &data, 1);    
    }
    return phev_core_requestMessage(PING_SEND_CMD_MY18, number, &data, 1);
}
phevMessage_t *phev_core_responseHandler(phevMessage_t * message)
{
    uint8_t command = ((message->command & 0xf) << 4) | ((message->command & 0xf0) >> 4);
    return phev_core_ackMessage(command, message->reg); 
}

uint8_t phev_core_checksum(const uint8_t * data) 
{
    uint8_t b = 0;
    int j = data[1] + 2;
    for (int i = 0;; i++)
    {
      if (i >= j - 1) {
        return b;
      }
      b = (uint8_t)(data[i] + b);
    }
}
message_t * phev_core_convertToMessage(phevMessage_t *message)
{
    LOG_V(APP_TAG,"START - convertToMessage");
        
    uint8_t * data = NULL;   
    
    size_t length = phev_core_encodeMessage(message, &data);

    message_t * out = msg_utils_createMsg(data,length);

    free(data);

    LOG_V(APP_TAG,"END - convertToMessage");
        
    return out;
}

phevMessage_t * phev_core_copyMessage(phevMessage_t * message)
{   
    phevMessage_t * out = malloc(sizeof(phevMessage_t));
    out->data = malloc(message->length);
    out->command = message->command;
    out->reg = message->reg;
    out->type = message->type;
    out->length = message->length;
    out->xor = message->xor;
    memcpy(out->data,message->data,out->length);

    return out;
}
message_t * phev_core_XOROutboundMessage(message_t * message,uint8_t xor)
{
    if(xor < 2) return message;

    uint8_t type = message->data[2];
    xor ^= type;
    
    for(int i=0;i<message->length;i++)
    {
        message->data[i] = (uint8_t) message->data[i] ^ xor;
    }
    LOG_I(APP_TAG,"XOR message");
    return message;
}
message_t * phev_core_XORInboundMessage(message_t * message,uint8_t xor)
{
    if(xor < 2) return message;

    
    for(int i=0;i<message->length;i++)
    {
        message->data[i] = (uint8_t) message->data[i] ^ xor;
    }
    //message->data[0] |= message->data[2]; 
    LOG_I(APP_TAG,"XOR message");
    return message;
}
