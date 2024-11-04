//
//  juce_player_testApp.swift
//  juce_player_test
//
//  Created by Chandan on 27/09/24.
//

import SwiftUI

@main
struct juce_player_testApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) var delegate

    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}

extension Dictionary {
    var toJsonString: String {
        if let jsonData = try? JSONSerialization.data(withJSONObject: self, options: .fragmentsAllowed) {
            return String(data: jsonData, encoding: .utf8) ?? ""
        }
        return ""
    }
}
