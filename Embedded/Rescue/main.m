#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOBSD.h>
#import <IOKit/serial/IOSerialKeys.h>
#import <sys/ioctl.h>
#import "Serial.h"
#import "Protocol.h"

static NSData *maskData(NSData *data)
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

static NSData *programData;
static uint8_t numOfPages;

static void deviceDidFound(void *shouldFlash, io_iterator_t iterator)
{
    io_object_t serialPort;
    while ((serialPort = IOIteratorNext(iterator))) {
        CFStringRef ret = IORegistryEntryCreateCFProperty(serialPort,
                                                          CFSTR(kIOCalloutDeviceKey),
                                                          kCFAllocatorDefault, 0);
        if (CFStringFind(ret, CFSTR("cu.usbserial"), 0).location != kCFNotFound) {
            if (shouldFlash) {
                Serial *serial = [[Serial alloc] initWithBSDPath:(__bridge NSString *)ret];
                [serial openWithBaud:460800];
                
                const uint8_t rawStartPacket[] = {0x46, 0x93, numOfPages};
                NSData *startPacket = [NSData dataWithBytes:rawStartPacket length:sizeof(rawStartPacket)];
                [serial sendRawData:startPacket];
                if ([[serial readRawDataOfLength:1] length] < 1) {
                    fprintf(stderr, "Device did not respond to initial packet\n");
                    exit(1);
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
                            exit(1);
                        }
                        if ([rxData isEqualToData:maskedData]) {
                            [serial sendRawData:ackData];
                            NSData *rxData = [serial readRawDataOfLength:1];
                            if ([rxData length] < 1) {
                                fprintf(stderr, "Device did not send ack on page %d\n", page);
                                exit(1);
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
                        exit(1);
                    }
                    if ([rxData isEqualToData:finalMaskedData]) {
                        [serial sendRawData:ackData];
                        NSData *rxData = [serial readRawDataOfLength:1];
                        if ([rxData length] == 0) {
                            fprintf(stderr, "Device did not send ack on page %d\n", (numOfPages - 1));
                            exit(1);
                        }
                        break;
                    }
                    [serial sendRawData:nackData];
                }
                CFRunLoopStop(CFRunLoopGetCurrent());
            }
        }
        CFRelease(ret);
        IOObjectRelease(serialPort);
    }
}

int main(int argc, const char * argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <bin>\n", argv[0]);
        return 1;
    }

    programData = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]];
    if (programData == nil) {
        fprintf(stderr, "Cound not open %s\n", argv[1]);
        return 1;
    }

    numOfPages = ([programData length] - 1) / 64 + 1;
    if (numOfPages > 240) {
        fprintf(stderr, "Binary is not fit in flash\n");
        return 1;
    }
    
    IONotificationPortRef notificationPort = IONotificationPortCreate(kIOMasterPortDefault);
    io_iterator_t notification;
    IOServiceAddMatchingNotification(notificationPort, kIOMatchedNotification,
                                     IOServiceMatching(kIOSerialBSDServiceValue),
                                     deviceDidFound, (void *)true, &notification);
    deviceDidFound((void *)false, notification);
    
    CFRunLoopSourceRef source = IONotificationPortGetRunLoopSource(notificationPort);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
    CFRunLoopRun();
    
    return 0;
}