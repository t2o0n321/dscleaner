import Foundation
import ServiceManagement

// Controls whether the menu bar app itself launches at login, via the modern
// ServiceManagement API (macOS 13+). This is distinct from the background
// cleaning *service* (a launchd agent managed by the core); this is just the UI.
enum LoginItem {
    static var isEnabled: Bool {
        SMAppService.mainApp.status == .enabled
    }

    static func setEnabled(_ enabled: Bool) throws {
        if enabled {
            try SMAppService.mainApp.register()
        } else {
            try SMAppService.mainApp.unregister()
        }
    }
}
