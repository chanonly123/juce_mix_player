import Flutter
import UIKit

@main
@objc class AppDelegate: FlutterAppDelegate {
  override func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?
  ) -> Bool {
    Java_com_rmsl_juce_Native_juceMessageManagerInit()

    // Register iOS PlatformView for video surface
    if let registrar = self.registrar(forPlugin: "gst-video-view") {
      registrar.register(IOSGstVideoViewFactory(), withId: "gst-video-view")
    }

    GeneratedPluginRegistrant.register(with: self)
      
//    let factory = GstVideoViewFactory()
//    registrar(forPlugin: "GstVideoView")?.register(factory, withId: "gst_video_view")
      
    return super.application(application, didFinishLaunchingWithOptions: launchOptions)
  }
}

// Inline iOS PlatformView implementations to avoid Xcode project edits
final class IOSGstVideoView: NSObject, FlutterPlatformView {
  private let container: UIView
  private let playerPtr: UInt64

  init(frame: CGRect, viewIdentifier viewId: Int64, arguments args: Any?) {
    self.container = UIView(frame: frame)
    self.container.backgroundColor = .black
    if let dict = args as? [String: Any], let ptr = dict["playerPtr"] as? NSNumber {
      self.playerPtr = ptr.uint64Value
    } else {
      self.playerPtr = 0
    }
    super.init()

    if playerPtr != 0 {
      let viewPtr = Unmanaged.passUnretained(self.container).toOpaque()
      GstPlayer_setSurfaceHandle(UnsafeMutableRawPointer(bitPattern: UInt(playerPtr)), viewPtr)
    }
  }

  func view() -> UIView { container }

  deinit {
    if playerPtr != 0 {
      GstPlayer_setSurfaceHandle(UnsafeMutableRawPointer(bitPattern: UInt(playerPtr)), nil)
    }
  }
}

final class IOSGstVideoViewFactory: NSObject, FlutterPlatformViewFactory {
  func createArgsCodec() -> FlutterMessageCodec & NSObjectProtocol {
    FlutterStandardMessageCodec.sharedInstance()
  }
  func create(withFrame frame: CGRect, viewIdentifier viewId: Int64, arguments args: Any?) -> FlutterPlatformView {
    IOSGstVideoView(frame: frame, viewIdentifier: viewId, arguments: args)
  }
}

