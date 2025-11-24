//
//  GstVideoPlayerView.swift
//  Runner
//
//  Created by Animesh on 18/11/25.
//

import Flutter
import UIKit

final class GstVideoPlayerView: NSObject, FlutterPlatformView {
  private let viewId: Int64
  private let container: UIView
  private let playerPtr: UInt64

  init(frame: CGRect, viewIdentifier viewId: Int64, arguments args: Any?) {
    self.viewId = viewId
    self.container = GstView(frame: frame)
    self.container.backgroundColor = .black
    // Set content scale factor for retina displays
    self.container.contentScaleFactor = UIScreen.main.scale
    
    if let dict = args as? [String: Any], let ptr = dict["playerPtr"] as? NSNumber {
      self.playerPtr = ptr.uint64Value
    } else {
      self.playerPtr = 0
    }
    super.init()

    // Pass UIView pointer to native for GStreamer video overlay
    // GStreamer's glimagesink on iOS works better with UIView than CALayer
    if playerPtr != 0 {
      let viewPtr = Unmanaged.passUnretained(self.container).toOpaque()
      GstPlayer_setSurfaceHandle(UnsafeMutableRawPointer(bitPattern: UInt(playerPtr)), viewPtr)
    }
  }



  // NEW: Required for GStreamer glimagesink to render on iOS
class GstView: UIView {
  override class var layerClass: AnyClass {
      return CAEAGLLayer.self
  }
  
  override init(frame: CGRect) {
      super.init(frame: frame)
      if let layer = self.layer as? CAEAGLLayer {
          layer.isOpaque = true
          layer.drawableProperties = [
              kEAGLDrawablePropertyRetainedBacking: false,
              kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
          ]
      }
  }
  
  required init?(coder: NSCoder) {
      fatalError("init(coder:) has not been implemented")
  }
}


  func view() -> UIView {
    return container
  }
}

final class GstVideoPlayerViewFactory: NSObject, FlutterPlatformViewFactory {
  func createArgsCodec() -> FlutterMessageCodec & NSObjectProtocol {
    return FlutterStandardMessageCodec.sharedInstance()
  }

  func create(withFrame frame: CGRect, viewIdentifier viewId: Int64, arguments args: Any?) -> FlutterPlatformView {
    return GstVideoPlayerView(frame: frame, viewIdentifier: viewId, arguments: args)
  }
}
