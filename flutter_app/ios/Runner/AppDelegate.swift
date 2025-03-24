import Flutter
import UIKit

@main
@objc class AppDelegate: FlutterAppDelegate {
  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    GeneratedPluginRegistrant.register(with: self)
    Java_com_rmsl_juce_Native_juceMessageManagerInit()
    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }
}
