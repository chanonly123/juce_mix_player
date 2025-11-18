//
//  GstVideoPlayerViewFactory.swift
//  Runner
//
//  Created by Animesh on 18/11/25.
//

import Flutter
import UIKit

final class GstVideoPlayerViewFactory: NSObject, FlutterPlatformViewFactory {
  func createArgsCodec() -> FlutterMessageCodec & NSObjectProtocol {
    return FlutterStandardMessageCodec.sharedInstance()
  }

  func create(withFrame frame: CGRect, viewIdentifier viewId: Int64, arguments args: Any?) -> FlutterPlatformView {
    return GstVideoPlayerView(frame: frame, viewIdentifier: viewId, arguments: args)
  }
}

