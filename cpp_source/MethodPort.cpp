#pragma once

#include <JuceHeader.h>
#include <iostream>
#include "JucePlayer.cpp"

struct ClassAddressInfo {
    long addr;
    std::string className;
};

/// Split className and address from string ex. from `JuceMixItem:23243243` to `ClassAddressInfo`
ClassAddressInfo splitClassAddress(const std::string& str) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);

    while (std::getline(tokenStream, token, ':')) {
        tokens.push_back(token);
    }

    ClassAddressInfo res;
    res.className = tokens[0];
    res.addr = std::atol(tokens[1].c_str());
    return res;
}

/// Native init, returns className and address ex. `JuceMixItem:23243243`
std::string nativeInit(const char* className) {
    std::string className_(className);
    std::string addr = std::string();
    if (className_ == "JuceMixItem") {
        addr.append("JuceMixItem:");
        addr.append(std::to_string(reinterpret_cast<long>(new JuceMixItem())));
    }
    else if (className_ == "JuceMixPlayer") {
        addr.append("JuceMixPlayer:");
        addr.append(std::to_string(reinterpret_cast<long>(new JuceMixPlayer())));
    }
    else {
        return "";
    }
    return addr;
}

/// Native method channel
std::string nativeCall(const char* addr, const char* funcName, const char* argsJson) {
    juce::var resultJson;

    if (!funcName) {
        PRINT("No funcName provided");
        resultJson.getDynamicObject()->setProperty("error", "No funcName provided");
        return juce::JSON::toString(resultJson).toStdString();;
    }
    std::string func(funcName);
    juce::var args = juce::JSON::parse(juce::String(argsJson));

    std::string methodNotFound;

    ClassAddressInfo thisObj = splitClassAddress(addr);

    if (thisObj.className == "JuceMixItem") {
        JuceMixItem* obj = reinterpret_cast<JuceMixItem*>(thisObj.addr);
        if (func == "setPath") {
            obj->setPath(args.getProperty("path", ""));
        } else if (func == "destroy") {
            delete obj;
        } else {
            methodNotFound = funcName;
        }
    }
    else if (thisObj.className == "JuceMixPlayer") {
        JuceMixPlayer* obj = reinterpret_cast<JuceMixPlayer*>(thisObj.addr);
        if (func == "play") {
            obj->play();
        } else if (func == "pause") {
            obj->pause();
        } else if (func == "stop") {
            obj->stop();
        } else if (func == "addItem") {
            juce::String info = args.getProperty("item", "");
            ClassAddressInfo item = splitClassAddress(info.toStdString());
            obj->addItem(reinterpret_cast<JuceMixItem*>(item.addr));
        } else if (func == "destroy") {
            delete obj;
        } else {
            methodNotFound = funcName;
        }
    } else {
        jassert(false);
    }

    if (!methodNotFound.empty()) {
        PRINT("Method name not found: '" + juce::String(func) + "'");
        resultJson.getDynamicObject()->setProperty("error", "Method name not found: '" + juce::String(func) + "'");
    }
    return juce::JSON::toString(resultJson).toStdString();
}
