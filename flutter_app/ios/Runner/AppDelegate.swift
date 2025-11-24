import Flutter
import UIKit

@main
@objc class AppDelegate: FlutterAppDelegate {
  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    Java_com_rmsl_juce_Native_juceMessageManagerInit()

    // Register iOS PlatformView for video surface using the dedicated Swift file
    if let registrar = self.registrar(forPlugin: "gst-video-view") {
      registrar.register(GstVideoPlayerViewFactory(), withId: "gst-video-view")
    }

    GeneratedPluginRegistrant.register(with: self)

    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }
}
