import Foundation
import Combine

// Observable state for the whole app. Owns the CoreClient, refreshes status on a
// timer, and exposes async actions the views call. Everything that touches UI
// state runs on the main actor.
@MainActor
final class AppModel: ObservableObject {
    @Published private(set) var status: CoreStatus = .unknown
    @Published private(set) var lastError: String?
    @Published private(set) var coreAvailable = true
    @Published private(set) var isBusy = false

    private let client = CoreClient()
    private var timer: Timer?

    init() {
        Task { await refresh() }
        // Light periodic refresh so the menu reflects mounts / service changes.
        timer = Timer.scheduledTimer(withTimeInterval: 5, repeats: true) { [weak self] _ in
            Task { await self?.refresh() }
        }
    }

    deinit { timer?.invalidate() }

    func refresh() async {
        coreAvailable = await client.isAvailable
        guard coreAvailable else {
            lastError = CoreClientError.binaryNotFound.errorDescription
            return
        }
        do {
            status = try await client.status()
            lastError = nil
        } catch {
            lastError = error.localizedDescription
        }
    }

    /// Turns the background service on (with the given paths) or off.
    func setServiceEnabled(_ enabled: Bool, paths: [String]) async {
        await perform {
            if enabled {
                try await self.client.installService(paths: paths)
            } else {
                try await self.client.uninstallService()
            }
        }
    }

    /// Re-applies the watch paths by reinstalling the service (only meaningful
    /// while the service is enabled).
    func applyWatchPaths(_ paths: [String]) async {
        await perform { try await self.client.installService(paths: paths) }
    }

    /// One-shot manual clean of a folder the user picked.
    func clean(path: String) async {
        await perform { try await self.client.clean(path: path) }
    }

    private func perform(_ action: @escaping () async throws -> Void) async {
        isBusy = true
        defer { isBusy = false }
        do {
            try await action()
            await refresh()
        } catch {
            lastError = error.localizedDescription
        }
    }
}
