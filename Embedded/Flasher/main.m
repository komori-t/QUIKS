#import <Foundation/Foundation.h>
#import "Serial.h"
#import "Protocol.h"

NSData *maskData(NSData *data)
{
    const uint8_t header = PACKET_HEADER;
    const uint8_t paddedHeader[] = {PACKET_HEADER, 0x00};
    NSData *headerData = [NSData dataWithBytes:&header length:1];
    NSMutableData *ret = [data mutableCopy];
    NSRange searchRange = NSMakeRange(0, [data length]);
    while (1) {
        NSRange headerRange = [ret rangeOfData:headerData options:0 range:searchRange];
        if (headerRange.location == NSNotFound) {
            break;
        }
        [ret replaceBytesInRange:headerRange withBytes:paddedHeader length:2];
        searchRange.location = headerRange.location + 2;
        searchRange.length = [ret length] - searchRange.location;
    }
    return ret;
}

int main(int argc, const char * argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <bin> <id>\n", argv[0]);
        return 1;
    }
    
    NSArray *availableDevices = [Serial availableDevices];
    if ([availableDevices count] < 1) {
        fprintf(stderr, "No serial device found\n");
        return 1;
    }
    Serial *serial = [[Serial alloc] initWithBSDPath:availableDevices[0]];
    [serial openWithBaud:460800];
    
    NSData *programData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]];
    if (programData == nil) {
        fprintf(stderr, "Cound not open %s\n", argv[1]);
        return 1;
    }
    
    const uint8_t numOfPages = ([programData length] - 1) / 64 + 1;
    if (numOfPages > 124) {
        fprintf(stderr, "Binary contains flash program part\n");
        return 1;
    }
    
    uint8_t deviceID = atoi(argv[2]);
    if (deviceID == 0) {
        for (deviceID = 1; deviceID < 254; ++deviceID) {
            const uint8_t rawPingPacket[] = {PACKET_HEADER, deviceID, Command_Ping};
            NSData *pingPacket = [NSData dataWithBytes:rawPingPacket length:sizeof(rawPingPacket)];
            [serial sendRawData:pingPacket];
            if ([[serial readRawDataOfLength:4] length] == 4) {
                break;
            }
        }
        if (deviceID == 254) {
            fprintf(stderr, "No device found\n");
            return 1;
        }
    }
    
    const uint8_t rawStartPacket[] = {PACKET_HEADER, deviceID, Command_Program, numOfPages};
    NSData *startPacket = [NSData dataWithBytes:rawStartPacket length:sizeof(rawStartPacket)];
    [serial sendRawData:startPacket];
    if ([[serial readRawDataOfLength:1] length] < 1) {
        fprintf(stderr, "Device did not respond to initial packet\n");
        return 1;
    }
    
    const uint8_t rawAck = 1;
    const uint8_t rawNack = 0;
    NSData *ackData = [NSData dataWithBytes:&rawAck length:1];
    NSData *nackData = [NSData dataWithBytes:&rawNack length:1];
    for (int page = 0; page < (numOfPages - 1); ++page) {
        NSData *partialData = [programData subdataWithRange:NSMakeRange(64 * page, 64)];
        NSData *maskedData = maskData(partialData);
        while (1) {
            [serial sendRawData:maskedData];
            NSData *rxData = [serial readRawDataOfLength:[maskedData length]];
            if ([rxData length] != [maskedData length]) {
                fprintf(stderr, "Device did not send verify data on page %d\n", page);
                return 1;
            }
            if ([rxData isEqualToData:maskedData]) {
                [serial sendRawData:ackData];
                NSData *rxData = [serial readRawDataOfLength:1];
                if ([rxData length] < 1) {
                    fprintf(stderr, "Device did not send ack on page %d\n", page);
                    return 1;
                }
                break;
            }
            [serial sendRawData:nackData];
        }
    }
    
    const NSUInteger finalDataLength = [programData length] - 64 * (numOfPages - 1);
    const NSRange finalDataRange = NSMakeRange(64 * (numOfPages - 1), finalDataLength);
    NSMutableData *finalData = [[programData subdataWithRange:finalDataRange] mutableCopy];
    if ([finalData length] < 64) {
        [finalData increaseLengthBy:64 - [finalData length]];
    }
    NSData *finalMaskedData = maskData(finalData);
    while (1) {
        [serial sendRawData:finalMaskedData];
        NSData *rxData = [serial readRawDataOfLength:[finalMaskedData length]];
        if ([rxData length] != [finalMaskedData length]) {
            fprintf(stderr, "Device did not send verify data on page %d\n", (numOfPages - 1));
            return 1;
        }
        if ([rxData isEqualToData:finalMaskedData]) {
            [serial sendRawData:ackData];
            NSData *rxData = [serial readRawDataOfLength:1];
            if ([rxData length] == 0) {
                fprintf(stderr, "Device did not send ack on page %d\n", (numOfPages - 1));
                return 1;
            }
            break;
        }
        [serial sendRawData:nackData];
    }
    
    return 0;
}
