import Flutter
import UIKit
import QuartzCore

private class GstVideoView: UIView {

    // For glimagesink on iOS the backing layer must be a CAEAGLLayer so that
    // GStreamer can render into it.
    override class var layerClass: AnyClass {
        return CAEAGLLayer.self
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func layoutSubviews() {
        super.layoutSubviews()

        // adds a border to visualize the view
        self.layer.borderColor = UIColor.red.cgColor
        self.layer.borderWidth = 1.0
    }
}

class GstVideoViewFactory: NSObject, FlutterPlatformViewFactory {
    
    func create(withFrame frame: CGRect, viewIdentifier viewId: Int64, arguments args: Any?) -> any FlutterPlatformView {
        GstVideoPlatformView(frame: frame)
    }
}

private class GstVideoPlatformView: NSObject, FlutterPlatformView {

    let _view: UIView

    init(frame: CGRect) {
        self._view = GstVideoView(frame: frame)
        super.init()
        // Pass the native UIView* down to the JUCE/GStreamer layer so that
        // GstPlayer can render video into this platform view via GstVideoOverlay.
        GstPlayer_setWindowHandleGlobal(Unmanaged.passUnretained(self._view).toOpaque())
    }

    func view() -> UIView {
        _view
    }
}
