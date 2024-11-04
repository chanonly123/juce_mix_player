#include "JucePlayer.cpp"
#include "MethodPort.cpp"
#import <Foundation/Foundation.h>

extern "C" {

NSString* native_init(NSString* className) {
    std::string res = nativeInit([className UTF8String]);
    NSString* copy = [NSString stringWithUTF8String: res.c_str()];
    return copy;
}

NSString* native_call(NSString* addr, NSString* func, NSString* args) {
    std::string res = nativeCall([addr UTF8String], [func UTF8String], [args UTF8String]);
    NSString* copy = [NSString stringWithUTF8String: res.c_str()];
    return copy;
}

}
