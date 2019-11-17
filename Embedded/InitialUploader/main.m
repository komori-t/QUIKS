#import <Foundation/Foundation.h>
#import "Serial.h"

int main(int argc, const char * argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <bin>\n", argv[0]);
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
    
    NSData *startPacket = [NSData dataWithBytes:&numOfPages length:1];
    [serial sendRawData:startPacket];
    if ([[serial readRawDataOfLength:1] length] < 1) {
        fprintf(stderr, "Device did not respond to initial packet\n");
        return 1;
    }
    
    const uint8_t rawAck = 1;
    const uint8_t rawNack = 0;
    NSData *ackData = [NSData dataWithBytes:&rawAck length:1];
    NSData *nackData = [NSData dataWithBytes:&rawNack length:1];
    
    for (int page = 0; page < numOfPages - 1; ++page) {
        NSData *partialData = [programData subdataWithRange:NSMakeRange(64 * page, 64)];
        while (1) {
            [serial sendRawData:partialData];
            NSData *rxData = [serial readRawDataOfLength:[partialData length]];
            if ([rxData length] != [partialData length]) {
                fprintf(stderr, "Device did not respond to page %d\n", page);
                return 1;
            }
            if ([rxData isEqualToData:partialData]) {
                [serial sendRawData:ackData];
                NSData *rxData = [serial readRawDataOfLength:1];
                if ([rxData length] < 1) {
                    fprintf(stderr, "Device did not respond to page %d\n", page);
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
    while (1) {
        [serial sendRawData:finalData];
        NSData *rxData = [serial readRawDataOfLength:[finalData length]];
        if ([rxData length] != [finalData length]) {
            fprintf(stderr, "Device did not respond to page %d\n", (numOfPages - 1));
            return 1;
        }
        if ([rxData isEqualToData:finalData]) {
            [serial sendRawData:ackData];
            NSData *rxData = [serial readRawDataOfLength:1];
            if ([rxData length] < 1) {
                fprintf(stderr, "Device did not respond to page %d\n", (numOfPages - 1));
                return 1;
            }
            break;
        }
        [serial sendRawData:nackData];
    }
    
    return 0;
}
