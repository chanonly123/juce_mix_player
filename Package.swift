// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "JuceMixPlayerSwift",
    platforms: [
        .iOS(.v13)
    ],
    products: [
        .library(
            name: "JuceMixPlayerSwift",
            targets: ["JuceMixPlayerSwift"])
    ],
    targets: [
        .target(
            name: "JuceMixPlayerSwift",
            dependencies: ["juce_mix_player_static"],
            path: "dist",
            sources: ["bridge.c"],
            publicHeadersPath: "include",
            linkerSettings: [
                .linkedFramework("AVFoundation"),
                .linkedFramework("AudioToolbox"),
                .linkedFramework("CoreAudioKit"),
                .linkedFramework("CoreAudio"),
                .linkedFramework("Accelerate"),
                .linkedFramework("CoreMIDI"),
                .linkedFramework("Foundation"),
                .linkedFramework("MobileCoreServices"),
                .linkedLibrary("c++")
            ]
        ),
        .binaryTarget(
            name: "juce_mix_player_static",
            path: "dist/juce_lib.xcframework"
        )
    ]
)
