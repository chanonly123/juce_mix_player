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

    // Pass CALayer pointer up to native when available
    if playerPtr != 0 {
      let layerPtr = Unmanaged.passUnretained(self.container.layer).toOpaque()
      GstPlayer_setSurfaceHandle(UnsafeMutableRawPointer(bitPattern: UInt(playerPtr)), layerPtr)
    }
  }

  func view() -> UIView {
    return container
  }
}
