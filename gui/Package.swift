// swift-tools-version: 5.9
import PackageDescription

// dscleaner menu bar app (macOS 13+, SwiftUI MenuBarExtra).
//
// The GUI is a thin shell over the `dscleaner` CLI: it never re-implements any
// cleaning logic, it just runs the binary (e.g. `dscleaner status --json`,
// `dscleaner install-service ...`) and renders the result. This keeps the C++
// core the single source of truth.
let package = Package(
    name: "DscleanerMenuBar",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "DscleanerMenuBar",
            path: "Sources/DscleanerMenuBar"
        )
    ]
)
