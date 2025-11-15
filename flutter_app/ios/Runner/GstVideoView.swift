import Flutter
import UIKit

private class GstVideoView: UIView {
    
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
    }
    
    func view() -> UIView {
        _view
    }
}
