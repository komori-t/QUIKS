#import "Serial.h"
#import <string.h>
#import <unistd.h>
#import <fcntl.h>
#import <sys/ioctl.h>
#import <errno.h>
#import <paths.h>
#import <termios.h>
#import <sysexits.h>
#import <sys/param.h>
#import <sys/select.h>
#import <sys/time.h>
#import <time.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/serial/IOSerialKeys.h>
#import <IOKit/serial/ioss.h>
#import <IOKit/IOBSD.h>
#import "Protocol.h"

NSString * const SerialException = @"SerialException";

@implementation Serial
{
    NSFileHandle *handle;
    struct termios gOriginalTTYAttrs;
}

+ (NSArray *)availableDevices
{
    io_object_t serialPort;
    io_iterator_t serialPortIterator;
    
    // ask for all the serial ports
    IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(kIOSerialBSDServiceValue), &serialPortIterator);
    
    NSMutableArray *ports = [NSMutableArray new];
    while ((serialPort = IOIteratorNext(serialPortIterator))) {
        CFStringRef ret = IORegistryEntryCreateCFProperty(
            serialPort,
            CFSTR(kIOCalloutDeviceKey),
            kCFAllocatorDefault, 0
        );
        if (CFStringFind(ret, CFSTR("cu.usbserial"), 0).location != kCFNotFound) {
            [ports addObject:(__bridge NSString *)ret];
        }
        CFRelease(ret);
        IOObjectRelease(serialPort);
    }
    
    IOObjectRelease(serialPortIterator);
    
    return ports;
}

- (id)initWithBSDPath:(NSString *)path
{
    if (self = [super init]) {
        _path = path;
    }
    return self;
}

- (void)dealloc
{
    [self close];
}

