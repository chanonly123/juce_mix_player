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
    self.container = UIView(frame: frame)
    self.container.backgroundColor = .black
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

  func view() -> UIView {
    return container
  }

  deinit {
    // Clean up the surface handle when the view is deallocated
    if playerPtr != 0 {
      GstPlayer_setSurfaceHandle(UnsafeMutableRawPointer(bitPattern: UInt(playerPtr)), nil)
    }
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
