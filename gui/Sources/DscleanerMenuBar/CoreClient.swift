import Foundation

// Runs the `dscleaner` CLI and decodes its output. All cleaning / service logic
// lives in the C++ core; this type is the only place that shells out to it.
enum CoreClientError: LocalizedError {
    case binaryNotFound
    case nonZeroExit(code: Int32, stderr: String)

    var errorDescription: String? {
        switch self {
        case .binaryNotFound:
            return "Could not find the `dscleaner` command. Install it (e.g. `brew install dscleaner`) "
                + "or set DSCLEANER_BIN to its path."
        case let .nonZeroExit(code, stderr):
            return "dscleaner exited with code \(code): \(stderr)"
        }
    }
}

actor CoreClient {
    /// Resolves the CLI path once: an explicit override, a binary bundled next to
    /// the app, then the common Homebrew / system locations.
    private static func resolveBinary() -> URL? {
        let fm = FileManager.default

        if let override = ProcessInfo.processInfo.environment["DSCLEANER_BIN"],
           fm.isExecutableFile(atPath: override) {
            return URL(fileURLWithPath: override)
        }
        if let bundled = Bundle.main.url(forResource: "dscleaner", withExtension: nil),
           fm.isExecutableFile(atPath: bundled.path) {
            return bundled
        }
        for candidate in ["/opt/homebrew/bin/dscleaner", "/usr/local/bin/dscleaner", "/usr/bin/dscleaner"] {
            if fm.isExecutableFile(atPath: candidate) {
                return URL(fileURLWithPath: candidate)
            }
        }
        return nil
    }

    private let binary: URL?

    init() {
        self.binary = Self.resolveBinary()
    }

    var isAvailable: Bool { binary != nil }

    /// Runs `dscleaner <args>` and returns stdout, throwing on failure.
    @discardableResult
    func run(_ args: [String]) throws -> Data {
        guard let binary else { throw CoreClientError.binaryNotFound }

        let process = Process()
        process.executableURL = binary
        process.arguments = args

        let stdout = Pipe()
        let stderr = Pipe()
        process.standardOutput = stdout
        process.standardError = stderr
        try process.run()

        let outData = stdout.fileHandleForReading.readDataToEndOfFile()
        let errData = stderr.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()

        guard process.terminationStatus == 0 else {
            let message = String(data: errData, encoding: .utf8) ?? ""
            throw CoreClientError.nonZeroExit(code: process.terminationStatus, stderr: message)
        }
        return outData
    }

    /// `dscleaner status --json`
    func status() throws -> CoreStatus {
        let data = try run(["status", "--json"])
        return try JSONDecoder().decode(CoreStatus.self, from: data)
    }

    /// `dscleaner install-service <paths...>` (an empty list lets the core pick
    /// its default, e.g. /Volumes).
    func installService(paths: [String]) throws {
        try run(["install-service"] + paths)
    }

    /// `dscleaner uninstall-service`
    func uninstallService() throws {
        try run(["uninstall-service"])
    }

    /// `dscleaner clean <path>` — one-shot manual clean of a chosen folder.
    func clean(path: String) throws {
        try run(["clean", path])
    }
}
