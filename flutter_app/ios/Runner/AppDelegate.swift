import Flutter
import UIKit

@main
@objc class AppDelegate: FlutterAppDelegate {
  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    Java_com_rmsl_juce_Native_juceMessageManagerInit()
    GeneratedPluginRegistrant.register(with: self)
      
    let factory = GstVideoViewFactory()
    registrar(forPlugin: "GstVideoView")?.register(factory, withId: "gst_video_view")
      
    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }
}
