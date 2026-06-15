import Foundation

// Mirrors the JSON emitted by `dscleaner status --json`. Decoding-only: the GUI
// reads status from the core and never invents its own.
struct CoreStatus: Codable, Equatable {
    struct Service: Codable, Equatable {
        struct Logs: Codable, Equatable {
            var out: String
            var err: String
        }

        var installed: Bool
        var running: Bool
        var notifications: Bool
        var label: String
        var plist: String
        var logs: Logs
    }

    var version: String
    var service: Service
    var watchPaths: [String]

    /// A neutral "unknown" status shown before the first refresh / on error.
    static let unknown = CoreStatus(
        version: "—",
        service: .init(
            installed: false,
            running: false,
            notifications: true,
            label: "com.t2o0n321.dscleaner",
            plist: "",
            logs: .init(out: "", err: "")
        ),
        watchPaths: []
    )
}
