#ifdef __cplusplus
extern "C" {
#endif

#import <Foundation/Foundation.h>

NSString* native_init(NSString* className);
NSString* native_call(NSString* addr, NSString* func, NSString* args);

#ifdef __cplusplus
}
#endif
