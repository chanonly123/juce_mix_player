import Flutter
import UIKit
import XCTest

class RunnerTests: XCTestCase {
    
    func testLatencyCalculation() {
        let buf = UnsafeMutablePointer<Float>.allocate(capacity: 20)
        defer { buf.deallocate() }
        for i in 0..<20 {
            buf[i] = 0
        }
        let rec = UnsafeMutablePointer<Float>.allocate(capacity: 20)
        defer { rec.deallocate() }
        for i in 0..<20 {
            rec[i] = 0
        }
        
        buf[5] = 0.5
        buf[7] = 0.5
        
        rec[13] = 0.4
        rec[15] = 0.5
        
        let offset = findTwoTickPattern(buf, 20, 2)
        let offsetFound = findTwoTickPattern(rec, 20, 2)
        
        print("Offset in buf: \(offset)")
        print("Offset in rec: \(offsetFound)")
    }
    
}