- (int)openCore:(speed_t)baud
{
    int             fileDescriptor = -1;
    int             handshake;
    struct termios  options;
    
    const char *bsdPath = _path.UTF8String;
    fileDescriptor = open(bsdPath, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fileDescriptor == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error opening serial port %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    if (ioctl(fileDescriptor, TIOCEXCL) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error setting TIOCEXCL on %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    if (fcntl(fileDescriptor, F_SETFL, 0) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error clearing O_NONBLOCK %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    if (tcgetattr(fileDescriptor, &gOriginalTTYAttrs) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error getting tty attributes %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    options = gOriginalTTYAttrs;
    
    cfmakeraw(&options);
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 1;
    
    if (tcsetattr(fileDescriptor, TCSANOW, &options) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error setting tty attributes %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    cfsetspeed(&options, baud);         // Set the baud
    options.c_cflag |= (CS8        |    // Use 8 bit words
                        CCTS_OFLOW |    // CTS flow control of output
                        CRTS_IFLOW);    // RTS flow control of input
    
    speed_t speed = baud; // Set the baud
    if (ioctl(fileDescriptor, IOSSIOSPEED, &speed) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error calling ioctl(..., IOSSIOSPEED, ...) %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    if (ioctl(fileDescriptor, TIOCSDTR) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error asserting DTR %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    if (ioctl(fileDescriptor, TIOCCDTR) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error clearing DTR %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    handshake = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR;
    if (ioctl(fileDescriptor, TIOCMSET, &handshake) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error setting handshake lines %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }

    if (ioctl(fileDescriptor, TIOCMGET, &handshake) == -1) {
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error getting handshake lines %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    unsigned long mics = 1UL;
    
    if (ioctl(fileDescriptor, IOSSDATALAT, &mics) == -1) {
        // set latency to 1 microsecond
        @throw [NSException exceptionWithName:SerialException
                                       reason:
                [NSString stringWithFormat:@"Error setting read latency %s - %s(%d).\n",
                 bsdPath, strerror(errno), errno]
                                     userInfo:nil];
    }
    
    return fileDescriptor;
}

- (void)openWithBaud:(speed_t)baud
{
    int fileDescriptor = -1;
    @try {
        fileDescriptor = [self openCore:baud];
        if (fileDescriptor == -1) {
            @throw [NSException exceptionWithName:SerialException
                                           reason:@"Cannot open serial port (unknown reason)"
                                         userInfo:nil];
        } else {
            handle = [[NSFileHandle alloc] initWithFileDescriptor:fileDescriptor
                                                   closeOnDealloc:NO];
        }
    }
    @catch (NSException *exception) {
        if (fileDescriptor != -1) {
            close(fileDescriptor);
        }
        @throw exception;
    }
}

- (void)sendRawData:(NSData *)data
{
    [handle writeData:data];
}

- (void)sendData:(NSData *)data
{
    NSRange searchRange = NSMakeRange(1, [data length] - 1);
    NSMutableData *paddedData = [data mutableCopy];
    const uint8_t header = PACKET_HEADER;
    NSData *headerData = [NSData dataWithBytes:&header length:1];
    const uint8_t paddedHeader[] = {0xFF, 0x00};
    while (1) {
        NSRange ffRange = [paddedData rangeOfData:headerData options:0 range:searchRange];
        if (ffRange.location == NSNotFound) {
            break;
        }
        [paddedData replaceBytesInRange:ffRange withBytes:paddedHeader length:2];
        searchRange.location = ffRange.location + 2;
        searchRange.length = [paddedData length] - searchRange.location;
    }
    [handle writeData:paddedData];
}

- (NSData *)readDataOfLength:(NSUInteger)length
{
    while (1) {
        NSData *header = [handle readDataOfLength:1];
        if ([header length] == 1 && ((const uint8_t *)[header bytes])[0] == PACKET_HEADER) {
            break;
        }
    }
    while (1) {
        NSData *masterID = [handle readDataOfLength:1];
        if ([masterID length] == 1 && ((const uint8_t *)[masterID bytes])[0] == 0) {
            break;
        }
    }
    const uint8_t packetHead[] = {PACKET_HEADER, 0};
    NSMutableData *ret = [NSMutableData dataWithBytes:packetHead length:sizeof(packetHead)];
    NSUInteger dataLengthToRead = length - 2;
    NSData *paddedFFData = [NSData dataWithBytes:packetHead length:2];
    while (1) {
        NSMutableData *data = [[handle readDataOfLength:dataLengthToRead] mutableCopy];
        NSRange searchRange = NSMakeRange(0, [data length]);
        while (1) {
            NSRange paddedDataRange = [data rangeOfData:paddedFFData options:0 range:searchRange];
            if (paddedDataRange.location == NSNotFound) {
                break;
            }
            paddedDataRange.location += 1;
            paddedDataRange.length = 1;
            [data replaceBytesInRange:paddedDataRange withBytes:NULL length:0];
            searchRange.location = paddedDataRange.location;
            searchRange.length = [data length] - searchRange.location;
        }
        [ret appendData:data];
        dataLengthToRead -= [data length];
        if (dataLengthToRead == 0) {
            break;
        }
    }
    return ret;
}

- (NSData *)readDataOfLengthWithTimeout:(NSUInteger)length
{
//    const uint8_t paddedHeaderRaw[] = {PACKET_HEADER, 0};
//    NSData *paddedData = [NSData dataWithBytes:paddedHeaderRaw length:2];
//    NSMutableData *rxData = [[handle availableData] mutableCopy];
//    while (1) {
//        NSRange ffRange = [rxData rangeOfData:paddedData options:0 range:NSMakeRange(2, [rxData length] - 2)];
//        if (ffRange.location == NSNotFound) {
//            break;
//        }
//        [rxData replaceBytesInRange:ffRange withBytes:paddedHeaderRaw length:1];
//    }
//    return rxData;
    
    while (1) {
        NSData *header = [handle readDataOfLength:1];
        if ([header length] < 1) {
            return nil;
        }
        if (((const uint8_t *)[header bytes])[0] == PACKET_HEADER) {
            break;
        }
    }
    NSData *masterID = [handle readDataOfLength:1];
    if ([masterID length] < 1 || ((const uint8_t *)[masterID bytes])[0] != 0) {
        return nil;
    }
    const uint8_t packetHead[] = {PACKET_HEADER, 0};
    NSMutableData *ret = [NSMutableData dataWithBytes:packetHead length:sizeof(packetHead)];
    NSUInteger dataLengthToRead = length - 2;
    NSData *paddedFFData = [NSData dataWithBytes:packetHead length:2];
    while (1) {
        NSMutableData *data = [[handle readDataOfLength:dataLengthToRead] mutableCopy];
        if ([data length] < dataLengthToRead) {
            return nil;
        }
        NSRange searchRange = NSMakeRange(0, [data length]);
        while (1) {
            NSRange paddedDataRange = [data rangeOfData:paddedFFData options:0 range:searchRange];
            if (paddedDataRange.location == NSNotFound) {
                break;
            }
            paddedDataRange.location += 1;
            paddedDataRange.length = 1;
            [data replaceBytesInRange:paddedDataRange withBytes:NULL length:0];
            searchRange.location = paddedDataRange.location;
            searchRange.length = [data length] - searchRange.location;
        }
        [ret appendData:data];
        dataLengthToRead -= [data length];
        if (dataLengthToRead == 0) {
            break;
        }
    }
    return ret;
}

- (NSData *)readRawDataOfLength:(NSUInteger)length
{
    return [handle readDataOfLength:length];
}

- (void)close
{
    @synchronized(self) {
        if (handle) {
            [[NSNotificationCenter defaultCenter] removeObserver:self];
            [handle closeFile];
            handle = nil;
        }
    }
}

@end
