#import <Foundation/Foundation.h>
#import <termios.h>

extern NSString * const SerialException;

@interface Serial : NSObject

+ (NSArray *)availableDevices;
- (id)initWithBSDPath:(NSString *)path;
- (void)openWithBaud:(speed_t)baud;
- (void)sendRawData:(NSData *)data;
- (void)sendData:(NSData *)data;
- (NSData *)readDataOfLength:(NSUInteger)length;
- (NSData *)readDataOfLengthWithTimeout:(NSUInteger)length;
- (NSData *)readRawDataOfLength:(NSUInteger)length;
- (void)close;

@property (readonly) NSString *path;

@end
